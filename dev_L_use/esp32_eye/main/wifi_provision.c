#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "nvs_config.h"
#include "wifi_provision.h"

static const char *TAG = "wifi_provision";

/* ── 临时连接 Brain SoftAP ───────────────────────────────────── */

static EventGroupHandle_t s_prov_event_group;
#define PROV_CONNECTED_BIT BIT0

static void prov_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Connected to provision AP, IP: " IPSTR,
                 IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_prov_event_group, PROV_CONNECTED_BIT);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected from provision AP, will not retry");
    }
}

static esp_err_t connect_to_provision_ap(void)
{
    s_prov_event_group = xEventGroupCreate();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t h_wifi, h_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, prov_event_handler, NULL, &h_wifi));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, prov_event_handler, NULL, &h_ip));

    wifi_config_t wifi_cfg = {};
    strncpy((char *)wifi_cfg.sta.ssid, PROVISION_AP_SSID,
            sizeof(wifi_cfg.sta.ssid) - 1);
    /* 开放热点，password 留空 */

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to provision AP: %s ...", PROVISION_AP_SSID);

    EventBits_t bits = xEventGroupWaitBits(
        s_prov_event_group, PROV_CONNECTED_BIT,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(PROVISION_CONNECT_TIMEOUT_MS));

    if (!(bits & PROV_CONNECTED_BIT)) {
        ESP_LOGE(TAG, "Provision AP connect timeout (%d ms)",
                 PROVISION_CONNECT_TIMEOUT_MS);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

/* ── HTTP 拉取配置并解析 ─────────────────────────────────────── */

static esp_err_t fetch_wifi_config(char *ssid, size_t ssid_len,
                                    char *password, size_t pwd_len)
{
    static char resp_buf[256];
    int resp_len = 0;

    esp_http_client_config_t http_cfg = {
        .url        = PROVISION_CONFIG_URL,
        .timeout_ms = PROVISION_HTTP_TIMEOUT_MS,
        .method     = HTTP_METHOD_GET,
    };
    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);

    esp_err_t ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: 0x%x", ret);
        goto cleanup;
    }

    int content_len = esp_http_client_fetch_headers(client);
    if (content_len <= 0 || content_len >= (int)sizeof(resp_buf)) {
        ESP_LOGE(TAG, "Invalid content length: %d", content_len);
        ret = ESP_FAIL;
        goto cleanup;
    }

    resp_len = esp_http_client_read(client, resp_buf, content_len);
    if (resp_len <= 0) {
        ESP_LOGE(TAG, "HTTP read failed");
        ret = ESP_FAIL;
        goto cleanup;
    }
    resp_buf[resp_len] = '\0';
    ESP_LOGI(TAG, "Config response: %s", resp_buf);

    /* 解析 JSON: {"ssid": "...", "password": "..."} */
    cJSON *json = cJSON_Parse(resp_buf);
    if (!json) {
        ESP_LOGE(TAG, "JSON parse failed");
        ret = ESP_FAIL;
        goto cleanup;
    }

    cJSON *j_ssid = cJSON_GetObjectItem(json, "ssid");
    cJSON *j_pwd  = cJSON_GetObjectItem(json, "password");

    if (!cJSON_IsString(j_ssid) || !cJSON_IsString(j_pwd)) {
        ESP_LOGE(TAG, "JSON missing ssid or password field");
        cJSON_Delete(json);
        ret = ESP_FAIL;
        goto cleanup;
    }

    strncpy(ssid,     j_ssid->valuestring, ssid_len - 1);
    strncpy(password, j_pwd->valuestring,  pwd_len  - 1);
    ssid[ssid_len - 1]    = '\0';
    password[pwd_len - 1] = '\0';

    cJSON_Delete(json);
    ret = ESP_OK;

cleanup:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ret;
}

/* ── 公共接口 ────────────────────────────────────────────────── */

esp_err_t wifi_provision_get_config(char *ssid, size_t ssid_len,
                                     char *password, size_t pwd_len)
{
    /* 1. 优先从 NVS 读取已有配置 */
    esp_err_t ret = nvs_config_load(ssid, ssid_len, password, pwd_len);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Using stored config: ssid=%s", ssid);
        return ESP_OK;
    }

    /* 2. NVS 无配置，进入配网流程 */
    ESP_LOGI(TAG, "No stored config, starting provision flow...");

    ret = connect_to_provision_ap();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cannot reach provision AP, system halted");
        /* 无法配网则无限等待，避免错误烧录造成反复重启刷屏 */
        while (1) { vTaskDelay(pdMS_TO_TICKS(5000)); }
    }

    char new_ssid[33]   = {};
    char new_pwd[65]    = {};
    ret = fetch_wifi_config(new_ssid, sizeof(new_ssid),
                            new_pwd,  sizeof(new_pwd));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to fetch config from Brain");
        while (1) { vTaskDelay(pdMS_TO_TICKS(5000)); }
    }

    /* 3. 写入 NVS */
    ret = nvs_config_save(new_ssid, new_pwd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save config to NVS");
        while (1) { vTaskDelay(pdMS_TO_TICKS(5000)); }
    }

    /* 4. 重启，下次从 NVS 正常启动 */
    ESP_LOGI(TAG, "Config saved, restarting...");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();

    return ESP_OK; /* unreachable */
}
