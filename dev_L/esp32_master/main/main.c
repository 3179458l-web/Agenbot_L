#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "wifi_manager.h"
// #include "telegram_bot.h"   // 待实现
#include "uart_bridge.h"
// #include "agent_loop.h"     // 待实现

static const char *TAG = "MAIN";

/* TODO: 实现 telegram_bot.c 后取消注释
static void on_telegram_message(int64_t chat_id, const char *text)
{
    ESP_LOGI(TAG, "Received from %lld: %s", chat_id, text);
    agent_loop_handle(chat_id, text);
}
*/

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 Master (MimiClaw Agent) starting...");

    /* NVS 初始化（WiFi 依赖） */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* 1. WiFi 连接 */
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_LOGI(TAG, "WiFi connected");

    /* 2. 主从串口桥初始化 */
    ESP_ERROR_CHECK(uart_bridge_init());
    ESP_LOGI(TAG, "UART bridge ready");

    /* 3. Telegram Bot 初始化 + 启动轮询（待实现） */
    // telegram_bot_init(on_telegram_message);
    // telegram_bot_start();
    ESP_LOGI(TAG, "TODO: Telegram bot not yet implemented");

    ESP_LOGI(TAG, "System ready. Send a message to the bot.");
}
