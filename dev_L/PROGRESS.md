# L 的开发进度追踪

> **给 AI 的说明**：
> 1. 每次会话开始先读此文件，了解 L 当前的开发状态
> 2. 每次完成任务后更新"最新进度"部分
> 3. 简洁但完整：做了什么、结果如何、下一步是什么

---

## 📍 最新进度

**日期**: 2026-03-15
**完成**: Agent 交互层完整实现
**状态**: 🟢 Agent 层完成，可离线测试

### 本次完成内容（Agent 交互层）
- 实现 `agent/agent.py` —— ReAct 循环（Claude tool_use，最多8轮）
- 实现 `agent/tools.py` —— 6个工具 Schema + 执行器（对接 arm_kinematics + vision_server）
- 实现 `agent/cli.py` —— 命令行交互界面（支持 --offline 离线模拟模式）
- 完成摄像头离线/在线测试（detector.py 已验证 Vision LLM 正确返回 bbox）

### 之前完成内容（主控固件）
- 搭建 `esp32_master/` 完整项目结构
- 实现 `uart_bridge.c/h` / `wifi_manager.c/h` / `tool_arm.c/h` / `agent_loop.c/h`
- `telegram_bot.c` / `llm_client.c` 暂注释（待实现）

### 当前阻塞
- **ESP-IDF 工具链未完整安装**：需在 PowerShell 运行 `D:\ESP\v5.5.3\esp-idf\install.ps1`
- Agent 层需要实际硬件（摄像头 + 机械臂）做端到端测试

### 下一步
- [ ] 运行 `python agent/cli.py --offline` 做离线测试
- [ ] 接好摄像头后运行 `python vision/vision_server.py` + `python agent/cli.py`
- [ ] 验证 `idf.py build` 编译通过
- [ ] 实现 `telegram_bot.c`（HTTP long polling）
- [ ] 实现 `llm_client.c`（HTTPS Claude API）

---

## 👁️ 当前任务：视觉模块开发

---

## 📊 负责模块概览

| 模块 | 路径 | 状态 |
|------|------|------|
| ESP32 #1 主控固件 | `esp32_master/` | 🟡 结构完成，待编译验证 |
| 视觉模块 | `vision/` | 🟡 进行中 |
| Agent 交互层 | `agent/` | ⏳ 待开发 |

---

## 🤖 ESP32 #1 主控固件 (`esp32_master/`)

**目标**: MimiClaw Agent 固件 —— Telegram 接收指令 → LLM 推理 → tool use 控制机械臂

### 任务列表

- [x] Task 1: 项目结构 + ESP-IDF 初始化
- [x] Task 2-pre: uart_bridge / wifi_manager / tool_arm / agent_loop 框架
- [ ] Task 2: Telegram Bot Poller（HTTPS 长轮询）
- [ ] Task 3: LLM API 调用（HTTP 请求 Claude/GPT）
- [ ] Task 4: tool use 解析 + UART 发送给从控
- [ ] Task 5: 主从联调测试

### 完成记录

（暂无）

---

## 👁️ 视觉模块 (`vision/`)

**目标**: USB 摄像头 → Vision LLM 识别 → 像素坐标转世界坐标 → XYZ 输出给机械臂

**硬件**: BL-1080p-S10 USB UVC 1080p 摄像头，接 PC，Python 处理

### 任务列表

- [x] Task 1: `camera.py` —— USB 摄像头采集封装
- [x] Task 2: `detector.py` —— Vision LLM 检测（Claude claude-sonnet-4-6），返回 bbox + label
- [x] Task 3: `calibration.py` —— 单应矩阵标定，像素→机械臂 XYZ（mm）
- [x] Task 4: `calibrate.py` —— 交互式标定工具（鼠标点击 4 点）
- [x] Task 5: `vision_server.py` —— Python API + TCP Socket 对外接口
- [ ] Task 6: 实际测试（需摄像头接到 PC 后运行）
- [ ] Task 7: 与 agent 层对接

### 当前阻塞
- 需要实际运行测试，先安装依赖：
  ```
  pip install -r vision/requirements.txt
  ```
- 需要设置环境变量：`ANTHROPIC_API_KEY=sk-ant-...`
- 标定需要配合机械臂硬件完成（运行 `calibrate.py`）

### 完成记录

**2026-03-15**：视觉模块框架完整搭建，5 个文件全部完成

---

## 🧠 Agent 交互层 (`agent/`)

**目标**: Python 端 AI Agent 接口 —— 接受自然语言指令，调用运动学层执行

### 任务列表

- [x] Task 1: Agent 框架设计（工具定义、上下文管理）
- [x] Task 2: 与 `arm_kinematics.py` 对接
- [x] Task 3: 对话管理 + 错误恢复
- [ ] Task 4: 端到端测试（需硬件）

### 完成记录

**2026-03-15**：Agent 层完整实现
- `agent.py`：ReAct 循环，Claude tool_use API，多轮对话，最多 8 轮工具调用
- `tools.py`：6 个工具（arm_move_cartesian/joint/grab/release/home, vision_detect），懒加载硬件接口
- `cli.py`：命令行界面，`--offline` 离线模拟模式

---

## 🔗 与主项目的接口约定

### 从控串口协议（已固定，由主项目定义）

```json
// 指令（发送给 ESP32 #2）
{"cmd":"move","joint":0,"angle":90,"speed":50}
{"cmd":"home"}

// 响应
{"ok":true,"joint":0,"angle":90}
{"ok":false,"error":"invalid json"}
```

### 运动学层接口（已固定，由主项目定义）

```python
from scripts.arm_kinematics import ArmKinematics
ak = ArmKinematics('../config/arm_config.json', controller=ctrl)
ak.move_to(x, y, z)   # 笛卡尔坐标移动
ak.move_to_home()      # 回零
```

---

## 📋 快速命令

```bash
# 进入开发目录
cd D:/Agenbot/dev_L

# 查看主项目最新状态
cat ../PROGRESS.md

# 运动学层测试（验证与主项目对接）
cd ../scripts
python test_kinematics.py -v
```

---

## 🐛 已知问题 / 注意事项

- ESP32-S3 HW678 板的 I2C 引脚是 GPIO8/9（不是 GPIO21/22）
- PCA9685 OE 引脚必须接地，否则舵机不动
- 从控串口：COM7，115200 波特率

---

**最后更新**: 2026-03-15
**开发者**: L
