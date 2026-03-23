#pragma once

#include "esp_err.h"

/* Brain 临时配网热点参数 */
#define PROVISION_AP_SSID        "MimiClaw-Config"
#define PROVISION_AP_PASSWORD    ""               /* 开放网络 */
#define PROVISION_CONFIG_URL     "http://192.168.4.1/wifi-config"
#define PROVISION_CONNECT_TIMEOUT_MS  20000
#define PROVISION_HTTP_TIMEOUT_MS      5000

/**
 * @brief 获取 WiFi 配置（NVS Bootstrap 主入口）
 *
 * - NVS 有配置 → 填充 ssid/password，返回 ESP_OK
 * - NVS 无配置 → 连接 Brain 临时热点，HTTP 拉取配置，写 NVS，调用
 *               esp_restart()，此分支永不返回
 *
 * 调用前须已完成 nvs_flash_init()、esp_netif_init()、
 * esp_event_loop_create_default()。
 */
esp_err_t wifi_provision_get_config(char *ssid, size_t ssid_len,
                                     char *password, size_t pwd_len);
