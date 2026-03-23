#pragma once

#include "esp_err.h"

/**
 * @brief 初始化 OV5640（AI-Thinker ESP32-CAM 板载）
 *
 * 固定参数：VGA 640×480，JPEG format，quality=12。
 * 成功返回 ESP_OK，失败返回对应错误码。
 */
esp_err_t camera_init(void);
