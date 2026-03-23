#ifndef AGENT_LOOP_H
#define AGENT_LOOP_H

/**
 * agent_loop —— ReAct 循环主控
 *
 * 流程：
 *   用户消息
 *     → llm_client_chat()          [LLM 推理]
 *     → 若返回 TOOL_USE
 *         → tool_arm_execute()     [执行动作]
 *         → llm_client_chat()      [把结果喂回 LLM]
 *         → 循环直到返回 TEXT
 *     → telegram_bot_send()        [回复用户]
 *
 * 最大 tool 调用轮次：5（防止死循环）
 */

#include <stdint.h>

/**
 * @brief 处理一条用户消息（阻塞直到得到最终文字回复）
 * @param chat_id   Telegram chat id，用于回复
 * @param user_msg  用户消息文本
 */
void agent_loop_handle(int64_t chat_id, const char *user_msg);

#endif // AGENT_LOOP_H
