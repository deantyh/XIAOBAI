# 硬件适配说明（XIAOBAI V0.0.4）

## 目标硬件
- 主控：ESP32-S3（面包板 ESP32-S3-CAM）
- 屏幕：ST7789 SPI LCD，240×320，背光 GPIO38，IPS 反色开启
- 摄像头：**OV5640**（DVP）—— 该套件实际输出 YUYV（YUV422）。V0.0.4 在 YUV 域做白平衡校正（U−36、V+41），预览由 YUYV 显式转换为 RGB565，AI 上传使用同一份校正后的 YUYV（见 CHANGELOG V0.0.4）
- 音频：I2S simplex，麦克风 WS/SCK/DIN = GPIO1/2/42，喇叭 DOUT/BCLK/LRCK = GPIO39/40/41
- 电池：GPIO3 充电检测，ADC2/CH3 电量采样（仅 `...-dl` 变体启用）

> 注意：LCD 使用 GPIO19/GPIO20，与 ESP32-S3 内置 USB D-/D+ 复用。烧录时使用板载 USB-TTL 芯片的串口，不要依赖 USB-JTAG。

## 构建变体

定制变体：`bread-compact-wifi-s3cam-240x320-dl`，在官方 `bread-compact-wifi-s3cam` 板型上叠加：
- `CONFIG_LCD_ST7789_240X320=y`
- `CONFIG_BREAD_COMPACT_WIFI_CAM_POWER_MANAGEMENT=y`
- `CONFIG_BREAD_COMPACT_WIFI_CAM_CAMERA_FIXES=y`（OV5640 色彩修正 + 预览居中适配）
- `camera_hmirror=true`, `camera_vflip=false`

## 构建方式一：GitHub Actions 云端（推荐）
在 `deantyh/XIAOBAI` 仓库 Actions 中手动触发 `build-custom` 工作流，构建完成后从运行页面下载产物。

## 构建方式二：本地构建脚本
需 ESP-IDF 6.1，且工程必须位于纯 ASCII 短路径（如 `E:\xb`）：
```bash
python scripts/build.py bread-compact-wifi-s3cam \
  --name bread-compact-wifi-s3cam-240x320-dl \
  --language zh-CN --wake-word nihaoxiaozhi --zip
```

## 烧录
- 单文件合并固件 `*_merged.bin`：地址 `0x0`，芯片 ESP32-S3，Flash 16MB，DIO，80MHz。
- 分文件镜像：
  - `0x00000 bootloader.bin`
  - `0x08000 partition-table.bin`
  - `0x0d000 ota_data_initial.bin`
  - `0x20000 xiaozhi.bin`
  - `0x800000 generated_assets.bin`

## 验证要点
1. 开机屏幕正常显示 240×320 内容，无上下/左右镜像。
2. 按键配网：连接热点后浏览器访问 `192.168.4.1`，输入 2.4GHz Wi-Fi 密码。
3. **拍照预览颜色正常（红、绿、蓝都正确，不再花屏或红蓝对调），预览图居中且宽度铺满屏幕；上传给 AI 的拍照颜色同样正常。**
4. MCP `self.get_device_status` 能看到电量（`-dl` 变体）。
5. 电池供电静置 60 秒后进入低功耗表情/降背光；长时间空闲触发深度睡眠，接上充电器后退出休眠。

变更明细见 [CHANGELOG.md](CHANGELOG.md)，版本规则见 [VERSIONING.md](VERSIONING.md)。
