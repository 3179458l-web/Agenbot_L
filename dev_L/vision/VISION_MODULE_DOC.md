# 视觉模块技术文档

**路径**: `dev_L/vision/`
**更新**: 2026-03-15

---

## 1. 模块概述

视觉模块负责将 USB 摄像头采集的画面转换为机械臂可直接使用的世界坐标（mm）。整体流程：

```
USB 摄像头
    │
    ▼
camera.py          抓帧（BGR ndarray）
    │
    ▼
detector.py        Vision LLM（Claude claude-sonnet-4-6）识别物体，返回像素 bbox
    │
    ▼
calibration.py     单应矩阵变换，像素坐标 → 机械臂 XYZ（mm）
    │
    ▼
vision_server.py   对外暴露 Python API 或 TCP Socket 接口
```

---

## 2. 文件结构

```
vision/
├── camera.py          USB 摄像头采集封装
├── detector.py        Vision LLM 目标检测
├── calibration.py     单应矩阵标定与坐标变换
├── calibrate.py       交互式标定工具（运行一次）
├── vision_server.py   对外接口（Python API + TCP Socket）
├── requirements.txt   Python 依赖
└── config/
    └── camera_params.json   标定结果（运行 calibrate.py 后生成）
```

---

## 3. 各文件详解

### 3.1 `camera.py` —— 摄像头采集

**硬件**: BL-1080p-S10 USB UVC 1080p 摄像头，Windows DirectShow 驱动。

**主要接口**:

```python
cam = Camera(device_index=1, width=1280, height=720)
frame = cam.grab_frame()       # 返回 BGR ndarray (H, W, 3)
frame = cam.grab_frame_rgb()   # 返回 RGB ndarray（用于 API）
cam.preview()                  # 弹出预览窗口，按 q 退出
cam.release()
```

**注意事项**:
- `device_index=1`：默认从索引 1 开始（索引 0 通常是笔记本内置摄像头），如连接失败改为 0
- 初始化时自动丢弃前 5 帧（曝光预热）
- Windows 下使用 `CAP_DSHOW` 后端，延迟更低

**调试**:
```bash
python camera.py    # 直接运行 = 开启预览窗口
```

---

### 3.2 `detector.py` —— Vision LLM 检测

**原理**: 将帧编码为 Base64 JPEG，发送给 Claude claude-sonnet-4-6 Vision API，LLM 返回 JSON 格式的物体列表和像素 bbox。

**返回格式**:
```json
[
  {
    "label": "红色方块",
    "bbox": [120, 80, 220, 180],
    "center": [170, 130],
    "confidence": "high"
  }
]
```

**主要接口**:

```python
from detector import detect_objects, draw_detections

detections = detect_objects(frame, query="找红色物体")
vis_frame = draw_detections(frame, detections)  # 调试可视化
```

**参数**:

| 参数 | 类型 | 说明 |
|------|------|------|
| `frame` | `np.ndarray` | BGR 帧 |
| `query` | `str` | 可选，用户意图描述，留空则识别所有物体 |

**JPEG 质量**: 85（平衡 API 传输大小和识别精度）

**调试**:
```bash
python detector.py    # 实时预览，按回车触发一次检测
```

---

### 3.3 `calibration.py` —— 坐标变换

**原理**: 单应性矩阵（Homography）平面标定。假设所有目标物体在同一桌面高度，用 4 个标定点建立像素坐标 → 机械臂坐标的映射关系。

**坐标系约定**:
- 原点：机械臂基座中心
- X：正前方（mm）
- Y：正左方（mm）
- Z：固定桌面高度（配置文件指定）

**主要接口**:

```python
from calibration import Calibration

# 加载已有标定
calib = Calibration.load()
x_mm, y_mm, z_mm = calib.pixel_to_world(cx, cy)

# 从点对计算（由 calibrate.py 调用）
calib = Calibration.from_point_pairs(pixel_pts, world_pts, z_mm=0.0)
calib.save()
```

**配置文件** (`config/camera_params.json`):
```json
{
  "homography": [[...], [...], [...]],
  "z_mm": -50.0
}
```

---

### 3.4 `calibrate.py` —— 交互式标定工具

**只需运行一次**，标定结果长期保存。

**流程**:

1. 在工作台四角放 4 个标记点（彩色胶带十字）
2. 用机械臂末端接触各点，通过正运动学算出 XY 坐标（mm）
3. 运行脚本：

```bash
python calibrate.py
```

4. 画面弹出后：
   - 按 **空格** 冻结画面
   - **鼠标点击** 4 个标记点（按 P1→P4 顺序）
   - 依次输入各点的机械臂 XY 坐标
   - 输入桌面 Z 高度
5. 自动计算单应矩阵，打印误差，保存到 `config/camera_params.json`

**标记点建议布局**（俯视）:
```
P1 ─────── P2
│           │
│   工作区  │
│           │
P4 ─────── P3
```
四角尽量大，标定精度更高。

**标定验证输出示例**:
```
P1: 预测(100.2, 80.1) 实际(100.0, 80.0) 误差=0.22mm
P2: 预测(200.1, 80.3) 实际(200.0, 80.0) 误差=0.32mm
```

---

### 3.5 `vision_server.py` —— 对外接口

提供两种调用方式：

#### 方式一：Python 直接 import（Agent 层使用）

```python
from vision_server import VisionServer

vs = VisionServer()
objects = vs.detect(query="找红色方块")
# 返回含 world 坐标的列表
```

**返回格式**:
```json
[
  {
    "label": "红色方块",
    "bbox": [120, 80, 220, 180],
    "center": [170, 130],
    "world": [150.0, 30.0, -50.0],
    "confidence": "high"
  }
]
```

#### 方式二：TCP Socket（ESP32 主控通过 WiFi 调用）

**端口**: 5555
**协议**: JSON over TCP，换行符 `\n` 分隔

```
请求: {"action": "detect", "query": "找红色方块"}\n
响应: {"ok": true, "objects": [...]}\n

请求: {"action": "ping"}\n
响应: {"ok": true, "message": "pong"}\n
```

**启动服务**:
```bash
python vision_server.py
```

启动后同时开启 TCP 服务（后台线程）和本地预览窗口，按空格触发检测。

---

## 4. 依赖安装

```bash
pip install -r vision/requirements.txt
```

| 包 | 版本要求 | 用途 |
|----|----------|------|
| opencv-python | ≥4.8.0 | 摄像头采集、图像处理、标定 |
| anthropic | ≥0.25.0 | Claude Vision API |
| numpy | ≥1.24.0 | 矩阵运算 |

---

## 5. 环境变量

```bash
# 必须设置
set ANTHROPIC_API_KEY=sk-ant-...
```

---

## 6. 快速启动流程

### 首次使用（需要标定）

```bash
cd D:\Agenbot\dev_L\vision

# 1. 安装依赖
pip install -r requirements.txt

# 2. 测试摄像头
python camera.py

# 3. 测试 Vision LLM 检测
python detector.py

# 4. 执行标定（需要机械臂配合）
python calibrate.py

# 5. 启动视觉服务
python vision_server.py
```

### 已标定后直接启动

```bash
set ANTHROPIC_API_KEY=sk-ant-...
cd D:\Agenbot\dev_L\vision
python vision_server.py
```

---

## 7. 与 Agent 层的接口

Agent 层（`agent/tools.py`）通过以下方式调用：

```python
# tools.py 内部
from vision_server import VisionServer
vs = VisionServer()
objects = vs.detect(query)
```

返回的 `world` 字段（`[X_mm, Y_mm, Z_mm]`）直接传给 `arm_move_cartesian` 工具，驱动机械臂移动到目标位置。

---

## 8. 已知注意事项

- 摄像头 `device_index`：外接摄像头通常为 1，如打开失败改为 0 或 2
- 标定精度依赖标记点分布，四角越大精度越高，建议覆盖 150×150mm 以上区域
- Vision LLM 每次检测约耗时 1-3 秒（网络 API 调用）
- 未完成标定时，`world` 字段返回 `null`，Agent 层无法执行笛卡尔移动
