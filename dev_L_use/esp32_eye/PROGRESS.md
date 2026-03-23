# MimiClaw Eye — 开发进度

> **规则**：每次完成一个任务后必须更新本文件的"最新进度"部分。

---

## 最新进度

**日期**: 2026-03-20
**状态**: 固件代码完成，已切换为同 WiFi 环境下的 DHCP 动态 IP 通讯，待重新编译验证

### 本次完成
- `CMakeLists.txt`（项目 + main）
- `wifi_sta.c/h`：STA 连接热点，改为 DHCP 动态获取 IP
- `camera_init.c/h`：摄像头初始化代码框架已完成
- `http_server.c/h`：/capture 返回 JPEG，/status 返回 JSON
- `app_main.c`：启动顺序串联，任何步骤失败自动重启，并自动打印当前 DHCP IP 访问地址
- 解决 ESP-IDF 5.3 下 `esp_camera` 组件解析失败问题：
  - 从 `main/CMakeLists.txt` 的 `REQUIRES` 中移除 `esp_camera`
  - 新增 `main/idf_component.yml`
  - 通过 component manager 引入 `espressif/esp32-camera`
- 调整通信架构：
  - 不再依赖写死 `192.168.4.2`
  - Brain 与摄像头在同一 WiFi 环境下通信
  - 摄像头启动后打印当前 IP，供 Brain 直接调用

### 待确认（阻塞项）
- [ ] Brain 所在 WiFi 环境的 SSID / 密码
- [ ] Brain 如何获取 camera IP（先用启动日志手动读；后续是否要扩展 mDNS / 注册机制）
- [ ] 最终摄像头型号与 pin 定义确认（当前代码需再核对板载型号）
- [ ] 交付方式

### 下一步
- 重新 `idf.py build`
- 烧录上板，确认启动日志打印当前 DHCP IP
- 验证 `/status` 与 `/capture`
- 与 Brain 联调：Brain 使用日志中的当前 IP 发起请求

---

## 任务列表

### 进行中
- 无

### 待开始
- [x] T1: `wifi_sta.c/h` — WiFi STA 连接 + 固定 IP 192.168.4.2
- [x] T2: `camera_init.c/h` — OV5640 初始化，VGA, quality=12
- [x] T3: `http_server.c/h` — `/capture` 返回 JPEG，`/status` 返回 JSON
- [x] T4: `app_main.c` — 启动顺序串联
- [x] T5: `CMakeLists.txt` — 项目构建配置
- [ ] T6: 本地编译通过
- [ ] T7: 烧录上板，串口监视器确认启动正常
- [ ] T8: 与 Brain 联调，Brain 能成功拉到图

### 已完成
- [x] 分工确认
- [x] 架构设计
- [x] 文档初始化

---

## 硬件配置备忘

| 项目 | 值 |
|------|----|
| 板型 | AI-Thinker ESP32-CAM |
| 摄像头 | OV5640（板载） |
| 串口芯片 | 需外接 USB-TTL（AI-Thinker 无板载串口芯片） |
| 烧录引脚 | IO0 接 GND 进入下载模式 |
| 分辨率 | VGA 640×480（固定） |
| JPEG quality | 12（固定，值越小质量越高） |
| 固定 IP | 不再写死，改为 DHCP 动态分配 |
| AP SSID | 当前测试值：iPhone |
| AP 密码 | 当前测试值：已写入代码，后续应改为正式环境配置 |

---

## 故障排查记录

_开发过程中遇到的问题和解决方法记录在这里。_
