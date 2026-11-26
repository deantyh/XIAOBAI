# 固件发布 / Firmware Releases

本目录存放固件的本地副本，与 GitHub Release 保持一致。

## V0.0.1（当前版本）

- 发布日期：2026-11-26
- 主要变更：修复 OV5640 拍照红蓝错色（BGR565 → RGB565 位域重排），预览与 AI 上传同时修正。
- 固件文件：
  - `V0.0.1_bread-compact-wifi-s3cam-240x320-dl_merged.bin`（9,651,265 字节，MD5 `58fcc0fae9c0c9e9c8f4bc50c250f36b`）
  - `V0.0.1_bread-compact-wifi-s3cam-240x320-dl.zip`（2,518,354 字节，MD5 `f6de60a83be2ff539bb1c5650c90df12`）
- GitHub Release 下载：<https://github.com/deantyh/XIAOBAI/releases/tag/V0.0.1>

## 说明

- 本地文件与 GitHub Release 资产一一对应，MD5 一致。
- 合并固件 `*_merged.bin` 烧录到 `0x0`；ZIP 为完整发布包。
- 后续版本按 `VERSIONING.md` 规则递增，并在此目录与 GitHub 同步更新。
