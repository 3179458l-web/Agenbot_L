#pragma once

#include "esp_err.h"

/**
 * @brief 启动 HTTP Server，注册 /capture 和 /status 路由
 *
 * /capture  GET  返回 JPEG 图片（image/jpeg）
 * /status   GET  返回摄像头状态 JSON
 */
esp_err_t http_server_start(void);
