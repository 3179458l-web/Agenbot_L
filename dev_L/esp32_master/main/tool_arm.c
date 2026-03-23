#include "tool_arm.h"
#include "uart_bridge.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "TOOL_ARM";

/* ------------------------------------------------------------------ */
/* Tool schema（传给 LLM 的工具定义，Claude tool_use 格式）             */
/* ------------------------------------------------------------------ */
static const char *TOOL_SCHEMA = "["
    "{"
        "\"name\":\"arm_move_joint\","
        "\"description\":\"控制机械臂单个关节转到指定角度\","
        "\"input_schema\":{"
            "\"type\":\"object\","
            "\"properties\":{"
                "\"joint\":{\"type\":\"integer\",\"description\":\"关节编号 0-5\"},"
                "\"angle\":{\"type\":\"number\",\"description\":\"目标角度（度）\"},"
                "\"speed\":{\"type\":\"integer\",\"description\":\"速度 1-100，默认50\"}"
            "},"
            "\"required\":[\"joint\",\"angle\"]"
        "}"
    "},"
    "{"
        "\"name\":\"arm_move_cartesian\","
        "\"description\":\"将机械臂末端移动到笛卡尔坐标（毫米），原点为机械臂基座\","
        "\"input_schema\":{"
            "\"type\":\"object\","
            "\"properties\":{"
                "\"x\":{\"type\":\"number\",\"description\":\"前后方向 mm\"},"
                "\"y\":{\"type\":\"number\",\"description\":\"左右方向 mm\"},"
                "\"z\":{\"type\":\"number\",\"description\":\"上下方向 mm\"}"
            "},"
            "\"required\":[\"x\",\"y\",\"z\"]"
        "}"
    "},"
    "{"
        "\"name\":\"arm_grab\","
        "\"description\":\"夹爪抓取\","
        "\"input_schema\":{"
            "\"type\":\"object\","
            "\"properties\":{"
                "\"force\":{\"type\":\"integer\",\"description\":\"抓取力度 0-100\"}"
            "},"
            "\"required\":[]"
        "}"
    "},"
    "{"
        "\"name\":\"arm_release\","
        "\"description\":\"夹爪释放\","
        "\"input_schema\":{\"type\":\"object\",\"properties\":{}}"
    "},"
    "{"
        "\"name\":\"arm_home\","
        "\"description\":\"机械臂回到 home 姿态（所有关节归零）\","
        "\"input_schema\":{\"type\":\"object\",\"properties\":{}}"
    "},"
    "{"
        "\"name\":\"sensor_read\","
        "\"description\":\"读取传感器数据\","
        "\"input_schema\":{"
            "\"type\":\"object\","
            "\"properties\":{"
                "\"type\":{\"type\":\"string\","
                           "\"enum\":[\"imu\",\"current\",\"pressure\"],"
                           "\"description\":\"传感器类型\"}"
            "},"
            "\"required\":[\"type\"]"
        "}"
    "}"
"]";

/* ------------------------------------------------------------------ */
/* 各 tool 执行函数                                                      */
/* ------------------------------------------------------------------ */

static esp_err_t exec_move_joint(cJSON *args, char *result, size_t result_size)
{
    int joint   = cJSON_GetObjectItem(args, "joint") ?
                  cJSON_GetObjectItem(args, "joint")->valueint : -1;
    double angle = cJSON_GetObjectItem(args, "angle") ?
                   cJSON_GetObjectItem(args, "angle")->valuedouble : -1;
    int speed   = cJSON_GetObjectItem(args, "speed") ?
                  cJSON_GetObjectItem(args, "speed")->valueint : 50;

    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "{\"cmd\":\"move\",\"joint\":%d,\"angle\":%.1f,\"speed\":%d}",
             joint, angle, speed);

    uart_bridge_resp_t resp;
    uart_bridge_send(cmd, &resp);
    snprintf(result, result_size, "%s", resp.payload);
    return resp.ok ? ESP_OK : ESP_FAIL;
}

static esp_err_t exec_move_cartesian(cJSON *args, char *result, size_t result_size)
{
    /* 笛卡尔移动由从控的运动学层处理，封装为 move_xyz 指令 */
    double x = cJSON_GetObjectItem(args, "x")->valuedouble;
    double y = cJSON_GetObjectItem(args, "y")->valuedouble;
    double z = cJSON_GetObjectItem(args, "z")->valuedouble;

    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "{\"cmd\":\"move_xyz\",\"x\":%.1f,\"y\":%.1f,\"z\":%.1f}",
             x, y, z);

    uart_bridge_resp_t resp;
    uart_bridge_send(cmd, &resp);
    snprintf(result, result_size, "%s", resp.payload);
    return resp.ok ? ESP_OK : ESP_FAIL;
}

static esp_err_t exec_grab(cJSON *args, char *result, size_t result_size)
{
    int force = 60;
    cJSON *f = cJSON_GetObjectItem(args, "force");
    if (f) force = f->valueint;

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "{\"cmd\":\"grab\",\"force\":%d}", force);

    uart_bridge_resp_t resp;
    uart_bridge_send(cmd, &resp);
    snprintf(result, result_size, "%s", resp.payload);
    return resp.ok ? ESP_OK : ESP_FAIL;
}

static esp_err_t exec_release(cJSON *args, char *result, size_t result_size)
{
    uart_bridge_resp_t resp;
    uart_bridge_send("{\"cmd\":\"release\"}", &resp);
    snprintf(result, result_size, "%s", resp.payload);
    return resp.ok ? ESP_OK : ESP_FAIL;
}

static esp_err_t exec_home(cJSON *args, char *result, size_t result_size)
{
    uart_bridge_resp_t resp;
    uart_bridge_send("{\"cmd\":\"home\"}", &resp);
    snprintf(result, result_size, "%s", resp.payload);
    return resp.ok ? ESP_OK : ESP_FAIL;
}

static esp_err_t exec_sensor_read(cJSON *args, char *result, size_t result_size)
{
    const char *type = cJSON_GetObjectItem(args, "type")->valuestring;
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "{\"cmd\":\"read\",\"sensor\":\"%s\"}", type);

    uart_bridge_resp_t resp;
    uart_bridge_send(cmd, &resp);
    snprintf(result, result_size, "%s", resp.payload);
    return resp.ok ? ESP_OK : ESP_FAIL;
}

/* ------------------------------------------------------------------ */
/* 公开接口                                                             */
/* ------------------------------------------------------------------ */

esp_err_t tool_arm_execute(const char *tool_name,
                            const char *args_json,
                            char *result_json,
                            size_t result_size)
{
    ESP_LOGI(TAG, "Execute tool: %s args: %s", tool_name, args_json);

    cJSON *args = cJSON_Parse(args_json);
    if (!args) {
        snprintf(result_json, result_size, "{\"ok\":false,\"error\":\"invalid args json\"}");
        return ESP_FAIL;
    }

    esp_err_t ret;
    if      (strcmp(tool_name, "arm_move_joint")     == 0) ret = exec_move_joint(args, result_json, result_size);
    else if (strcmp(tool_name, "arm_move_cartesian")  == 0) ret = exec_move_cartesian(args, result_json, result_size);
    else if (strcmp(tool_name, "arm_grab")            == 0) ret = exec_grab(args, result_json, result_size);
    else if (strcmp(tool_name, "arm_release")         == 0) ret = exec_release(args, result_json, result_size);
    else if (strcmp(tool_name, "arm_home")            == 0) ret = exec_home(args, result_json, result_size);
    else if (strcmp(tool_name, "sensor_read")         == 0) ret = exec_sensor_read(args, result_json, result_size);
    else {
        snprintf(result_json, result_size, "{\"ok\":false,\"error\":\"unknown tool\"}");
        ret = ESP_FAIL;
    }

    cJSON_Delete(args);
    return ret;
}

const char *tool_arm_get_schema(void)
{
    return TOOL_SCHEMA;
}
