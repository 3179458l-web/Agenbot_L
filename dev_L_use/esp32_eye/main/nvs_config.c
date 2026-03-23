#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "nvs_config.h"

static const char *TAG        = "nvs_config";
static const char *NAMESPACE  = "eye_cfg";
static const char *KEY_SSID   = "ssid";
static const char *KEY_PWD    = "password";

esp_err_t nvs_config_load(char *ssid, size_t ssid_len,
                           char *password, size_t pwd_len)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        /* namespace 不存在 = 尚未配过网 */
        return ESP_ERR_NVS_NOT_FOUND;
    }

    ret = nvs_get_str(handle, KEY_SSID, ssid, &ssid_len);
    if (ret != ESP_OK) {
        nvs_close(handle);
        return ret;
    }

    ret = nvs_get_str(handle, KEY_PWD, password, &pwd_len);
    nvs_close(handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Loaded config: ssid=%s", ssid);
    }
    return ret;
}

esp_err_t nvs_config_save(const char *ssid, const char *password)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: 0x%x", ret);
        return ret;
    }

    ret = nvs_set_str(handle, KEY_SSID, ssid);
    if (ret != ESP_OK) goto done;

    ret = nvs_set_str(handle, KEY_PWD, password);
    if (ret != ESP_OK) goto done;

    ret = nvs_commit(handle);

done:
    nvs_close(handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Config saved: ssid=%s", ssid);
    } else {
        ESP_LOGE(TAG, "Config save failed: 0x%x", ret);
    }
    return ret;
}
