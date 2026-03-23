"""
cli.py —— Agent 命令行交互界面

用法：
  python cli.py              # 正常模式（连接硬件）
  python cli.py --offline    # 离线模式（无硬件，工具返回模拟结果）

命令：
  直接输入自然语言指令
  /reset   清空对话历史
  /quit    退出
"""

import argparse
import os
import sys

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--offline", action="store_true", help="离线模式，不连接硬件")
    args = parser.parse_args()

    if not os.environ.get("ANTHROPIC_API_KEY"):
        print("错误：请先设置环境变量 ANTHROPIC_API_KEY")
        sys.exit(1)

    if args.offline:
        _patch_offline_mode()

    from agent import Agent
    agent = Agent()

    print("=" * 50)
    print("  MimiClaw Agent 已启动")
    print("  输入自然语言指令控制机械臂")
    print("  /reset 清空对话  /quit 退出")
    if args.offline:
        print("  [离线模式] 工具调用将返回模拟结果")
    print("=" * 50)

    while True:
        try:
            user_input = input("\n你: ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\n退出")
            break

        if not user_input:
            continue
        if user_input == "/quit":
            break
        if user_input == "/reset":
            agent.reset()
            print("[已清空对话历史]")
            continue

        print("MimiClaw: 思考中...")
        reply = agent.chat(user_input)
        print(f"MimiClaw: {reply}")


def _patch_offline_mode():
    """离线模式：mock 掉所有硬件调用"""
    import tools

    def mock_execute(tool_name, args):
        mock_responses = {
            "vision_detect": {
                "ok": True, "count": 2,
                "objects": [
                    {"label": "红色方块", "bbox": [100, 100, 200, 200],
                     "center": [150, 150], "world": [120.0, 30.0, 0.0], "confidence": "high"},
                    {"label": "蓝色圆柱", "bbox": [300, 150, 380, 260],
                     "center": [340, 205], "world": [180.0, -50.0, 0.0], "confidence": "medium"},
                ]
            },
            "arm_move_cartesian": {"ok": True, "position": {"x": args.get("x"), "y": args.get("y"), "z": args.get("z")}},
            "arm_move_joint":     {"ok": True, "joint": args.get("joint"), "angle": args.get("angle")},
            "arm_grab":           {"ok": True, "force": args.get("force", 60), "gripper_angle": 30.0},
            "arm_release":        {"ok": True},
            "arm_home":           {"ok": True, "message": "已回到 home 姿态"},
        }
        result = mock_responses.get(tool_name, {"ok": False, "error": f"未知工具: {tool_name}"})
        return result

    tools.execute_tool = mock_execute


if __name__ == "__main__":
    main()
