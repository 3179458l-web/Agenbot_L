# esp32_master — ESP32 #1 主控固件（MimiClaw Agent）

开发者：L

## 功能

- WiFi 连接
- Telegram Bot 长轮询，接收用户自然语言指令
- 调用 LLM API（Claude），支持 tool use
- 通过 UART 控制 ESP32 #2 从控（机械臂舵机）
- ReAct 循环：感知 → 推理 → 行动 → 回复

## 文件结构

```
main/
├── main.c              # 入口：初始化各模块，启动 Bot
├── wifi_manager.c/h    # WiFi STA 连接 + 断线重连
├── telegram_bot.c/h    # Telegram Bot 长轮询收发（待实现）
├── llm_client.c/h      # LLM API HTTP 调用（待实现）
├── uart_bridge.c/h     # 主从 UART JSON 通信桥
├── tool_arm.c/h        # 机械臂 tool 定义 + 执行
├── agent_loop.c/h      # ReAct 循环主控
└── secrets.h.example   # 密钥配置模板
```

## 快速开始

### 1. 配置密钥

```bash
cp main/secrets.h.example main/secrets.h
# 编辑 secrets.h，填入 WiFi / Telegram Token / API Key
```

### 2. 编译烧录

```bash
cd dev_L/esp32_master
idf.py set-target esp32s3
idf.py build
idf.py -p COM端口 flash monitor
```

### 3. 验收测试顺序

1. 串口看到 `WiFi connected` + IP 地址
2. 串口看到 `Telegram bot polling started`
3. 手机 Telegram 发消息，串口看到 `Received from ...`
4. Bot 回复（LLM 推理成功）
5. 发"把关节0转到90度"，串口看到 UART TX 日志，舵机动作

## 待实现

- [ ] `telegram_bot.c` — HTTP 长轮询实现
- [ ] `llm_client.c` — Claude API HTTPS 调用

## 接口约定（不可变）

与从控 UART 协议（见 `../../../esp32_slave/`）：
```json
发送：{"cmd":"move","joint":0,"angle":90,"speed":50}
接收：{"ok":true,"joint":0,"angle":90}
```
