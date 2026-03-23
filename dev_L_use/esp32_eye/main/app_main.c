#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "mdns.h"
#include "wifi_provision.h"
#include "nvs_config.h"
#include "wifi_sta.h"
#include "camera_init.h"
#include "http_server.h"

static const char *TAG = "app_main";

void app_main(void)
{
    /* NVS：WiFi 驱动 + 配置存储依赖 */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "=== MimiClaw Eye booting ===");

    /* ⚠️  临时调试：手动写入 NVS，跳过配网流程，调试完删除 */
    nvs_config_save("AgentBot_AP", "agentbot123");
    /* ⚠️  END 临时调试 */

    /* 网络基础初始化（provision 和 wifi_sta 均依赖） */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 1. 获取 WiFi 配置（NVS Bootstrap：无配置则自动配网后重启） */
    ESP_LOGI(TAG, "[1/5] WiFi provision check...");
    char ssid[33]     = {};
    char password[65] = {};
    /* 若 NVS 无配置，此函数内部完成配网后调用 esp_restart()，不会返回 */
    ESP_ERROR_CHECK(wifi_provision_get_config(ssid, sizeof(ssid),
                                              password, sizeof(password)));

    /* 2. 连接家庭 WiFi */
    ESP_LOGI(TAG, "[2/5] WiFi STA init (ssid=%s)...", ssid);
    ret = wifi_sta_init(ssid, password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi failed (0x%x), restarting in 3s...", ret);
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    }

    /* 3. mDNS */
    ESP_LOGI(TAG, "[3/5] mDNS init...");
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set("mimiclaw-eye"));
    mdns_instance_name_set("MimiClaw Eye Camera");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS: http://mimiclaw-eye.local/capture");

    /* 4. Camera */
    ESP_LOGI(TAG, "[4/5] Camera init...");
    ret = camera_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera failed (0x%x), restarting in 3s...", ret);
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    }

    /* 5. HTTP Server */
    ESP_LOGI(TAG, "[5/5] HTTP server start...");
    ret = http_server_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server failed (0x%x), restarting in 3s...", ret);
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    }

    ESP_LOGI(TAG, "=== MimiClaw Eye ready ===");

    esp_netif_ip_info_t ip_info = {0};
    if (wifi_sta_get_ip_info(&ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "    http://" IPSTR "/capture", IP2STR(&ip_info.ip));
        ESP_LOGI(TAG, "    http://" IPSTR "/status",  IP2STR(&ip_info.ip));
    }
}
