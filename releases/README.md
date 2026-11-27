# 固件发布 / Firmware Releases

本目录存放固件的本地副本，与 GitHub Release 保持一致。

## V0.0.5（当前推荐版本）

- 发布日期：2026-11-27
- 主要变更：修复 V0.0.4 固定白平衡校正幅度不足的问题。改用灰度世界自动白平衡，每帧采样统计 U/V 均值并自动拉回中性 128（限幅 ±80），不依赖手工标定常量；串口日志打印原始 U/V 与校正量。
- 固件文件：
  - `V0.0.5_bread-compact-wifi-s3cam-240x320-dl_merged.bin`（9,651,265 字节，MD5 `9d3a0da68b08f4807a8331699764af8a`）
  - `V0.0.5_bread-compact-wifi-s3cam-240x320-dl.zip`（2,520,362 字节，MD5 `493a09ef0bd2bed5711bbe59c026a8c8`）
- GitHub Release 下载：<https://github.com/deantyh/XIAOBAI/releases/tag/V0.0.5>
- 云端构建：Actions run `45939528213`（commit `faf8e62`），success。

## V0.0.4（旧版）

- 发布日期：2026-11-26
- 主要变更：修复 V0.0.3 拍照画面整体偏青蓝、红通道不足的问题。白纸测试确认屏幕显示正常，偏色来自 OV5640 输出；在 YUV 域做白平衡校正（U−36、V+41），预览与 AI 上传使用同一份校正后数据。
- 固件文件：
  - `V0.0.4_bread-compact-wifi-s3cam-240x320-dl_merged.bin`（9,651,265 字节，MD5 `24be2b9aeeeb8be8c7aad6d9594cc548`）
  - `V0.0.4_bread-compact-wifi-s3cam-240x320-dl.zip`（2,518,799 字节，MD5 `443f20db718d143762e6ba814fe6d327`）
- GitHub Release 下载：<https://github.com/deantyh/XIAOBAI/releases/tag/V0.0.4>
- 云端构建：Actions run `45836175826`（commit `f021425`），success。

## V0.0.3（旧版）

- 发布日期：2026-11-26
- 主要变更：修复 V0.0.2 拍照出现彩虹/等高线状伪彩的问题。OV5640 实际输出 YUYV（YUV422），V0.0.3 改为原生 YUV422 输入，预览显式转换为 RGB565，AI 上传直接编码原生 YUYV。
- 固件文件：
  - `V0.0.3_bread-compact-wifi-s3cam-240x320-dl_merged.bin`（9,651,265 字节，MD5 `997d913c36e1d96393ceb5c0f1fc23a1`）
  - `V0.0.3_bread-compact-wifi-s3cam-240x320-dl.zip`（2,518,647 字节，MD5 `bb38105fd410effa0724b464ce24e046`）
- GitHub Release 下载：<https://github.com/deantyh/XIAOBAI/releases/tag/V0.0.3>
- 云端构建：Actions run `45791755160`（commit `4b544b4`），success。

## V0.0.2（旧版）

- 发布日期：2026-11-26
- 主要变更：修复 V0.0.1 拍照仍红蓝的问题。V0.0.1 的代码与说明不一致，实际多开了一次字节交换；V0.0.2 改为「关闭字节交换 + 只做 R/B 位域重排」，一次修正预览与上传 JPEG，预览图同时居中铺满。
- 固件文件：
  - `V0.0.2_bread-compact-wifi-s3cam-240x320-dl_merged.bin`（9,651,265 字节，MD5 `d5574ec416628b6eadc77d0a5dfc15b6`）
  - `V0.0.2_bread-compact-wifi-s3cam-240x320-dl.zip`（2,518,425 字节，MD5 `4abeb437c5933625d8ade763f1a46638`）
- GitHub Release 下载：<https://github.com/deantyh/XIAOBAI/releases/tag/V0.0.2>
- 云端构建：Actions run `45785675949`（commit `ab57cd7`），success。

## V0.0.1（旧版，有问题，勿烧录）

- 发布日期：2026-11-26
- 主要变更：首个版本，尝试修复 OV5640 拍照红蓝错色（基于「BGR565」错误假设），预览与 AI 上传同时处理。
- 固件文件：
  - `V0.0.1_bread-compact-wifi-s3cam-240x320-dl_merged.bin`（9,651,265 字节，MD5 `58fcc0fae9c0c9e9c8f4bc50c250f36b`）
  - `V0.0.1_bread-compact-wifi-s3cam-240x320-dl.zip`（2,518,354 字节，MD5 `f6de60a83be2ff539bb1c5650c90df12`）
- GitHub Release 下载：<https://github.com/deantyh/XIAOBAI/releases/tag/V0.0.1>

## 说明

- 本地文件与 GitHub Release 资产一一对应，MD5 一致。
- 合并固件 `*_merged.bin` 烧录到 `0x0`；ZIP 为完整发布包。
- 后续版本按 `VERSIONING.md` 规则递增，并在此目录与 GitHub 同步更新。
