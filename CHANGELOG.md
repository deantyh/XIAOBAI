# 修改日志 / Changelog

本项目（XIAOBAI）的所有版本变更记录。版本号规则见 `VERSIONING.md`。
格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)。

## [V0.0.1] - 2026-11-26

首个版本。基于官方 `78/xiaozhi-esp32` 的 `v2.6.0` tag（commit `4de00ae`）定制，面向面包板 ESP32-S3-CAM（**OV5640** 摄像头 + ST7789 240×320 屏幕），并修复拍照红蓝错色问题。

### 新增 / Added
- 新建 GitHub 仓库 `deantyh/XIAOBAI` 与本地工程 `E:\XIAOBAI`，内容与 GitHub 保持一致。
- 建立版本管理规范：`VERSION`、`VERSIONING.md`、本 `CHANGELOG.md`，后续每次更新按规则递增版本号。
- 新增构建变体 `bread-compact-wifi-s3cam-240x320-dl`：
  - 固定 `CONFIG_LCD_ST7789_240X320=y`，屏幕参数与硬件一致。
  - 开启 `camera_hmirror=true`，摄像头画面方向与旧版固件一致。
- 恢复电池供电能力（官方 v2.6.0 从该板型移除了相关代码）：
  - 从 v2.5.2 移植 `power_manager.h`：ADC2 读取电量、GPIO3 检测充电状态。
  - 新增 Kconfig 选项 `CONFIG_BREAD_COMPACT_WIFI_CAM_POWER_MANAGEMENT`（默认关闭，仅定制变体开启）。
  - 空闲 60 秒进入低功耗显示、36000 秒（10 小时）请求深度睡眠；外接供电时自动关闭休眠。
  - 实现 `GetBatteryLevel()`，使电量查询与 MCP `self.get_device_status` 能读到真实电量。
- 新增云端构建工作流 `.github/workflows/build-custom.yml`，仅编译本定制变体。

### 修复 / Fixed
- **修复拍照红蓝错色（本版本重点）**：确认硬件摄像头为 **OV5640**。其驱动在 `PIXFORMAT_RGB565` 模式下实际向传感器写入 `0x4300 = 0x61`（RGB565 但字节序为 BGR），导致 R/B 通道对调，拍照预览与上传给 AI 的图片红蓝颠倒。
  - 在 `Camera` 基类新增可选接口 `SetSwapRBChannels(bool)`，`Esp32Camera` 实现之。
  - 在 `Esp32Camera::Capture()` 中新增 BGR565→RGB565 位域重排：`((v & 0x001F) << 11) | (v & 0x07E0) | ((v >> 11) & 0x001F)`，作用对象为 `encode_buf_`，因此**预览画面与上传 JPEG 同时修正**。
  - 板级 `InitializeCamera()` 在 `CONFIG_BREAD_COMPACT_WIFI_CAM_CAMERA_FIXES` 开关下设置 `SetSwapBytes(false)` + `SetSwapRBChannels(true)`：保留原生字节序并仅重排 R/B 位域。
  - 注意：上一版仅改 `SetSwapBytes`（字节序）无法解决通道顺序问题，本次用位域重排从根本上修正。
- 修复拍照预览过小、不满屏：
  - 采集分辨率由官方 `FRAMESIZE_VGA`(640×480) 改为 `FRAMESIZE_QVGA`(320×240)，与屏幕比例匹配。
  - 预览对象由半屏改为全屏 `width_ × height_`，按屏幕 87.5% 等比缩放，接近铺满。
  - 显示层改动同样受 `CONFIG_BREAD_COMPACT_WIFI_CAM_CAMERA_FIXES` 保护。

### 修改 / Changed
- `main/Kconfig.projbuild`：新增 `CONFIG_BREAD_COMPACT_WIFI_CAM_POWER_MANAGEMENT` 与 `CONFIG_BREAD_COMPACT_WIFI_CAM_CAMERA_FIXES` 两个板级开关，均默认关闭，仅定制变体开启，不影响官方默认板型。
- `main/boards/bread-compact-wifi-s3cam/config.json`：新增定制变体及其 `sdkconfig_append`。
- `main/boards/bread-compact-wifi-s3cam/compact_wifi_board_s3cam.cc`：按开关接入电源管理、摄像头色彩修正、深度睡眠前拉低板载 LED 的 RTC GPIO 降低待机功耗。
- `main/boards/common/camera.h`、`esp32_camera.h`、`esp32_camera.cc`：新增 R/B 通道重排接口与实现。
- `main/display/lcd_display.cc`：预览图放大显示（受开关保护）。

### 说明 / Notes
- 基线：官方 tag `v2.6.0`（commit `4de00ae`），上游 `https://github.com/78/xiaozhi-esp32`。
- 编译要求：ESP-IDF `release/v6.1`；Windows 下需使用纯 ASCII 短路径。
- 屏幕：ST7789 SPI，240×320，SPI mode 0，RGB 顺序，IPS 反色开启。
- 未改动官方联网、语音、MCP、OTA 逻辑；未内置任何云端账号或激活信息。
