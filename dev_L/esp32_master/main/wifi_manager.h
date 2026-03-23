#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

/**
 * wifi_manager —— WiFi 连接管理
 *
 * 负责：STA 模式连接、断线重连、IP 获取事件通知
 */

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief 初始化 WiFi，连接到 secrets.h 中配置的热点
 *        阻塞直到获得 IP 或超时（30s）
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief 返回当前是否已连接并持有 IP
 */
bool wifi_manager_is_connected(void);

#endif // WIFI_MANAGER_H
