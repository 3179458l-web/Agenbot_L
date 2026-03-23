#include "uart_bridge.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UART_BRIDGE";

#define BRIDGE_UART_NUM   UART_NUM_1
#define BRIDGE_TX_PIN     43
#define BRIDGE_RX_PIN     44
#define BRIDGE_BAUD_RATE  115200
#define BRIDGE_BUF_SIZE   512
#define BRIDGE_TIMEOUT_MS 3000

esp_err_t uart_bridge_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = BRIDGE_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(BRIDGE_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(BRIDGE_UART_NUM,
                                 BRIDGE_TX_PIN, BRIDGE_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(BRIDGE_UART_NUM,
                                        BRIDGE_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_LOGI(TAG, "UART bridge init OK (TX=%d RX=%d %dbaud)",
             BRIDGE_TX_PIN, BRIDGE_RX_PIN, BRIDGE_BAUD_RATE);
    return ESP_OK;
}

esp_err_t uart_bridge_send(const char *cmd_json, uart_bridge_resp_t *resp)
{
    /* 清空接收缓冲 */
    uart_flush_input(BRIDGE_UART_NUM);

    /* 发送指令（末尾加换行，从控按行解析） */
    char tx_buf[BRIDGE_BUF_SIZE];
    int len = snprintf(tx_buf, sizeof(tx_buf), "%s\n", cmd_json);
    uart_write_bytes(BRIDGE_UART_NUM, tx_buf, len);
    ESP_LOGD(TAG, "TX: %s", cmd_json);

    /* 等待响应（逐字节读，直到遇到换行或超时） */
    char rx_buf[BRIDGE_BUF_SIZE];
    int rx_pos = 0;
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(BRIDGE_TIMEOUT_MS);

    while (xTaskGetTickCount() < deadline && rx_pos < (int)sizeof(rx_buf) - 1) {
        uint8_t byte;
        int read = uart_read_bytes(BRIDGE_UART_NUM, &byte, 1, pdMS_TO_TICKS(50));
        if (read > 0) {
            if (byte == '\n') break;
            rx_buf[rx_pos++] = (char)byte;
        }
    }
    rx_buf[rx_pos] = '\0';

    if (rx_pos == 0) {
        ESP_LOGE(TAG, "Timeout waiting for slave response");
        resp->ok = false;
        snprintf(resp->payload, sizeof(resp->payload),
                 "{\"ok\":false,\"error\":\"slave timeout\"}");
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGD(TAG, "RX: %s", rx_buf);
    strncpy(resp->payload, rx_buf, sizeof(resp->payload) - 1);

    /* 简单判断 ok 字段（避免引入完整 JSON 解析） */
    resp->ok = (strstr(rx_buf, "\"ok\":true") != NULL);
    return ESP_OK;
}
