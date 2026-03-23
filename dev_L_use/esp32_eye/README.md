# MimiClaw Eye — ESP32-CAM 拍照服务固件

## 职责

ESP32-CAM 在整个 MimiClaw 系统中扮演 **Eye（眼睛）** 角色：
- 连接 Brain（ESP32-S3 #1）发出的 WiFi AP 热点
- 提供 HTTP 拍照接口，Brain 按需 GET 获取 JPEG

**不负责**：图像识别、坐标变换、视觉伺服，这些全部由 Brain 侧处理。

---

## 硬件

| 项目 | 规格 |
|------|------|
| 主板 | AI-Thinker ESP32-CAM |
| 摄像头 | OV5640（板载） |
| 开发框架 | ESP-IDF v5.x |

---

## 网络拓扑

```
Brain (ESP32-S3 #1)
  与 CAM 处于同一 WiFi 环境
       │
       │ WiFi STA / 同网段通信
       ▼
CAM (ESP32-CAM)
  通过 DHCP 动态获取 IP
  HTTP Server: port 80
```

---

## HTTP 接口

### GET /capture

返回一帧 JPEG 图片。

```
Request:  GET http://<camera-ip>/capture
Response: HTTP 200, Content-Type: image/jpeg
          <raw JPEG bytes>

Error:    HTTP 503  摄像头未就绪
          HTTP 500  拍照失败
```

当前参数固定：VGA (640×480)，JPEG quality=12。

### GET /status

返回摄像头状态 JSON。

```
Request:  GET http://<camera-ip>/status
Response: HTTP 200, Content-Type: application/json
{
  "camera": "ready",
  "resolution": "640x480",
  "free_heap": <bytes>,
  "uptime_s": <seconds>
}
```

---

## 目录结构

```
esp32_eye/
├── README.md               ← 本文件
├── PROGRESS.md             ← 开发进度追踪
├── CMakeLists.txt          ← ESP-IDF 项目配置
├── doc/
│   └── TECHNICAL.md        ← 技术设计文档
└── main/
    ├── CMakeLists.txt
    ├── app_main.c          ← 启动入口
    ├── wifi_sta.c/h        ← WiFi STA + DHCP
    ├── camera_init.c/h     ← OV5640 初始化
    └── http_server.c/h     ← HTTP /capture /status
```

---

## 启动流程

```
上电
 └→ WiFi STA 连接同一 WiFi 环境（DHCP 动态获取 IP）
     └→ 摄像头初始化（VGA, JPEG quality=12）
         └→ HTTP Server 启动（port 80）
             └→ 就绪，等待 Brain 请求
```

---

## 待确认项

- [ ] Brain AP 热点密码
- [ ] Brain AP 网段确认（当前假设 192.168.4.0/24）
- [ ] 交付方式：自行烧录验证 or 交给他人

---

## 快速开始

```bash
cd esp32_eye

# 设置目标芯片
idf.py set-target esp32

# 编译
idf.py build

# 烧录（端口按实际修改）
idf.py -p COM? flash monitor
```

> 说明：本项目在 ESP-IDF 5.3 下通过 `main/idf_component.yml` 自动引入
> `espressif/esp32-camera`。首次 `idf.py build` 时会自动下载该依赖。

---

## 常见问题

### 1. Failed to resolve component 'esp_camera'

如果看到类似报错：

```text
Failed to resolve component 'esp_camera'
```

说明工程把摄像头组件当成了内置组件直接引用。
本项目的解决方式是：

- 使用 `main/idf_component.yml` 引入 `espressif/esp32-camera`
- 不在 `main/CMakeLists.txt` 的 `REQUIRES` 里直接写 `esp_camera`

然后重新执行：

```bash
idf.py build
```

---

## 与整体系统的关系

参见：`../doc/2026-03-19-pcless-architecture-design.md` 第 4.4 节 Eye 固件设计。
