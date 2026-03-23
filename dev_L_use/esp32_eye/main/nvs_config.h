#pragma once

#include "esp_err.h"

/**
 * @brief 从 NVS 读取 WiFi 配置
 *
 * 成功返回 ESP_OK，并填充 ssid/password。
 * 未找到配置返回 ESP_ERR_NVS_NOT_FOUND。
 */
esp_err_t nvs_config_load(char *ssid, size_t ssid_len,
                           char *password, size_t pwd_len);

/**
 * @brief 将 WiFi 配置写入 NVS
 *
 * 成功返回 ESP_OK。
 */
esp_err_t nvs_config_save(const char *ssid, const char *password);
