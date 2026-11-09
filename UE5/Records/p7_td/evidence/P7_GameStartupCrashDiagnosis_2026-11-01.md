# Development Game 启动阻断诊断

日期：2026-11-01

## 结论

主 Game 原先的 777006 已定位并修复，复验状态为 Development 启动 PASS、cook/stage 后打包启动 PASS；项目源码不需要为此修改。根因是临时 Game 构建参数 `-NoLiveCoding` 令项目 UHT 注册表结构为 32 字节，而预编译 Engine Game 模块按 40 字节步长读取，deferred class registry 因 ABI 布局不一致而错位；移除该参数后，临时 `.uproject` 生成的二进制又因内嵌工程名 `Nai_LongP7Repair_TEMP` 与运行时正式工程名不一致而进入 Fatal 路径。因此最终修复同时使用正式 `Nai_Long.uproject` clean build 并移除 `-NoLiveCoding`。无 P7 空白 Game 对照仍分别存在 MachineId 初始化和异步读取 Engine 默认资源的历史失败；它们是独立对照，不能作为已修复主 Game 或 P7 源码的证据。

## 复现与证据

- 以空的 P7BuildProbe primary module 生成 Development Game target，构建结果 Succeeded：[构建日志](P7_EmptyNoP7GameControlBuild_2026-11-01_13-37-24.log)。该工程描述只含 primary module；源码没有 NaiwaP7Runtime，也没有 USTRUCT/UCLASS/UENUM。
- 使用标准 -project、-unattended、-NullRHI、Entry map 参数启动。进程返回 0，但没有正常 Engine AbsLog；CrashContext 明确记录 EXCEPTION_ACCESS_VIOLATION，读取地址 0x0，SecondsSinceStart=0。
- 空白对照的 [CrashContext](P7_NoP7ControlCrashContext_2026-11-01.xml)、[minidump](P7_NoP7ControlCrashDump_2026-11-01.dmp) 和 [UE 日志](P7_NoP7ControlCrashLog_2026-11-01.log) 已归档。另有此前带一个空反射结构体的 [P7 probe CrashContext](P7_ProbeCrashContext_2026-11-01.xml)、[minidump](P7_ProbeCrashDump_2026-11-01.dmp) 和 [日志](P7_ProbeCrashLog_2026-11-01.log)。
- 用本次构建的 PDB 符号化后，空白对照栈为：FOutputDevice::LogfImpl → FWindowsPlatformMisc::SetStoredValue → FGenericPlatformMisc::GetMachineId → FGenericPlatformMisc::GetLoginId → FCsvProfiler::FCsvProfiler/Get/SetMetadata → FLLMGlobals::ProcessCommandLineInner → FEngineLoop::PreInitPreStartupScreen。
- 只读查询 HKCU/Software/Epic Games/Unreal Engine/Identifiers 的 MachineId 时，reg.exe 返回 ERROR: Access is denied。UE 的 SetStoredValue 在注册表写入失败后进入错误日志路径；当前异常正发生于 FOutputDevice::LogfImpl。该证据与受限会话无法访问/持久化 Unreal machine identifier 相符。具体注册表 API 返回码没有从 minidump 中恢复。

## 主 Game 复跑

- 更新了 run_p7_main_game_smoke.ps1：结束后扫描本次新写入的项目 CrashContext，归档 CrashContext/minidump/UE 日志；即使进程码为 0，只要捕获到对应 GameName 的 CrashContext，就记为 CRASH_WITH_CONTEXT。
- 更新后的脚本在主 Nai_Long.exe 上重跑，16:19 和桌面权限下的 16:31 两次均返回 777006（UE CrashDuringStaticInit）；最新的 [16:31:58 报告](P7_MainGameStartup_2026-11-01_16-31-58.json) 显示没有主 Game CrashContext、正常 UE log、地图加载或干净退出记录。
- 受限会话中的 MachineId 初始化访问冲突、桌面权限下空白 Game 的 Engine 内容异步读取断言，都不能替代主 Game 的异常栈。它们表明无 P7 的 Game 路径也失败，但与主 Game 的 777006 不同；主 Game 具体原因仍未定位，也不能据此归因 P7。

## 16:31 桌面权限复验

- 在允许 UnrealEditor-Cmd 正常运行并完成 CSV 文件导入的桌面权限条件下，以同一 smoke 脚本再次启动主 `Nai_Long.exe`。进程仍返回 `777006 / CrashDuringStaticInit`，见 [P7_MainGameStartup_2026-11-01_16-31-58.json](P7_MainGameStartup_2026-11-01_16-31-58.json)。
- 本次未生成 Engine log、项目 CrashContext、minidump 或地图加载记录；stdout/stderr 均为 0 字节。对 16:31 后的 Windows Application 1000/1001 事件只读查询没有匹配事件，因而没有获得故障模块或异常栈。
- 截至当时的桌面权限复验排除了“仅受限沙箱才会触发”这一解释；16:41 的空白 Game 桌面对照也在 UE 内容读取阶段失败，但错误与主 Game 的 777006 不同。主 Game 未定位状态于 18:49–18:54 由后续符号化和复验关闭，见本文末尾。

## 16:41 空白 Game 桌面权限对照

- `run_p7_game_static_init_control.ps1` 先因使用相对 `.uproject` 参数而得到一次无效运行（16:39 报告：工程描述未找到，退出码 1）；已修正为绝对工程路径，并在脚本清理前归档 UE 崩溃文件。
- 修正后的无 P7/无主工程 Runtime Game target 构建成功，随后通过机器标识初始化并开始加载 `/Engine/Maps/Entry`。它在 `IOThreadPool #2` 线程触发 `ReaderPos + Num <= ReaderSize` 断言，报告退出码 3、地图未加载、没有干净退出。
- [CrashContext](P7_GameInitControl_CrashContext_2026-11-01_16-41-52.xml)、[minidump](P7_GameInitControl_CrashDump_2026-11-01_16-41-52.dmp)、[UE 崩溃日志](P7_GameInitControl_CrashLog_2026-11-01_16-41-52.log)、[启动日志](P7_GameInitControlStartup_2026-11-01_16-41-52.log)和[报告](P7_GameInitControl_2026-11-01_16-41-52.json)已归档。控制工程退出后已清理。
- 该失败来自不含 P7 代码的最小 Game 对照，说明当前会话的 UE Game 内容读取链也存在独立故障；它与主 Game 的 777006 不同，不能作为主 Game 异常栈，也不能把主 Game 问题归因到 P7。

## 18:04–18:09 Engine 资源异步读取差分

- 在最小 Game 对照上加入 `-NoAsyncLoadingThread` 后，日志确认 Async Loading Thread 为 false，但 `IOThreadPool #2` 仍触发相同 `FBufferReaderBase::Serialize` 越界断言（[报告](P7_GameInitControl_2026-11-01_18-04-56.json)、[日志](P7_GameInitControlStartup_2026-11-01_18-04-56.log)）。
- 移除显式 `-Map=/Engine/Maps/Entry` 参数后，仍在同一 IO 线程退出 3（[报告](P7_GameInitControl_2026-11-01_18-06-10.json)、[CrashContext](P7_GameInitControl_CrashContext_2026-11-01_18-06-10.xml)）。因此失败不只由显式 Entry 地图加载触发。
- Streaming/AsyncLoading/IoDispatcher VeryVerbose 日志显示，断言前刚开始读取 `/Engine/EngineResources/WhiteSquareTexture`；前面队列中还有 WorldGrid、DefaultDeferredDecal、DefaultLightFunction 和 DefaultPostProcess 等引擎材质包。[详细运行报告](P7_GameInitControl_2026-11-01_18-07-38.json)、[调用栈日志](P7_GameInitControlStartup_2026-11-01_18-07-38.log)。
- 单独通过 UnrealEditor-Cmd 读取同一 `/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture` 对象成功，返回 `Texture2D` 且进程码为 0（[报告](P7_EngineAssetReadProbe_2026-11-01_18-09-44.json)、[Editor log](P7_EngineAssetReadProbe_2026-11-01_18-09-44.log)）。这证明 Editor 侧可读取该源资产；Game target 的异步包读取路径仍失败。该差分不能单独证明物理源文件损坏或主 Game 的 777006 与此同因。
- 18:09:06 的首次 Editor 读包探针在成功取得对象后访问了 Python API 未暴露的 `size_x` 属性而报错（[失败探针报告](P7_EngineAssetReadProbe_2026-11-01_18-09-06.json)）；修正为只检查对象存在与 `Texture2D` 类型后，18:09:44 复验通过。该首次报错属于探针断言问题。
## 对早期差分结果的更正

先前若干 probe 仅以 Start-Process 的退出码和外部 AbsLog 路径判断。部分进程返回 0，但 UE 实际把异常日志与 CrashContext 写到了工程 Saved/Crashes，因此“返回 0/无 AbsLog”不是干净启动。按结构体数量做的差分不能用于归因，P7 源码没有因此修改。以上是 18:10 以前的调查状态；主 Game 的后续原生调试、根因和最终复验见下节。

## 下一步

主 Game 启动阻断已关闭。剩余工程验收针对 P7 内容本身：建立生产 DataTable 与玩法 fixture、接入 P7 地图及运行时装配，运行 PIE/Standalone/Shipping 和实际输入/画面验证；这些内容未包含在本次主菜单启动 smoke 中。

## 18:26–18:54 根因定位与修复复验

- 原生调试捕获表明，旧 Game 二进制在 `TDeferredRegistry<TRegistrationInfo<UClass,FClassReloadVersionInfo>>::AddRegistration` 内因错误注册记录访问崩溃；项目生成的 class info 表行长为 32 字节，而预编译 Engine Game 注册数组按 40 字节读取，第二行起指针字段错位。调用现场、行内容和数组步长分别见[注册调用现场](P7_MainGameRegistrationBreakpoint_2026-11-01_18-43-00.txt)、[注册表数据表](P7_MainGameClassInfoTable_2026-11-01_18-50-00.txt)和[数组步长检查](P7_MainGameRegistryArrayStrides_2026-11-01_19-11-00.txt)。Game build script 之前带 `-NoLiveCoding`，使项目侧宏布局与 Engine 不一致；移除该参数后该布局问题关闭。
- 修复布局后用临时 `Nai_LongP7Repair_TEMP.uproject` 构建的中间二进制仍发生写地址 `0x10` 的异常。CrashContext 和符号栈将位置指向 `LaunchSetGameName` 的 Fatal 处理，CrashContext 的 `GameName` 为 `UE-Nai_LongP7Repair_TEMP`，而 smoke 运行正式 `Nai_Long.uproject`；证据见[项目名调试记录](P7_MainGameProjectArgumentDebug_2026-11-01_19-27-00.txt)。因此最终脚本直接用正式描述符构建。
- 最终全量 Game rebuild 使用正式 `Nai_Long.uproject`，96/96 动作成功，日志为 [P7_MainNaiLongGameBuild_2026-11-01_18-50-23.log](P7_MainNaiLongGameBuild_2026-11-01_18-50-23.log)。最终 Game target 的 `WITH_LIVE_CODING=1` 和 UHT 注册表行已另存为[布局快照](P7_MainGameFinalBuildLayout_2026-11-01.txt)。[Development smoke](P7_MainGameStartup_2026-11-01_18-52-03.json)返回码 0，报告确认 `/Game/Naiwa/UI/L_NaiwaMainMenu` 加载且引擎干净退出，没有 CrashContext。
- 正式工程 cook/pak/stage 成功，日志见 [P7_MainGameCookStage_Final_2026-11-01_18-52-37.log](P7_MainGameCookStage_Final_2026-11-01_18-52-37.log)；[打包 smoke](P7_MainGameCookedStartup_2026-11-01_18-54-22.json)同样确认主菜单加载、进程码 0 和干净退出。
- 为保留可复测入口，新增 [`build_p7_main_game_probe.ps1`](../../Scripts/build_p7_main_game_probe.ps1) 的正式描述符 clean build 选项，以及 [`run_p7_main_game_cooked_smoke.ps1`](../../Scripts/run_p7_main_game_cooked_smoke.ps1) 打包启动 smoke。该验证只覆盖主菜单启动，不代表 P7 Alpha 或 P7 地图玩法验收。

