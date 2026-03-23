# MimiClaw Eye — 技术设计文档

> **定位**：记录关键技术决策、接口规范、参数依据，供开发和联调使用。

---

## 一、模块划分

```
app_main.c
  │
  ├── wifi_sta.c        WiFi STA 连接 + DHCP 获取 IP
  ├── camera_init.c     摄像头初始化
  └── http_server.c     HTTP Server（/capture, /status）
```

每个模块职责单一，`app_main.c` 只负责按顺序初始化，不含业务逻辑。

---

## 二、启动顺序

```
app_main()
  1. nvs_flash_init()                    NVS 初始化（WiFi 依赖）
  2. wifi_sta_init()                     连接同一 WiFi 环境，通过 DHCP 获取 IP
     └─ 等待 IP_EVENT_STA_GOT_IP         阻塞直到 IP 获取成功
  3. camera_init()                       摄像头初始化
     └─ 失败则重启                        摄像头不可用无意义继续运行
  4. http_server_start()                 HTTP Server 启动
  5. 读取当前 DHCP IP 并打印访问地址
  6. 进入空闲，等待请求
```

任何步骤失败都通过串口打印明确错误信息，便于调试。

---

## 三、WiFi 模块设计

### 3.1 连接参数

| 参数 | 值 |
|------|----|
| 模式 | STA |
| 网络环境 | 与 Brain 处于同一 WiFi 环境 |
| IP 获取方式 | DHCP 动态分配 |
| 发现方式 | 启动日志打印当前 IP；后续可扩展 mDNS / 注册机制 |

### 3.2 DHCP 实现方式

使用 ESP-IDF 默认 STA DHCP 客户端获取地址，不再写死静态 IP。
启动完成后通过 `esp_netif_get_ip_info()` 读取当前地址，并打印成：

```text
http://<camera-ip>/capture
http://<camera-ip>/status
```

### 3.3 重连策略

- 连接失败或断开 → 自动重连（ESP-IDF 默认行为）
- 不做指数退避，Eye 的唯一职责就是提供拍照服务，应尽快恢复

---

## 四、摄像头模块设计

### 4.1 硬件：AI-Thinker ESP32-CAM Pin 定义

```c
// AI-Thinker 标准 pin 定义
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1   // 无硬件 RESET 引脚
#define CAM_PIN_XCLK     0
#define CAM_PIN_SIOD    26   // SDA
#define CAM_PIN_SIOC    27   // SCL
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0       5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22
```

### 4.2 初始化参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 分辨率 | `FRAMESIZE_VGA` | 640×480 |
| 像素格式 | `PIXFORMAT_JPEG` | 直接输出 JPEG |
| JPEG quality | `12` | 值越小质量越高，12 是平衡点 |
| fb_count | `1` | frame buffer 数量，单帧足够 |
| XCLK 频率 | `20000000` | 20MHz |

### 4.3 拍照流程

```
camera_fb_t *fb = esp_camera_fb_get()   获取一帧
  │
  ├── 成功 → 将 fb->buf, fb->len 通过 HTTP 返回
  │
  └── 失败 → 返回 HTTP 500
      └→ esp_camera_fb_return(fb)        必须释放，否则内存泄漏
```

**注意**：`esp_camera_fb_return()` 必须在每次 `fb_get()` 后调用，无论成功失败。

---

## 五、HTTP Server 设计

### 5.1 框架选择

使用 ESP-IDF 内置的 `esp_http_server`，不引入第三方库。

### 5.2 接口规范

#### GET /capture

```
请求: GET /capture HTTP/1.1

响应（成功）:
  HTTP/1.1 200 OK
  Content-Type: image/jpeg
  Content-Length: <size>
  <JPEG 字节流>

响应（摄像头未就绪）:
  HTTP/1.1 503 Service Unavailable

响应（拍照失败）:
  HTTP/1.1 500 Internal Server Error
```

当前版本参数固定，不解析 query string。后续如需支持动态参数再扩展。

#### GET /status

```
请求: GET /status HTTP/1.1

响应:
  HTTP/1.1 200 OK
  Content-Type: application/json
  {
    "camera": "ready",
    "resolution": "640x480",
    "free_heap": 45000,
    "uptime_s": 3600
  }
```

### 5.3 并发处理

Brain 每次拍照是串行请求，不需要处理并发。`esp_http_server` 默认单线程处理足够。

---

## 六、内存估算

AI-Thinker ESP32-CAM 有 4MB PSRAM。

| 用途 | 大小 |
|------|------|
| OV5640 frame buffer (VGA JPEG) | ~30-80 KB |
| HTTP 响应缓冲 | 与 frame buffer 共用 |
| WiFi + TCP/IP 栈 | ~60 KB |
| 系统/FreeRTOS | ~30 KB |
| **合计** | **~170 KB**，余量充足 |

frame buffer 分配在 PSRAM（`MALLOC_CAP_SPIRAM`）。

---

## 七、关键技术决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| 固定 IP vs DHCP | DHCP 动态分配 | 现阶段 Brain 与摄像头处于同一 WiFi 环境通信，不再要求写死 192.168.4.2 |
| 地址获取方式 | 启动日志打印当前 IP | 先用最简单可行方案，后续按需扩展 mDNS 或注册上报 |
| 参数动态 vs 固定 | 固定 VGA/quality=12 | 先跑通链路，后续按需扩展 |
| 功耗优化 | 不做 | 基础拍照优先，发热问题后续处理 |
| 第三方库 | 不引入 | 用 ESP-IDF 内置组件，减少依赖 |

---

## 八、与整体系统的接口约定

Brain 侧调用方式（同一 WiFi 环境下）：

```
GET http://<camera-ip>/capture
```

其中 `<camera-ip>` 由 DHCP 动态分配，摄像头启动日志会打印当前地址。

Brain 预期响应：
- `HTTP 200` + `Content-Type: image/jpeg` + JPEG 字节流
- 超时设置：Brain 侧 5 秒超时

CAM 这边只需保证在 5 秒内返回响应即可。

---

## 九、构建依赖说明

### 9.1 ESP-IDF 5.3 兼容性

在 ESP-IDF 5.3 环境下，`esp32-camera` 不是默认内置到工程里的组件，
如果直接在 `main/CMakeLists.txt` 里写：

```cmake
REQUIRES esp_camera
```

会报错：

```text
Failed to resolve component 'esp_camera'
```

### 9.2 正确做法

本项目采用 ESP-IDF component manager 引入摄像头组件。

文件：`main/idf_component.yml`

```yaml
dependencies:
  espressif/esp32-camera: "*"
```

同时 `main/CMakeLists.txt` 中 **不要** 再把 `esp_camera` 写进 `REQUIRES`。

### 9.3 构建行为

首次执行：

```bash
idf.py build
```

ESP-IDF 会自动下载 `espressif/esp32-camera` 依赖。
如果网络不可用或下载失败，构建会卡在依赖解析阶段。

---

## 十、待确认项

| 项目 | 状态 | 影响 |
|------|------|------|
| Brain AP 密码 | ❌ 未知 | 阻塞 wifi_sta.c 开发 |
| Brain AP 网段 | 假设 192.168.4.0/24 | 影响固定 IP 配置 |
| 交付方式 | ❌ 未知 | 影响是否需要烧录验证 |
