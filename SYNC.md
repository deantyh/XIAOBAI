# 本地与 GitHub 同步说明

为确保「本地更新情况与 GitHub 一致」，本项目约定如下：

## 双份位置

| 内容 | 本地 | GitHub |
|---|---|---|
| 源码与文档 | `E:\XIAOBAI` | `deantyh/XIAOBAI` 的 `main` 分支 |
| 固件产物 | `E:\XIAOBAI\releases\` | `deantyh/XIAOBAI` 的 `releases\` 目录 |
| 版本号 | 本地 `VERSION` | 仓库 `VERSION` + Git tag |
| 变更记录 | 本地 `CHANGELOG.md` | 仓库 `CHANGELOG.md` |

## 更新流程（每次发版）

1. 修改代码与文档，更新 `VERSION`、`CHANGELOG.md`。
2. 本地提交到 `main`，推送到 GitHub（`git push origin main`）。
3. 打版本 tag（如 `V0.0.2`）并推送（`git push origin V0.0.2`）。
4. 触发 GitHub Actions `build-custom` 构建固件。
5. 下载构建产物到 `E:\XIAOBAI\releases\`，再次提交推送，使仓库与本地固件一致。
6. 核对：本地与 GitHub 文件列表、MD5 一致。

## 说明

- `.gitignore` 已调整：放行 `releases/` 下的固件（zip 与 bin），确保固件能入库同步；`managed_components/`、`build/` 等构建缓存仍被排除。
- 云端构建产物也可从 Actions 运行页面下载。
