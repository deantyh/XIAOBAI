# XIAOBAI

基于官方小智 ESP32 项目定制的固件，运行在**面包板 ESP32-S3-CAM** 硬件上。

> 本项目源码基于 `78/xiaozhi-esp32` 的 `v2.6.0` tag，保留官方上游说明见 `README_zh.md` / `README_ja.md`（历史文件）。

## 硬件

| 部件 | 规格 |
|---|---|
| 主控 | ESP32-S3 |
| 摄像头 | **OV5640**（DVP） |
| 屏幕 | ST7789 SPI LCD，240×320 |
| 音频 | I2S simplex |
| 电池 | GPIO3 充电检测 + ADC2 电量采样 |

## 当前版本

`V0.0.3`（详见 `VERSION` 与 `CHANGELOG.md`）。

## 源码结构

- 板型：`bread-compact-wifi-s3cam`，定制构建变体 `bread-compact-wifi-s3cam-240x320-dl`。
- 关键定制文件：
  - `main/boards/bread-compact-wifi-s3cam/` — 板级初始化、电源管理、变体配置
  - `main/boards/common/esp32_camera.{h,cc}` — 摄像头采集；先交换字节再交换红蓝通道
  - `main/display/lcd_display.cc` — 预览图按比例居中显示
  - `main/Kconfig.projbuild` — 板级功能开关
- 版本规则见 `VERSIONING.md`；硬件适配与烧录说明见 `HARDWARE_ADAPTATION.md`。

## 编译

需要 ESP-IDF 6.1。推荐用项目自带的云端工作流：

1. 在 GitHub 仓库的 Actions 中手动触发 `build-custom` 工作流。
2. 构建完成后从 Actions 运行页面下载产物（merged 固件与 release zip）。

本地编译需先把工程放到**纯 ASCII 短路径**（如 `E:\xb`），否则 Windows 中文路径会导致 `xtensa-esp-elf-gcc` 无法读取 spec 文件：

```powershell
python scripts/build.py bread-compact-wifi-s3cam `
  --name bread-compact-wifi-s3cam-240x320-dl `
  --language zh-CN --wake-word nihaoxiaozhi --zip
```

## 烧录

合并固件 `*_merged.bin` 烧录到 `0x0`。分文件烧录地址：

| 地址 | 文件 |
|---|---|
| `0x00000` | bootloader.bin |
| `0x08000` | partition-table.bin |
| `0x0d000` | ota_data_initial.bin |
| `0x20000` | xiaozhi.bin |
| `0x800000` | generated_assets.bin |

## 版本库

- GitHub：`https://github.com/deantyh/XIAOBAI`
- 本地：`E:\XIAOBAI`
- 两者内容保持一致，每个版本打对应 tag。
