# 修改日志 / Changelog

本项目（XIAOBAI）的所有版本变更记录。版本号规则见 `VERSIONING.md`。
格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)。

## [V0.0.3] - 2026-11-26

修复 V0.0.2 拍照仍出现彩虹/等高线状伪彩的问题。根据实拍截图判断，OV5640 在该套件上实际输出的是 **YUYV（YUV422）**，此前强行按 RGB565 解析，导致亮度基本正常但色相被严重打乱；仅调整 RGB565 字节序或 R/B 位域无法根治。

### 修复 / Fixed
- **摄像头输入格式改为原生 YUV422**：定制变体下 `config.pixel_format` 由 `PIXFORMAT_RGB565` 改为 `PIXFORMAT_YUV422`，不再依赖 OV5640 的 RGB565 寄存器配置。
- **新增显式 YUYV→RGB565 转换**：`Esp32Camera::Capture()` 中使用 BT.601 整数公式，把摄像头原生 YUYV 数据转换成 LVGL 可直接显示的 RGB565 预览；每两个像素共享一组 U/V，转换在 PSRAM 缓冲区中完成。
- **AI 上传保持原生 YUYV**：`Explain()` 对 YUV422 帧直接以 `V4L2_PIX_FMT_YUYV` 送给 JPEG 编码器，不再经过有损的 RGB565 中转，去掉了红蓝/伪彩修正链条。
- 移除板级 `SetSwapBytes()` / `SetSwapRBChannels()` 调用；保留这两个接口供其他 RGB565 板型使用，但本定制变体不再需要。

### 说明 / Notes
- 画面中若仍有细密彩色条纹，可能是手机拍摄 LCD 产生的摩尔纹，请以肉眼直视屏幕或改变拍摄角度为准。
- 本版本只改摄像头颜色链路，不影响语音、联网、电池管理和预览缩放逻辑。

### 修改 / Changed
- `main/boards/bread-compact-wifi-s3cam/compact_wifi_board_s3cam.cc`
- `main/boards/common/esp32_camera.cc`

## [V0.0.2] - 2026-11-26

修复 V0.0.1 拍照画面仍红蓝错色的问题：V0.0.1 的代码与说明不一致，实际多开了一次字节交换。

### 修复 / Fixed
- **拍照画面红蓝错色（本版本重点）**：V0.0.1 的变更说明写的是「关闭字节交换 + 只重排 R/B 位域」，但板级代码实际写成了 `SetSwapBytes(true)` + `SetSwapRBChannels(true)`，即字节交换和通道重排同时开启。字节交换会把 5/6/5 位域打乱，再叠加 R/B 重排后颜色依旧错乱。
  - 根因澄清：查阅 Linux 内核 OV5640 驱动源码确认，寄存器 `0x4300 = 0x61` 表示 **RGB565 大端**（字节序 `{r[4:0],g[5:3]},{g[2:0],b[4:0]}`），`0x6F` 才是小端——两者都是 **RGB 顺序，并非 BGR**。此前「OV5640 输出 BGR565」的判断有误。
  - 依据实测反馈反推：仅关字节交换（`swap=false`、不重排）时画面绿色正常、红蓝对调，说明字节序已被 LVGL 正确接受，只差 R/B 两个 5 位通道互换。因此本版本采用 **`SetSwapBytes(false)` + `SetSwapRBChannels(true)`**（四种组合中唯一未真正验证过的一种），一次修正预览与上传 JPEG。
  - 位域重排公式保持不变：`((v & 0x001F) << 11) | (v & 0x07E0) | ((v >> 11) & 0x001F)`，只交换高低 5 位，保留 G[5:0]。
- **预览位置**：全屏预览对象按比例缩放后居中（`LV_IMAGE_ALIGN_CENTER`）。320×240 画面在 240×320 屏幕上以宽度为准铺满，不再缩在一角。

### 说明 / Notes
- 若本版本颜色仍不理想，可快速在 `compact_wifi_board_s3cam.cc` 切换四个独立开关组合（`SetSwapBytes` × `SetSwapRBChannels`）中的任意一种，重新构建即可；两开关相互正交，调整成本很低。

### 修改 / Changed
- `main/boards/bread-compact-wifi-s3cam/compact_wifi_board_s3cam.cc`
- `main/boards/common/esp32_camera.cc`
- `main/display/lcd_display.cc`
- `main/Kconfig.projbuild`

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
