# P7 工程进度交接 checkpoint · 2026-11-02

## 当前结论

P7 已从运行规则/自动化骨架推进到真实 Alpha 技术数据链和打包灰盒视口：主工程 P7 自动化 14/14 通过，五张正式 DataTable 已装配到专属地图；Development 包在真实 Direct3D 12 路径启动，并生成 1280×720 实图。**还不是可玩 Alpha，也没有 G0/G1 签收。** 当前最具体的可见问题是 720p HUD 字体过小；键鼠输入还没有实测。

## 已完成

- 独立 UE Runtime 模块：`UE5/NaiwaP0/Source/NaiwaP7Runtime/`。
- 五张 P7 Alpha01 候选 CSV 已持久导入 `/Game/Naiwa/P7/Data/`：Level 1 行、Tower 9 行、Enemy 3 行、Obstacle 2 行、Wave 8 行。数据只用于技术接线，路线/经济/敌塔数值尚未获设计签收。
- `ANaiwaP7AlphaGrayboxGameMode` 连接五表；`/Game/Naiwa/P7/Maps/L_P7_Alpha01_Graybox` 已 cook/stage。
- 主工程自动化 14/14：验证真实 DataTable 装配、初始购买扣费、开始首波及首只普通敌人刷出。报告：[`P7_Automation_MainProject_2026-11-02_12-17-49.json`](evidence/P7_Automation_MainProject_2026-11-02_12-17-49.json)。
- Development cook/stage 与 P7 fixture startup smoke 通过：[`P7_AlphaGrayboxCookStage_2026-11-02_12-15-38.log`](evidence/P7_AlphaGrayboxCookStage_2026-11-02_12-15-38.log)、[`P7_MainGameCookedStartup_2026-11-02_12-16-40.json`](evidence/P7_MainGameCookedStartup_2026-11-02_12-16-40.json)。
- 真实 PCD3D_SM6 视口运行并保存截图：[`P7_AlphaGraybox_1280x720_2026-11-02.png`](evidence/P7_AlphaGraybox_1280x720_2026-11-02.png)、[`Viewport smoke JSON`](evidence/P7_AlphaGrayboxViewportSmoke_2026-11-02_12-52-21.json)、[`截图引擎日志`](evidence/P7_AlphaGrayboxScreenshot_2026-11-02_12-52-21.log)。板格、A/B 路径、8 个可见建造位、2 个障碍、奶罐/波次/金币和交互提示均已绘出。检查截图发现 HUD 字体在 720p 明显偏小。

## 下次接手优先顺序

1. 调整 [`NaiwaP7GrayboxHUD.cpp`](../../NaiwaP0/Source/NaiwaP7Runtime/Private/Presentation/NaiwaP7GrayboxHUD.cpp) 的文字缩放/布局，先保证 1280×720 可读，再复核 1920×1080。
2. 在真实视口实测 `1–3` 选塔、点击建造、点击选择塔/标障碍、`U` 升级、`C` 清目标、`N` 开波、`A/B` 波间切路、`Space` 暂停、右键取消。上次 Computer Use 桌面读屏审批超时，所以没有做这些输入验证。
3. 如运行日志显示正常但沙箱内启动崩在 `FWindowsPlatformMisc::SetStoredValue → GetMachineId → PreInit`，在当前用户桌面会话启动打包程序；该路径已在本次真实图形启动中成功。
4. 补 G0：登记项目/设计 owner、reference test PC，并由设计确认路线、经济、难度和 fixture 数值。当前 CSV 不要标成批准版。
5. 完成 A1 资源导入与游戏内比例/灰度检查，再做 Standalone/Shipping、性能和试玩回归。

## 可重复入口

- 主工程 P7 自动化：`powershell -File UE5/Scripts/run_p7_main_project_automation.ps1`
- DataTable cooked startup：`powershell -File UE5/Scripts/run_p7_main_game_cooked_smoke.ps1 -StageDirectory <Windows stage> -MapName /Game/Naiwa/P7/Maps/L_P7_Alpha01_Graybox -RequireP7Fixture`
- 范围与证据索引：[`P7 记录索引`](README.md)、[`工程开工记录`](P7_ENGINEERING_KICKOFF_2026-11-01.md)、[`源数据签收状态`](../../SourceAssets/P7/Data/README.md)。
