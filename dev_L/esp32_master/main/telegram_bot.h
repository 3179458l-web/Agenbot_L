#ifndef TELEGRAM_BOT_H
#define TELEGRAM_BOT_H

/**
 * telegram_bot —— Telegram Bot 长轮询收发
 *
 * 负责：getUpdates 轮询、消息解析、sendMessage 回复
 * 不包含业务逻辑，收到消息后调用注册的回调
 */

#include <stdint.h>

/** 收到消息时的回调类型。chat_id 用于回复，text 是用户消息 */
typedef void (*tg_message_cb_t)(int64_t chat_id, const char *text);

/**
 * @brief 初始化 Telegram Bot，注册消息回调
 * @param cb  收到新消息时调用的回调函数
 */
void telegram_bot_init(tg_message_cb_t cb);

/**
 * @brief 启动轮询任务（FreeRTOS task）
 *        每 1s 调用一次 getUpdates，有新消息则触发回调
 */
void telegram_bot_start(void);

/**
 * @brief 向指定 chat_id 发送文本消息
 */
esp_err_t telegram_bot_send(int64_t chat_id, const char *text);

#endif // TELEGRAM_BOT_H
