#ifndef LLM_CLIENT_H
#define LLM_CLIENT_H

/**
 * llm_client —— LLM API 调用（Claude / OpenAI 兼容接口）
 *
 * 负责：构造 HTTP 请求、解析响应、提取 tool_use 或 text
 */

#include <stdint.h>
#include "esp_err.h"

/** tool_use 调用结果 */
typedef struct {
    char tool_name[32];     // e.g. "arm_move"
    char args_json[256];    // tool 参数的原始 JSON 字符串
} llm_tool_call_t;

/** LLM 响应类型 */
typedef enum {
    LLM_RESP_TEXT,      // 纯文字回复
    LLM_RESP_TOOL_USE,  // 要求调用 tool
    LLM_RESP_ERROR,     // 请求失败
} llm_resp_type_t;

/** LLM 响应 */
typedef struct {
    llm_resp_type_t type;
    char text[512];             // type == TEXT 时有效
    llm_tool_call_t tool_call;  // type == TOOL_USE 时有效
} llm_response_t;

/**
 * @brief 向 LLM 发送一轮对话（含历史 + 可用 tools schema）
 * @param user_msg   用户输入的文本
 * @param tool_result_json  上一轮 tool 执行结果（首次传 NULL）
 * @param out        输出响应（调用方负责 out 的生命周期）
 */
esp_err_t llm_client_chat(const char *user_msg,
                           const char *tool_result_json,
                           llm_response_t *out);

#endif // LLM_CLIENT_H
