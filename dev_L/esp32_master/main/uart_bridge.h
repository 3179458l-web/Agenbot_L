#ifndef UART_BRIDGE_H
#define UART_BRIDGE_H

/**
 * uart_bridge —— 主从串口通信桥
 *
 * 负责：向 ESP32 #2 发送 JSON 指令，等待响应，超时处理
 * 协议格式与从控固件完全对齐（不可改）：
 *   发送：{"cmd":"move","joint":0,"angle":90,"speed":50}\n
 *   接收：{"ok":true,"joint":0,"angle":90}\n
 */

#include <stdbool.h>
#include "esp_err.h"

/** 从控响应 */
typedef struct {
    bool ok;
    char payload[128];  // 响应的原始 JSON（用于传回 LLM）
} uart_bridge_resp_t;

/**
 * @brief 初始化串口（GPIO43 TX, GPIO44 RX, 115200 baud）
 */
esp_err_t uart_bridge_init(void);

/**
 * @brief 发送 JSON 指令到从控，等待响应（超时 3s）
 * @param cmd_json  完整的 JSON 字符串，不含换行
 * @param resp      输出响应
 */
esp_err_t uart_bridge_send(const char *cmd_json, uart_bridge_resp_t *resp);

#endif // UART_BRIDGE_H
