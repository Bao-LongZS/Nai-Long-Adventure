# P1-0 基线与素材记录（2026-10-15）

P0 实现改动前已封存独立快照。快照所依据的 Git HEAD 为 `32d7d29d928e863f1e3adff359b4c52619bfde50`；捕获前 Git 状态见 `p0_before_p1_manifest.json`。当时已存在未跟踪的 `UE5/Records/P1_计划书_20261014.md`，没有覆盖它。快照创建后其余 P1 文件会继续变化，不应把当下 Git 状态当作 P0 快照状态。

## P0 可恢复快照

- `p0_before_p1.zip`：548 个文件，252,894,590 字节，SHA256 `e512133ec96ff268c5e72c6bbebf0141e8b11d0e94bbe2e1e9545fcf8be0e443`。逐文件路径、大小及 SHA256 在 `p0_before_p1_manifest.json`。
- 包含 P0 `Config/Source/Content`、`.uproject`、原启动/阶段脚本、P0 最终报告与原始证据，以及 `20261013T053123Z_44908` 完整自动测试输出（含 24 张 PNG）。也包含工作工程中的 UE 模板 `Content/Characters` 依赖，尽管该目录不纳入 Git。
- 不含可重建的 `Binaries/Intermediate/DerivedDataCache`、整个 `Saved` 目录、UE 引擎安装文件。需要相同引擎版本 UE 5.8.3（CL58210709）重建原生模块。
- ZIP 已逐条重新读取并对照 548 个文件 SHA256 验证，ZIP 自身 SHA256 与清单相符。ZIP 因体积约 241 MiB 按 `.gitignore` 保留为**本机独立归档**；Git 仓库本身不能代替此 ZIP。若要迁移机器，需连同 ZIP 和清单一起转存。
- 安全查看/恢复：先校验 ZIP SHA256，再解压到独立空目录比较；确认后再决定是否恢复到工作工程。不要直接在当前 P1 工程上执行 `Expand-Archive -Force`。

关键 P0 资源的原始哈希可在清单中按路径查询：`Content/Naiwa/Maps/L_NaiwaGraybox.umap` 为 `b713ec2acc836fac392374c9f5598fabf350127636767a55c28950a9f3f10a7b`，`Content/Naiwa/Input/IMC_Naiwa.uasset` 为 `870464bc7cd92ce04699dd9e2c07c50cd1a6a70eddaee395ae71dad0bfa3e529`，`Content/Naiwa/Gameplay/SK_Naiwa_Elastic.uasset` 为 `a05495d621318544fca34e29327e10218d8f76808abcec08cd870ea5ea2ebdf5`。正式 E2 源 `.blend` 在 Git 中单独保存，SHA256 `317f62278d63217636bf393f831e2fb41ce21acfefbe21beed6ad6d4dc75f322`；Run 源 `.blend` SHA256 为 `040b2d413897596231e0ee82d92ab1359364f8a70802776f228185831f6dcda9`。

P0 原行为对照：A/D 或左右键移动；Space 跳跃；P/Esc 暂停；每按一次 Shift 切 Walk150/Run300，默认 Run；R 在 P0 调用 `ResetToSpawn`，解除暂停并保持当前步态。行为证据见 ZIP 中的 `gait_input_mapping.json`、`gait_acceptance.json`、最终 `summary.json`、输入日志及完成报告。最后 Shift 版本完成三轮 24/24 的 60 上限自动回归和普通窗口实际步态采样；30 上限完整通过记录早于最终 Shift 版本，不能作为其 30 上限覆盖。

本轮尚未把 P0 **新进程实际载入地图**判为通过：一次旧二进制命令返回 0 却没有生成预期日志，证据不足。等完整构建后需另用可靠启动/日志和窗口检查复核。

## 正式 SFML 第一关源素材

`UE5/SourceAssets/SFML_Level1` 中有 16 张 PNG、13 个 WAV，共 37,874,943 字节。29 个文件名都在旧 `Application.cpp` 构造器中引用；其中 14 张 PNG 与 10 个 WAV 在当前第一关绘制或播放，另 2 张背景 PNG 与 3 个 WAV 仅被构造器载入。完整保存 29 项是因为旧程序实例化时即读取这些文件，而非声称它们都应导入 UE。逐项角色、Release/Debug 来源、大小和 SHA256 见 `asset_manifest.json`。两套原始构建目录哈希相同，拷贝结果再次校验相同。后续 UE 导入只选择实际需要的资源。

采集脚本：`UE5/Scripts/capture_p1_0_baseline.ps1`（拒绝覆盖现有 ZIP/清单）与 `UE5/Scripts/copy_sfml_p1_assets.ps1`（检查源码引用、两套来源哈希及正式目录哈希；若目的文件不同则拒绝覆盖）。`UE5/run_stage.ps1` 的标准入口现在拒绝 `Setup/Import/RunImport`，已验证 `Setup` 在启动 UE 前被拒绝；直接执行旧 Python 创建/导入脚本仍须视为禁止，除非先在隔离副本审查和运行。

P1 独立地图与生成配置由 P1 关卡工作继续建立；P0 地图不得被 P1 生成脚本改写。P1-0 的可靠新进程启动与 P1 地图重跑保护验收随该工作完成后再记录。
