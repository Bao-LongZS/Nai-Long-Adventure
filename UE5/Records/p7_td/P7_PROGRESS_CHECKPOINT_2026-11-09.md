# P7 工程进度交接 checkpoint · 2026-11-09

> 历史快照。当前进度以[2026-11-11 checkpoint](P7_PROGRESS_CHECKPOINT_2026-11-11.md)和[G0 依赖登记表](P7_G0_DEPENDENCY_REGISTER_2026-11-11.md)为准。

## 当前结论

P7 Alpha01 灰盒已达到可重复编译、cook、启动、真实 RHI 截图、输入事件驱动与自动化验证的技术切片阶段。最终 Development Game 与 Editor 编译通过；最终 Cooked 包在 1280×720 窗口与 1920×1080 全屏各有一张完整截图；P7 隔离探针与主工程最新各 16 项自动化均通过。新增测试通过 Enhanced Input 注入动作、控制器绑定和 P7 子系统，驱动选塔→主鼠标建造→升级→障碍指定/清除→选路线→开波→战斗中锁路→暂停/恢复。格子位置由 `WITH_DEV_AUTOMATION_TESTS` 专用输入替身提供，所以这证明了 UE action 到游戏命令的闭环，不等同于桌面设备的真实鼠标坐标试玩。Computer Use 后端初始化及重置后仍返回“系统找不到指定的路径”，实际设备级试玩仍待补。

当前不能称为可玩 Alpha：G0 的负责人、参考测试 PC 和 fixture 平衡值签收仍未落实；正式 A1 资源导入、Standalone/Shipping、性能和试玩也待做。

## 本轮完成

- HUD 720p 字号底线提高到参考 1080p 大小；增大塔选项、底栏操作说明和状态文本；障碍标签由长 ID 改为 `CrA 90` / `CrB 130` 并放回障碍块内部，避免压住建造位。
- 主工程 Game 构建通过：[`P7_MainNaiLongGameBuild_2026-11-09_10-29-02.log`](evidence/P7_MainNaiLongGameBuild_2026-11-09_10-29-02.log)。
- 主工程 Editor 构建通过：[`P7_MainNaiLongEditorBuild_AltRules_2026-11-09_10-25-42.log`](evidence/P7_MainNaiLongEditorBuild_AltRules_2026-11-09_10-25-42.log)。
- 最终 Development cook/stage 通过，Pak 收录 10,674 项：[`P7_AlphaGrayboxCookStage_2026-11-09_10-29-56.json`](evidence/P7_AlphaGrayboxCookStage_2026-11-09_10-29-56.json)。
- 全工程 cook 同时记录了一条非 P7 警告：P4 `M_P4_W1_SoftShadow` 材质缺少 Saturate 输入，Cook 对该材质使用 Default Material；这不影响 P7 灰盒地图的验证，但应由 P4 后续清理。
- 最终 Cooked Startup 通过：目标地图加载、五张 DataTable fixture 装配、`LogExit: Exiting.` 均有记录：[`P7_MainGameCookedStartup_2026-11-09_10-39-41.json`](evidence/P7_MainGameCookedStartup_2026-11-09_10-39-41.json)。进程对象未返回可读的退出码，但脚本确认它在超时前结束且引擎记录 clean exit。
- 最新隔离 P7 后台探针编译通过，自动化 16/16，测试进程退出码 0：[`P7_IsolatedEditorBuild_2026-11-09_11-05-34.log`](evidence/P7_IsolatedEditorBuild_2026-11-09_11-05-34.log)、[`P7_Automation_2026-11-09_11-05-34.log`](evidence/P7_Automation_2026-11-09_11-05-34.log)。`RuntimeInputBindings` 检查 11 项鼠标/键位到 action 的映射；`RuntimeInputDispatch` 通过 UE Enhanced Input 事件注入驱动完整逻辑闭环，并验证建造扣币、升级等级、障碍目标、波次阶段、路线锁定和暂停状态。
- 最新主工程 Editor 编译通过：[`P7_MainNaiLongEditorBuild_AltRules_2026-11-09_11-07-20.log`](evidence/P7_MainNaiLongEditorBuild_AltRules_2026-11-09_11-07-20.log)。主工程自动化 16/16：[`P7_Automation_MainProject_2026-11-09_11-09-22.json`](evidence/P7_Automation_MainProject_2026-11-09_11-09-22.json)，报告状态 `PASS`、退出码 0、无缺项/失败。脚本会从 Unreal 的 `RequestExitWithStatus(..., 0)` 补读进程对象未提供的退出码。
- 最新主工程 Development Game 编译通过：[`P7_MainNaiLongGameBuild_2026-11-09_11-10-27.log`](evidence/P7_MainNaiLongGameBuild_2026-11-09_11-10-27.log)。新增改动限于自动化测试与 `WITH_DEV_AUTOMATION_TESTS` 下的逻辑格输入替身；运行时正式输入映射和数据未变，既有 cook/stage/startup 与 720p/1080p RHI 证据继续适用。
- 当前代码新 Cook/Stage 与打包回归通过：Pak 收录 10,674 项，[Cook/Stage 报告](evidence/P7_AlphaGrayboxCookStage_2026-11-09_11-12-44.json)；目标地图加载、五表 fixture 装配、clean exit 均通过，[Cooked Startup 报告](evidence/P7_MainGameCookedStartup_2026-11-09_11-14-00.json)。
- 当前 stage 的实图通过 RHI 捕获与分辨率校验：1280×720 [PNG](evidence/P7_AlphaGraybox_1280x720_2026-11-09_11-14-16.png)/[报告](evidence/P7_AlphaGrayboxViewport_1280x720_2026-11-09_11-14-16.json)，1920×1080 全屏 [PNG](evidence/P7_AlphaGraybox_1920x1080_2026-11-09_11-14-36.png)/[报告](evidence/P7_AlphaGrayboxViewport_1920x1080_2026-11-09_11-14-36.json)。画面复核确认 720p 的建造位、障碍简码和操作说明可见。
- 新增 [G0 依赖登记表](P7_G0_DEPENDENCY_REGISTER_2026-11-09.md)：记录未实名角色/投入窗口、参考测试 PC 的候选环境信息、五表 fixture 的候选范围与 A0 样片输入。参考候选环境从最新 RHI 日志取得 Windows 11 25H2、Core Ultra 9 275HX、RTX 5070 Ti Laptop/11.6GB 显存、驱动 572.84、UE 5.8.3；构建日志记录约 31.47GB RAM。负责人尚未确认其为正式 reference PC，G0 仍待签。
- 1280×720 窗口实图：[`P7_AlphaGraybox_1280x720_2026-11-09_10-40-53.png`](evidence/P7_AlphaGraybox_1280x720_2026-11-09_10-40-53.png) / [报告](evidence/P7_AlphaGrayboxViewport_1280x720_2026-11-09_10-40-53.json)。
- 1920×1080 全屏实图：[`P7_AlphaGraybox_1920x1080_2026-11-09_10-39-05.png`](evidence/P7_AlphaGraybox_1920x1080_2026-11-09_10-39-05.png) / [报告](evidence/P7_AlphaGrayboxViewport_1920x1080_2026-11-09_10-39-05.json)。该报告的 `screenshotLogged` 为 false，但全分辨率 PNG 已写盘并经尺寸和画面检查；Startup smoke 单独验证了正常引擎退出。
- 为让既有未提交 P6 源码进入主工程构建，最小调整了 `NaiwaRuntime.Build.cs` 的 P6 include paths，并修复两处 P6 编译错误（`FString` 初始化类型和 `Controller` 遮蔽）。这些是主工程构建阻塞处理，不是 P7 功能验收。
- 新增/修正了可复验入口：`UE5/Scripts/run_p7_alpha_graybox_viewport_smoke.ps1`、`UE5/Scripts/cook_p7_alpha_graybox.ps1`、`UE5/Scripts/run_p7_main_project_automation.ps1`、`UE5/Scripts/run_p7_main_game_cooked_smoke.ps1`。Cook 日志现在使用统一 UTF-8；视口烟测把实图捕获与进程关闭分开报告。

## 键鼠映射和限制

运行时代码通过 Enhanced Input 映射左/右键、`1–3`、`U`、`C`、`N`、`Space`、`A/B`；最新 16 项测试已注入这些 action，并覆盖“选塔→主键点击建造→升级→点击障碍标记/清除→选路→开波→战斗锁路→暂停/恢复”。注入测试用开发测试专用逻辑格替身，不触碰真实视口鼠标坐标；所以实体设备级输入仍未验收。本轮在新打包程序上再次尝试 Computer Use，仍返回“系统找不到指定的路径”；此前初始化和重置重试也遇到相同错误。

## 下一阶段入口

1. 在 Computer Use 可用时完成实体键鼠/真实屏幕坐标点击闭环，并保存状态前后截图。
2. 请 G0 owner 确认参考测试 PC、路线/经济/难度以及五张 DataTable 数值；目前 CSV 只作技术 fixture。
3. 正式 A1 资源通过尺寸/灰度/层级验收并导入 UE，再做 Standalone/Shipping、性能和试玩。

## 可重复命令

- 隔离 P7 编译与 16 项自动化：`powershell -File UE5/Scripts/build_p7_isolated_probe.ps1 -Target Editor -RunAutomationTests`
- 主工程 P7 自动化：`powershell -File UE5/Scripts/run_p7_main_project_automation.ps1`
- Cook/stage：`powershell -File UE5/Scripts/cook_p7_alpha_graybox.ps1`
- Cooked Startup：`powershell -File UE5/Scripts/run_p7_main_game_cooked_smoke.ps1 -StageDirectory <Windows stage> -MapName /Game/Naiwa/P7/Maps/L_P7_Alpha01_Graybox -RequireP7Fixture`
- 1280×720 RHI 图像：`powershell -File UE5/Scripts/run_p7_alpha_graybox_viewport_smoke.ps1 -StageDirectory <Windows stage> -Width 1280 -Height 720`
- 1920×1080 全屏 RHI 图像：上述命令改用 `-Width 1920 -Height 1080 -Fullscreen`。
