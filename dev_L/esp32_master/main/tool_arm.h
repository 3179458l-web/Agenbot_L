#ifndef TOOL_ARM_H
#define TOOL_ARM_H

/**
 * tool_arm —— 注册给 LLM 的机械臂控制 tools
 *
 * 每个 tool 对应：
 *   1. JSON schema（传给 LLM，告诉它 tool 的参数格式）
 *   2. 执行函数（解析 LLM 的 args_json，调用 uart_bridge 发指令）
 *
 * Tools 列表：
 *   - arm_move_joint   : 控制单个关节角度
 *   - arm_move_cartesian: 笛卡尔坐标移动（x,y,z mm）
 *   - arm_grab         : 夹爪抓取（force 0-100）
 *   - arm_release      : 夹爪释放
 *   - arm_home         : 回零姿态
 *   - sensor_read      : 读取传感器（imu / current / pressure）
 */

#include "esp_err.h"

/**
 * @brief 执行 tool_use 调用
 * @param tool_name   LLM 返回的 tool 名称
 * @param args_json   LLM 返回的参数 JSON
 * @param result_json 执行结果 JSON（输出给 LLM 的下一轮）
 * @param result_size result_json 缓冲区大小
 */
esp_err_t tool_arm_execute(const char *tool_name,
                            const char *args_json,
                            char *result_json,
                            size_t result_size);

/**
 * @brief 获取所有 tool 的 JSON schema（用于构造 LLM 请求）
 *        返回静态字符串，无需释放
 */
const char *tool_arm_get_schema(void);

#endif // TOOL_ARM_H
