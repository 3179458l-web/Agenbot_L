#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "wifi_sta.h"

#define WIFI_CONNECT_TIMEOUT_MS  30000

static const char *TAG = "wifi_sta";
static esp_netif_t *s_sta_netif = NULL;

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static void event_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected, reconnecting...");
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "Connected, static IP: 192.168.4.2");
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_sta_init(const char *ssid, const char *password)
{
    s_wifi_event_group = xEventGroupCreate();

    s_sta_netif = esp_netif_create_default_wifi_sta();

    /* 静态 IP：192.168.4.2，避免 DHCP 分配不确定性 */
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(s_sta_netif));
    esp_netif_ip_info_t ip_info = {0};
    esp_netif_str_to_ip4("192.168.4.2",   &ip_info.ip);
    esp_netif_str_to_ip4("192.168.4.1",   &ip_info.gw);
    esp_netif_str_to_ip4("255.255.255.0", &ip_info.netmask);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(s_sta_netif, &ip_info));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));

    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid,     ssid,     sizeof(wifi_config.sta.ssid)     - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to %s ...", ssid);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected, static IP: 192.168.4.2");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Connection timeout after %d ms", WIFI_CONNECT_TIMEOUT_MS);
    return ESP_ERR_TIMEOUT;
}

esp_netif_t *wifi_sta_get_netif(void)
{
    return s_sta_netif;
}

esp_err_t wifi_sta_get_ip_info(esp_netif_ip_info_t *out_ip_info)
{
    if (s_sta_netif == NULL || out_ip_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_netif_get_ip_info(s_sta_netif, out_ip_info);
}
