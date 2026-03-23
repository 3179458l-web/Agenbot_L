idf_component_register(
    SRCS
        "main.c"
        "wifi_manager.c"
        # "telegram_bot.c"   # 待实现
        # "llm_client.c"     # 待实现
        "uart_bridge.c"
        "tool_arm.c"
        "agent_loop.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        esp_http_client
        json
        driver
)
