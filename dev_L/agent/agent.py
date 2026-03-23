"""
agent.py —— ReAct 循环主控

流程：
  用户输入
    → Claude（带 tools）
    → 若返回 tool_use → 执行工具 → 结果喂回 Claude → 循环
    → 直到 Claude 返回纯文字 → 输出给用户

对话历史在同一 session 内保持（多轮），支持连续指令。
最大 tool 调用轮次：8（防止死循环）
"""

import os
import json
import anthropic
from tools import TOOL_SCHEMAS, execute_tool

MAX_TOOL_ROUNDS = 8

SYSTEM_PROMPT = """你是 MimiClaw，一个控制 6 自由度机械臂的 AI 助手。

你可以使用以下工具控制机械臂和摄像头：
- vision_detect: 识别桌面上的物体及其坐标
- arm_move_cartesian: 移动末端到指定 XYZ 坐标（mm）
- arm_move_joint: 控制单个关节角度
- arm_grab: 夹爪抓取
- arm_release: 夹爪释放
- arm_home: 机械臂回零

执行抓取任务的标准流程：
1. 先用 vision_detect 识别目标位置
2. arm_home 回零（避免碰撞）
3. arm_move_cartesian 移到目标上方（Z 多加 50mm）
4. arm_move_cartesian 下降到抓取高度
5. arm_grab 抓取
6. arm_move_cartesian 抬起
7. 移动到目标放置位置，arm_release 释放

遇到错误时，分析原因并告知用户，不要重复失败的动作。
用中文回复用户。"""


class Agent:
    def __init__(self):
        self._client = anthropic.Anthropic()
        self._history: list[dict] = []

    def chat(self, user_message: str) -> str:
        """
        处理一条用户消息，执行必要的工具调用，返回最终文字回复。
        """
        self._history.append({"role": "user", "content": user_message})

        for round_idx in range(MAX_TOOL_ROUNDS):
            resp = self._client.messages.create(
                model="claude-sonnet-4-6",
                max_tokens=4096,
                system=SYSTEM_PROMPT,
                tools=TOOL_SCHEMAS,
                messages=self._history,
            )

            # —— 纯文字回复，结束 ——
            if resp.stop_reason == "end_turn":
                text = self._extract_text(resp)
                self._history.append({"role": "assistant", "content": resp.content})
                return text

            # —— tool_use ——
            if resp.stop_reason == "tool_use":
                self._history.append({"role": "assistant", "content": resp.content})

                # 执行所有 tool_use 块（可能有多个）
                tool_results = []
                for block in resp.content:
                    if block.type != "tool_use":
                        continue

                    print(f"  [工具] {block.name}({json.dumps(block.input, ensure_ascii=False)})")
                    result = execute_tool(block.name, block.input)
                    print(f"  [结果] {json.dumps(result, ensure_ascii=False)}")

                    tool_results.append({
                        "type": "tool_result",
                        "tool_use_id": block.id,
                        "content": json.dumps(result, ensure_ascii=False),
                    })

                self._history.append({"role": "user", "content": tool_results})
                continue

            # 其他 stop_reason（max_tokens 等）
            break

        return "（已达到最大工具调用轮次，任务可能未完成）"

    def reset(self):
        """清空对话历史，开始新会话"""
        self._history = []

    @staticmethod
    def _extract_text(resp) -> str:
        parts = [b.text for b in resp.content if hasattr(b, "text")]
        return "\n".join(parts) if parts else ""
