#include <stdio.h>
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "http_server.h"

static const char *TAG = "http_server";

/* ── /capture ────────────────────────────────────────────── */

static esp_err_t capture_handler(httpd_req_t *req)
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Capture failed");
        return ESP_FAIL;
    }

    if (fb->format != PIXFORMAT_JPEG) {
        /* 不应发生，camera_init 已固定 JPEG 模式，防御性检查 */
        ESP_LOGE(TAG, "Unexpected pixel format: %d", fb->format);
        esp_camera_fb_return(fb);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Wrong format");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

    size_t len = fb->len;
    esp_err_t ret = httpd_resp_send(req, (const char *)fb->buf, len);
    esp_camera_fb_return(fb);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "GET /capture -> %zu bytes", len);
    }
    return ret;
}

static const httpd_uri_t uri_capture = {
    .uri     = "/capture",
    .method  = HTTP_GET,
    .handler = capture_handler,
};

/* ── /status ─────────────────────────────────────────────── */

static esp_err_t status_handler(httpd_req_t *req)
{
    char buf[128];
    uint32_t uptime_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    uint32_t free_heap = esp_get_free_heap_size();

    snprintf(buf, sizeof(buf),
             "{\"camera\":\"ready\",\"resolution\":\"640x480\","
             "\"free_heap\":%lu,\"uptime_s\":%lu}",
             (unsigned long)free_heap,
             (unsigned long)uptime_s);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

static const httpd_uri_t uri_status = {
    .uri     = "/status",
    .method  = HTTP_GET,
    .handler = status_handler,
};

/* ── 启动 ─────────────────────────────────────────────────── */

esp_err_t http_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_handle_t server = NULL;
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: 0x%x", ret);
        return ret;
    }

    httpd_register_uri_handler(server, &uri_capture);
    httpd_register_uri_handler(server, &uri_status);

    ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
    ESP_LOGI(TAG, "  GET /capture  -> JPEG");
    ESP_LOGI(TAG, "  GET /status   -> JSON");
    return ESP_OK;
}
