# P7 工程开工记录 · 2026-11-01

状态（2026-11-02 更新）：工程基础、E1 模拟骨架、E2 核心规则及 E4 灰盒 HUD/输入已落地；P7-only 与真实主工程内 14/14 自动化通过。5 张 `P7_Alpha01` 源 CSV 已经 CSVImportFactory 导入为持久 DataTable；自动化在正式工程内通过 C++ 装配器校验，并将真实 fixture 加入模拟器，验证购买塔、启动第一波及首个敌人刷出。`L_P7_Alpha01_Graybox` 地图已创建并设置 P7 专用 GameMode；Development cook/pak/stage 后确认地图加载和五表装配。真实 Direct3D 12 视口启动并运行后以 UE `HighResShot` 生成 1280×720 截图，板面和主要灰盒信息均已绘出；视觉检查发现 720p HUD 字体偏小，键鼠尚未实测（桌面读屏审批超时）。截图与运行报告见[视口证据](evidence/P7_AlphaGrayboxViewportSmoke_2026-11-02_12-52-21.json)，完整交接见[checkpoint](P7_PROGRESS_CHECKPOINT_2026-11-02.md)。fixture 是待 G0 签收的技术验证候选；实际 owner、参考测试 PC、经济/难度和数值尚未确认，G0/G1 未签收。A1 游戏内尺度、Standalone/Shipping、性能/试玩仍待验收。主 Game 旧启动 ABI 与临时工程名问题已在 2026-11-01 闭环，见[启动诊断](evidence/P7_GameStartupCrashDiagnosis_2026-11-01.md)。

## 模块隔离决策

当前 `NaiwaRuntime` 是 UE 主模块，P4、P5 及既有主线代码都编入同一模块。仅把新代码放在 `NaiwaRuntime/P7/` 子目录不能隔离 UE 模块编译，也容易让 P7 增量构建受其他玩法源文件牵连。

按项目负责人 2026-11-01 的指示，P7 使用独立的 `NaiwaP7Runtime` UE Runtime 模块。该模块不依赖 `NaiwaRuntime`，不包含 P4/P5 头文件；主游戏和 Editor target 分别注册此模块。后续如需与主菜单或全局存档集成，必须在 Gamma 另行评审一个窄接口，不能把 P7 模拟代码并入 P4/P5。

此项是对综合计划书 v1.1 §5.1 建议源码路径的工程隔离细化；玩法范围和资产边界不变。新增的构建回归需覆盖 `Nai_LongEditor` 和 Shipping target，确认模块注册、cook 与模块装载。

## 本批交付

- 独立 P7 Runtime 模块注册和 Build.cs。
- 20×11 逻辑板坐标映射，按 BoardRect 比例换算，不依赖全屏像素尺寸。
- Alpha 关卡数据校验：A/B 路线连续且共用端点、8 个可见建造位、2 个障碍解锁位，所有位置在界内并避开路线。
- P7 独立权威模拟器与 `UNaiwaP7TowerDefenseSubsystem`：波间切路、波次状态、整数定步敌人移动、漏怪扣奶罐耐久、只读快照和状态哈希。
- 固定 1/60 秒更新；每帧最多追赶 4 步，剩余积压保留并可观测。
- DataTable 驱动的八波刷怪计划：按首刷延迟和组内间隔定步生成敌人；仍有待刷敌人或存活敌人时不能结算本波。
- 增加可导入的 Level DataTable 行：两条路线、共享干扰节点、可见/隐藏建造位、障碍引用和初始金币都来自表；四类玩法表与 Level 表由装配器检查行结构、稳定排序并整体校验，失败不覆写当前 fixture。子系统可直接从这些表加载。
- 建塔/升级的建造位、等级、金币校验；关卡快照包含塔、障碍、金币和刷怪队列，状态哈希包含规则表与战斗状态。
- 确定性塔战斗：按距离筛选并固定优先目标，支持单体伤害、范围伤害、减速、定点攻击障碍、清障奖励和隐藏建造位解锁。障碍清理也可在准备阶段推进。
- 按 v1.1 落地索敌链“最靠近终点 → 距离最近 → EntityId 最小”；干扰敌越过两条预设路线共享的固定内部节点后，按触发点距离封锁最近塔 90 步，同距按稳定 CellId 决胜，敌人最多触发一次且效果不叠加。
- 固定步顺序已调整为刷怪 → 敌人移动/干扰 → 塔攻击与伤害 → 漏怪结算；干扰剩余步数进入快照和状态哈希，暂停时通过“不推进固定步”保持计时。
- `UNaiwaP7TowerDefenseSubsystem` 暴露加载 fixture、建塔、升级、刷怪和障碍目标接口；暂停时不推进模拟。
- 新增 `ANaiwaP7GrayboxHUD` Canvas 样片与 P7 专用 `ANaiwaP7GrayboxGameMode` / `ANaiwaP7GrayboxPlayerController`：HUD 从模拟快照绘制 A/B 路线、建造位、障碍、塔、敌人、奶罐/波次/金币和干扰倒计时；玩家控制器使用 P7 自有 Enhanced Input context，支持点击放塔/选塔/标障碍、右键取消选择、1–3 选塔、U 升级、C 清除障碍目标、N 开波、A/B 波间切路、Space 暂停。控制器在 `SetupInputComponent` 中确保使用 `UEnhancedInputComponent`，不依赖主工程全局输入组件默认值；`GameAndUI` 输入模式在鼠标捕获时保留指针。GameMode 可配置五张玩法 DataTable 与 LevelRowName，启动时调用现有校验装配器；未配置表时保持等待态。板面采用 1920×1080 基准比例居中，自动化用例源码覆盖 1920×1080 与 1280×720 的同 CellId 映射。
- Editor 自动化用例覆盖 1080p/720p 对应 BoardRect 映射、无效地图、DataTable 装配与错误行类型拒绝、Tower/Enemy/Wave/Obstacle 表校验、路线锁定、长路线移动、奶罐归零、八波结算、刷怪延迟/间隔、建塔升级、障碍解锁、单体/范围/减速攻击、干扰单次触发/稳定目标/不叠加/90 步恢复、重复输入哈希一致和追赶积压。另有 `Naiwa.P7.Data.CsvArrayImport` 覆盖 Level 与 Wave 的数组导入形态。

截至 2026-11-01 的阶段快照：路线与塔/敌数值尚待设计签收，生产关卡 fixture、持久 DataTable 和可配置地图尚未验收；HUD/GameMode/PlayerController 只有可编译灰盒实现，尚无运行截图或 Editor 视觉/输入验收。九张塔图已作为 Texture2D 导入，但尚未做游戏内尺度/灰度检查；投射物表现和 UMG 成品仍未完成。DataTable 的内存 CSV 数组解析自动化已通过，真实源 CSV/UE DataTable 资产的创建与 Editor 重导入尚未验收。战斗表现仍由模拟规则结算。以上是本记录下方 2026-11-02 工程进度之前的历史状态；当前状态以文首和最新进度条目为准。

## 编译核验

- UE 5.8.3 下通过 [`build_p7_isolated_probe.ps1`](../../Scripts/build_p7_isolated_probe.ps1) 分别生成仅含 `NaiwaP7Runtime` 与空主模块的临时 Editor/Game target；两个 target 均为 21/21 个动作、结果 `Succeeded`。完整输出见 [Editor 日志](evidence/P7_IsolatedEditorBuild_2026-11-01_11-05-10.log) 和 [Game 日志](evidence/P7_IsolatedGameBuild_2026-11-01_11-04-19.log)。每次只编一个 target，探针不含 P4/P5，运行后自动清理。
- 上述 11:16/11:51 自动化启动失败记录保留为历史；12:35 修复脚本参数传递后，隔离 Editor 自动化实际通过 13/13；12:40 在真实 `Nai_Long.uproject` 的 Editor 命令行内再次通过 13/13。两次均有逐项 `Test Completed` 日志，包括 `Naiwa.P7.Data.CsvArrayImport`。日志与根因见[自动化尝试记录](evidence/P7_CsvArrayImport_AutomationAttempt_2026-11-01.md)。
- 只读灰盒 HUD 与分辨率布局回归已通过 P7-only Editor 编译，23/23 个动作成功（[编译日志](evidence/P7_IsolatedEditorBuild_2026-11-01_11-20-57.log)）；测试源码已编译，但 HUD 和自动化都没有运行时日志/截图，不能记为视觉或运行时通过。
- P7 Enhanced Input 操作和 DataTable 配置入口加入后，补齐 `InputCore` 依赖并显式启用 Engine EnhancedInput 插件。随后一次错误尝试使用了 UE 5.8 中不存在的 `InputComponentClass` 属性（[失败日志](evidence/P7_IsolatedEditorBuild_2026-11-01_11-41-37.log)）；改为在 `SetupInputComponent` 创建 Enhanced 组件后，P7-only Editor 25/25 动作成功（[最新 Editor 日志](evidence/P7_IsolatedEditorBuild_2026-11-01_11-51-01.log)），Game 25/25 动作成功（[Game 日志](evidence/P7_IsolatedGameBuild_2026-11-01_11-44-18.log)）。该探针只编译 `NaiwaP7Runtime` 与空主模块 `P7BuildProbe`，不依赖 `NaiwaRuntime`，不含 P4/P5 源码。
- 新增 [`build_p7_main_editor_probe.ps1`](../../Scripts/build_p7_main_editor_probe.ps1)，用原 `.uproject` 的临时同目录副本生成独立 Rules 程序集，避免覆盖当前被 UBT 报告占用的 `Nai_LongModuleRules.dll`。复跑结果为 `Nai_LongEditor` target up to date、`Result: Succeeded`（[探针日志](evidence/P7_MainNaiLongEditorBuild_AltRules_2026-11-01_12-42-05.log)）；临时 `.uproject` 已清理，已编译产物保留供增量构建。
- 历史状态（16:31）：临时工程描述构建的主 Nai_Long Development Game target 完成 94/94 动作，但主菜单启动以 777006 退出；当时未生成 CrashContext 或 Engine log（[旧报告](evidence/P7_MainGameStartup_2026-11-01_16-31-58.json)）。该问题已由 18:49–18:54 的符号与布局证据定位并修复，最终结果见下方。
- 隔离 P7 Game probe 的早期报告为 25/25 编译、启动退出 777006（[启动报告](evidence/P7_IsolatedGameStartup_2026-11-01_12-58-00.json)）。早先在受限会话中，一个不含 NaiwaP7Runtime、也不含 USTRUCT 的空 Game 对照在 MachineId 初始化路径发生访问冲突；该会话只读注册表查询被拒绝。桌面权限下修正参数后的最小 Game 对照通过 MachineId 初始化，却在异步读取 Engine 默认地图时触发 BufferReader 越界断言（退出码 3；[CrashContext](evidence/P7_GameInitControl_CrashContext_2026-11-01_16-41-52.xml)、[minidump](evidence/P7_GameInitControl_CrashDump_2026-11-01_16-41-52.dmp)、[报告](evidence/P7_GameInitControl_2026-11-01_16-41-52.json)）。这些无 P7 对照错误均不同于主 Game 的 777006，不能替代主 Game 栈或作为 P7 归因。此前“返回 0、没有 AbsLog”的对照曾被记为无结论；现在已从 Saved/Crashes 找到其崩溃上下文，故不再把此类结果视作干净启动或结构体差分证据。最初对照尝试曾误用受限的 ProgramData UBA 路径，已中止并修正为临时工程内目录。
- 18:04–18:07 最小 Game 对照在关闭 Async Loading Thread、移除显式地图参数后仍于 `IOThreadPool #2` 触发相同断言；VeryVerbose 日志把首个失败对象定位到 `/Engine/EngineResources/WhiteSquareTexture`（[详细日志](evidence/P7_GameInitControlStartup_2026-11-01_18-07-38.log)）。18:09 UnrealEditor-Cmd 单独加载同一对象成功并读回为 Texture2D（[报告](evidence/P7_EngineAssetReadProbe_2026-11-01_18-09-44.json)）。当前证据指向 Game 异步读取路径差异，但还不能证明主 Game 的 777006 与此同因；完整边界见[启动诊断](evidence/P7_GameStartupCrashDiagnosis_2026-11-01.md)。
- P7 真实 CSV 文件导入现已验收：LevelRow/WaveRow fixture 经 CSVImportFactory 导入、保存并读回数组/嵌套字段，临时资产清理通过（[最新运行器报告](evidence/P7_DataTableCsvRunner_2026-11-01_16-37-29.json)、[读回报告](evidence/P7_DataTableCsvProbe_2026-11-01_16-37-29.json)）。Engine log 另记录 13 条未归因的 AutomationTest 条件错误和 2 条临时资产删除警告；相同错误行也见于先前 13/13 P7 自动化日志，不能单独说明本次导入失败，详情见[CSV 导入记录](evidence/P7_CsvArrayImport_AutomationAttempt_2026-11-01.md)。这些是导入验证 fixture，不是生产关卡数据；生产 DataTable、运行时装配和地图仍未验收。此前静默退出/无日志的运行记录保留为历史。
- A1 九张已批准炮塔纹理的导入脚本完成幂等复核：报告显示源预检 9/9、九张均为同源既有 Texture2D、纹理参数读回全部通过（[导入复核报告](evidence/P7_A1_ApprovedTowerImport_2026-11-01_12-42-32.json)）。脚本未打开地图或运行 PIE；游戏内尺寸和视觉仍待测。
- 曾在同一临时工程连续切换 Editor→Game 时遇到 UBT/.NET 启动异常（未进入 Game C++ 编译）；两个 target 改为各自新建探针后均单独通过。失败输出也保留在 `evidence/` 供追溯。
- 原始 `Nai_LongEditor` 的早期构建曾因 BuildRules 文件锁失败；后用临时描述完成 Editor 构建以保留当时的共享产物。该早期锁定记录仍在[失败日志](evidence/P7_MainNaiLongEditorBuild_2026-11-01_12-42-00.log)；18:49 对正式 `Nai_Long.uproject` 执行全量 Game rebuild 已成功。2026-11-02 的 P7 Alpha 数据表与专属地图随后已 cook/stage 并由打包 Development 验证；Standalone、Shipping、PIE 视觉/输入、性能和试玩仍未验收。
- 18:49–18:54 主 Game 启动根因闭环：移除 Game target 构建参数 `-NoLiveCoding`，并改用正式 `Nai_Long.uproject` 全量重编。前者让项目生成的 `FClassRegisterCompiledInInfo` 行采用 32 字节布局，而预编译 Engine Game 注册表按 40 字节步长读取，造成 deferred class registry 初始化时错位访问；临时 `.uproject` 又把构建内嵌项目名设成 `Nai_LongP7Repair_TEMP`，运行时以正式项目名启动会进入错误 Fatal 路径。布局和调用现场记录见[数组步长](evidence/P7_MainGameRegistryArrayStrides_2026-11-01_19-11-00.txt)、[注册表数据表](evidence/P7_MainGameClassInfoTable_2026-11-01_18-50-00.txt)、[注册调用现场](evidence/P7_MainGameRegistrationBreakpoint_2026-11-01_18-43-00.txt)和[临时项目名异常](evidence/P7_MainGameProjectArgumentDebug_2026-11-01_19-27-00.txt)。修复后的正式构建为 96/96 动作成功（[构建日志](evidence/P7_MainNaiLongGameBuild_2026-11-01_18-50-23.log)）；[Development smoke](evidence/P7_MainGameStartup_2026-11-01_18-52-03.json)通过。cook/pak/stage 成功（[日志](evidence/P7_MainGameCookStage_Final_2026-11-01_18-52-37.log)），[打包启动 smoke](evidence/P7_MainGameCookedStartup_2026-11-01_18-54-22.json)通过；两个 smoke 均确认 `L_NaiwaMainMenu` 加载且引擎干净退出。
- 2026-11-02 P7 Alpha 数据与地图验证：新增单关候选源数据目录 [`SourceAssets/P7/Data`](../../SourceAssets/P7/Data/README.md) 及 CSV 导入脚本，5 张表均由 CSVImportFactory 持久导入（[报告](evidence/P7_AlphaFixtureImport_2026-11-02_12-10-27.json)）。Production fixture 自动化使用正式 DataTable 运行现有 C++ 校验器，并把数据载入 `FSimulation`，验证可购塔、350 起始金币扣费、第一波队列和第一个 `P7_Enemy_Normal` 刷出；全部 P7 14/14 通过（[主工程自动化报告](evidence/P7_Automation_MainProject_2026-11-02_12-17-49.json)）。新增 `ANaiwaP7AlphaGrayboxGameMode` 和地图 `L_P7_Alpha01_Graybox`，地图 GameMode 指向该类（[创建报告](evidence/P7_AlphaGrayboxMapCreation_2026-11-02_12-13-45.json)）；未 cook 的 Development Game 无法读取新资产，因此以正式 `BuildCookRun -map=/Game/Naiwa/P7/Maps/L_P7_Alpha01_Graybox` 路径验收，cook/stage 成功（[日志](evidence/P7_AlphaGrayboxCookStage_2026-11-02_12-15-38.log)），打包版返回码 0，确认地图加载、五表装配、干净退出（[报告](evidence/P7_MainGameCookedStartup_2026-11-02_12-16-40.json)）。CSV 数值仍是技术验证候选，测试 PC、责任人及设计签收待补；本次不记 G0/G1 完成或可玩 Alpha。
- 2026-11-02 真实图形视口检查：沙箱内进程的早期崩溃经 PDB 符号化定位到 Unreal `GetMachineId → SetStoredValue → PreInit`，因此通过用户桌面会话运行本地包后正常进入 Direct3D 12 (`PCD3D_SM6`)；加载同一灰盒地图、五张 DataTable 并以正常退出完成运行。调用 `HighResShot 1280x720` 保存截图（[截图](evidence/P7_AlphaGraybox_1280x720_2026-11-02.png)、[运行报告](evidence/P7_AlphaGrayboxViewportSmoke_2026-11-02_12-52-21.json)、[引擎日志](evidence/P7_AlphaGrayboxScreenshot_2026-11-02_12-52-21.log)）。视觉确认布局、路线、建造位和障碍已绘制；720p 字体明显偏小，需改 HUD 缩放/间距并回归 720p 与 1080p。桌面读屏审批超时，未执行鼠标/键盘输入验收。本进度不表示可玩 Alpha 或阶段门签收。





