#include "agent_loop.h"
#include "tool_arm.h"
#include "esp_log.h"
#include <string.h>

// llm_client 和 telegram_bot 待实现，暂用 stub
static const char *TAG = "AGENT_LOOP";

void agent_loop_handle(int64_t chat_id, const char *user_msg)
{
    ESP_LOGI(TAG, "agent_loop_handle: chat_id=%lld msg=%s", chat_id, user_msg);
    // TODO: 实现 llm_client 和 telegram_bot 后补全此函数
}
