# dev_L — L 的开发工作区

这是协作开发者 L 的专属开发目录。

## 负责模块

- `esp32_master/` — ESP32 #1 主控固件（MimiClaw Agent）
- `vision/` — 视觉模块（OV5640 摄像头 + 图像识别）
- `agent/` — Python 端 AI Agent 交互层

## 文件说明

| 文件 | 说明 |
|------|------|
| `PROGRESS.md` | **开发进度追踪（每次会话必读）** |
| `README.md` | 本文件，目录说明 |
| `esp32_master/` | ESP32 #1 主控固件源码 |
| `vision/` | 视觉模块代码 |
| `agent/` | Agent 交互层代码 |

## 快速开始

每次开始开发前，先读：
1. `PROGRESS.md` — 当前任务和进度
2. `../PROGRESS.md` — 主项目最新状态
3. `../ARCHITECTURE.md` — 整体架构

## 与主项目协作

- 主项目代码不在此目录，在 `../scripts/`、`../esp32_slave/`、`../config/`
- 不要直接修改主项目文件，通过 PR 协作
- 接口约定见 `PROGRESS.md` 中的"接口约定"章节
