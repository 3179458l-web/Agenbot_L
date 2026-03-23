#pragma once

#include "esp_err.h"
#include "esp_netif.h"

/**
 * @brief 初始化 WiFi STA，连接指定热点并通过 DHCP 获取 IP
 *
 * 阻塞直到获取到 IP 或超时（30秒）。
 * 成功返回 ESP_OK，失败返回对应错误码。
 */
esp_err_t wifi_sta_init(const char *ssid, const char *password);

/**
 * @brief 获取当前 STA 网络接口
 */
esp_netif_t *wifi_sta_get_netif(void);

/**
 * @brief 获取当前 STA IP 信息
 *
 * 成功返回 ESP_OK，并将结果写入 out_ip_info。
 */
esp_err_t wifi_sta_get_ip_info(esp_netif_ip_info_t *out_ip_info);
