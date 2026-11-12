# P7 工程进度交接 checkpoint · 2026-11-12 更新

## 当前结论

P7 Alpha01 灰盒已有可复验的技术切片：2026-11-11 主工程与隔离工程既有后台自动化均为 16/16 通过；隔离 Editor 探针 25/25 构建动作成功。地图现接入 A1 VFX、炮塔九图、双路线、中文 HUD、鼠标按钮、批准 HUD 图标、A0 地面底图、奶蛋终点、图录姿态和分岔门。2026-11-12 已完成画幅、建造提示、敌速、击杀金币、炮位距离与清障反馈调整：Game 编译、关卡单表重导入、最新 Cook/Stage 和 1280×720 RHI 启动捕获均通过，P7 地图与五张表正常加载。击杀及清障实操、放大后炮塔的视觉尺寸仍待现场试玩；本轮未运行自动化测试。

当前视口截图验证了地图构图和 UI 资源：显示逻辑地图第 4–10 行，逻辑坐标仍是原有 20×11；全板细网格已移除，只在可建造炮位显示格子提示；路线条带取消逐格接缝，炮塔实例绘制尺寸调至 1.18 格宽。普通/快速/干扰敌速调整为 2.4/4.5/1.8 格/秒并通过敌人 DataTable 回读。普通炮位已从逻辑 Y=8 移到 Y=7，障碍解锁炮位从 Y=9 移到 Y=8，路线保持 Y=4/5；新截图处于布防阶段，能确认新炮位格提示和新地图布局，但未实际放塔、开波或观察金币/清障反馈。实体键鼠点击验证仍待完成；G0 负责人、reference PC 与 fixture 平衡签收、其余 A1 正式资源、Standalone/Shipping 与性能验收也仍待完成。

## 2026-11-12 击杀金币与炮位距离

- 模拟结算现在会在敌人生命降至 0 并移除前，根据敌种增加金币：普通 +10、快速 +15、干扰 +20。HUD 观察快照金币数值的正向变化，并在金币计数旁短暂显示 `+N 金币`。实现位于 `NaiwaP7Simulation.cpp` 与 `NaiwaP7GrayboxHUD.cpp`；Game 构建通过：[击杀金币反馈构建日志](evidence/P7_MainNaiLongGameBuild_DirectUBT_KillGoldFeedback_2026-11-12.log)。这次未进行实体键鼠击杀验证，数值作为工程候选待平衡签收。
- 为缩短炮塔与路线间距，关卡 CSV 将 8 个普通炮位由 Y=8 移至 Y=7，将清障解锁的两个隐藏炮位由 Y=9 移至 Y=8；两处障碍仍在 Y=9。DataTable 单表重导入/回读通过：[炮位回读报告](evidence/P7_LevelBuildSitesReimport_2026-11-12_10-41-39.json)。
- 最新 Cook/Stage 成功：[报告](evidence/P7_AlphaGrayboxCookStage_2026-11-12_10-46-39.json)。真实 RHI 1280×720 截图通过，并确认 P7 地图与 fixture 加载：[截图](evidence/P7_AlphaGraybox_1280x720_2026-11-12_10-48-04.png)、[报告](evidence/P7_AlphaGrayboxViewport_1280x720_2026-11-12_10-48-04.json)。截图处于布防阶段，只验证布局与加载，不代表击杀金币的运行时实操验收。
- 后续反馈指出清箱时反馈不明显。检查发现零伤害的奶鼠减速塔此前仍可设置清障目标，会造成目标存在但箱子永不掉血；现已禁止此无效目标并显示原因。有效伤害炮塔更换清障目标时重置攻击冷却，首击立即生效；HUD 增加炮塔至目标箱的连线、目标高亮和更醒目的耐久条。棋盘炮塔纹理尺寸由 1.03 格调至 1.18 格。Game 构建与 Cook/Stage 均通过：[Game 构建日志](evidence/P7_MainNaiLongGameBuild_DirectUBT_ClearBoxAndTowerScale_2026-11-12.log)、[Cook/Stage 报告](evidence/P7_AlphaGrayboxCookStage_2026-11-12_10-59-04.json)。新包 1280×720 RHI 启动截图通过：[截图](evidence/P7_AlphaGraybox_1280x720_2026-11-12_11-00-55.png)、[报告](evidence/P7_AlphaGrayboxViewport_1280x720_2026-11-12_11-00-55.json)。截图没有放置炮塔，因此没有目测确认放大后的实例尺寸；本轮也没有实体点击箱子验证伤害。

## 2026-11-12 画面收紧与速度可读性

- 视口展示区域裁为逻辑行 3–9（人类计数第 4–10 行），剔除上下空置行；有效格子由压扁的约 `58.9×35.2` 调为约 `58.9×55.3` 像素。鼠标映射仍返回原逻辑 CellId，并只接受可见行。
- 全板逐格网格已移除。普通建造位以及解锁后的隐藏炮位才显示完整炮位格，悬停提示也限制在未占用、可建造的位置。
- 路线描边和面色分两轮绘制，避免每个路径格之间出现深色接缝；棋盘炮塔纹理从 `0.82` 调至 `1.03` 个格宽，敌人标记也略放大。
- 敌人速度 CSV 改为普通 2400、快速 4500、干扰 1800 milli-cells/sec；单表重导入并回读三行通过：[报告](evidence/P7_EnemyTuningReimport_2026-11-12_10-16-55.json)。该值是响应本轮试玩反馈的工程候选，仍待 G0 平衡签收。
- 主工程 Game 编译成功：[击杀金币反馈构建日志](evidence/P7_MainNaiLongGameBuild_DirectUBT_KillGoldFeedback_2026-11-12.log)。最新 Development Cook/Stage PASS：[报告](evidence/P7_AlphaGrayboxCookStage_2026-11-12_10-46-39.json)。真实 RHI 截图 1280×720 PASS，地图及五表均加载：[截图](evidence/P7_AlphaGraybox_1280x720_2026-11-12_10-48-04.png)、[报告](evidence/P7_AlphaGrayboxViewport_1280x720_2026-11-12_10-48-04.json)。

## 2026-11-11 地图、中文 UI 与鼠标操作补齐

- 地图按 92% 安全宽度显示；当前可视逻辑行 3–9，去除上下空置行并扩大单格纵向尺寸。A 直行和 B 绕行使用不同颜色的连续宽车道与行进箭头；已批准地面主底图居中裁切铺底，奶蛋角色锚在路线终点。全板细网格已移除，只给可建造炮位画格子；入口、建造位、障碍、干扰点、炮塔等级/目标和敌人类型/耐久均有标记。
- 游戏内 HUD、塔种与费用、波次/金币/奶罐状态、阶段提示、悬停说明和交互反馈使用中文；遗留的 `Lv` 和 `s` 缩写也替换为“级”和“秒”。
- 塔卡、路线 A/B、开始波次、暂停/继续、升级、恢复普通索敌和胜负后的“再来一局”均支持鼠标点击；棋盘继续支持鼠标建造、选塔后点障碍设优先目标及右键取消，原有 1–3、N、A/B、空格、U、C 快捷键保留。重开会重新装载首局 fixture 并解除暂停。
- 代码位于 `NaiwaP7GrayboxHUD`、`NaiwaP7GrayboxPlayerController` 和 `NaiwaP7TowerDefenseSubsystem`；用户可见模拟错误文本同步改为中文。本轮未运行测试。
- HUD 图集 v02 的 4×4 UV 切片接入波次、金币、路线 A/B 和奶蛋状态。获批 A0 地面底图与 A1 奶蛋 PNG 已按哈希/尺寸预检、导入并接入地图呈现；地形路线、分岔门、障碍图仍未获评审，不在本轮导入范围。
- Game 启动阻断已定位为构建参数回归：修复前 Game 命令行含 `-NoLiveCoding`，而 [既有诊断](evidence/P7_GameStartupCrashDiagnosis_2026-11-01.md)已证明它会让项目 UHT 注册表布局与预编译 Engine Game 模块不一致。正式 Game 构建脚本现保留 Live Coding 默认布局、固定 UBA 缓存到 `tmp/P7UbaCache` 并实时记日志：[修复后的 Game 全量构建日志](evidence/P7_MainNaiLongGameBuild_2026-11-11_23-06-56.log)。105/105 动作通过，`Result: Succeeded`。
- 最新 Development Cook/Stage 与实机视口在“2026-11-12 画面收紧与速度可读性”段落记录；11-11 的 10,682 项 Pak 和 777006 启动失败保留作历史，不再代表当前构建。

## 2026-11-11 A0 地面底图与 A1 奶蛋终点接入

- 用户已通过的 `P7_A0_GroundBoard_Field_v01.png`（1672×941 RGBA）与 `P7_A1_GuardianMilkEgg_v04.png`（1254×1254 RGBA）按各自 manifest 中的授权状态、尺寸和 SHA-256 完成导入预检，并写入 `/Game/Naiwa/P7/Environment/A0/` 与 `/Game/Naiwa/P7/Environment/A1/`。
- 导入回读报告：[两项资源报告](evidence/P7_ApprovedMapPresentationAssetImport_2026-11-11_22-36-25.json)；代码在 HUD 中按 BoardRect 进行底图 aspect-fill 中心裁切，并将奶蛋角色放在终点外侧安全区。终点画面尺寸为 48×HudScale，并避开地图标签。
- 最新 Editor/Game 编译与 22:43 Cook/Stage 成功；接入后 viewport 于 22:41 返回 777006，未产生日志或截图。地图纹理裁切与奶蛋定位仍需视觉验收，未获评审的 tile、门和清障候选仍不导入。

## 2026-11-11 Game 启动参数修复与最终重打包

- 对照 22:41 viewport 的 777006 与既有启动诊断后，发现 22:43 Game 二进制由带 `-NoLiveCoding` 的构建命令生成；该参数会造成已记录的 UHT 注册表步长不匹配。已更新 `UE5/Scripts/build_p7_main_game_probe.ps1`：正式 `Nai_Long.uproject` Game target 不允许该参数，UBA 缓存固定在工程内 `tmp/P7UbaCache`，构建日志边执行边写入。
- 修复后 Game 全量构建 105/105、`Result: Succeeded`：[构建日志](evidence/P7_MainNaiLongGameBuild_2026-11-11_23-06-56.log)。重新 Cook/Stage 通过，Pak 收录 10,682 项：[最终 Cook/Stage 报告](evidence/P7_AlphaGrayboxCookStage_2026-11-11_23-14-57.json)。此前 viewport 失败来自修复前包；收尾时尚未启动新包，也未完成视觉或实体输入验收。

## 2026-11-11 已批准 HUD 图集运行时接入

- `T_P7_A1_HUDIconAtlas_v02` 已导入 UE；本轮在 `NaiwaP7GrayboxHUD` 中按 4×4 网格、256px 单元、24px 留边实现 Canvas UV 切片。
- 波次、金币、路线 A/B 和奶蛋耐久状态增加对应图标；地图终点从手绘方块替换为批准图集的奶蛋图标。炮塔图和 VFX v07 的既有接入保留。具体代码见 `NaiwaP7GrayboxHUD.cpp` 的 `DrawHudIcon` 与状态栏绘制段。
- Editor 编译 15 actions、Game 编译 12 actions 均通过；完整 Development Cook/Stage 通过。Viewport 尝试 777006 在引擎日志开始前退出，因此本轮新增 UV 裁切、透明边距、720p/1080p 缩放仍未获得实际画面验收。
- 桌面 Computer Use helper 连续两次初始化失败并报缺少系统路径资源；没有发出鼠标或键盘操作。实体试玩步骤仍按[试玩卡](P7_MANUAL_INPUT_TRIAL_CARD_2026-11-11.md)执行。

## 2026-11-11 A1 VFX 导入与运行时接线

- 用户通过的 v07 图集已后台导入 UE 5.8.3，作为 848×848 RGBA `Texture2D` 写入 `/Game/Naiwa/P7/VFX/A1/T_P7_A1_VFXAtlas_v07`；16 帧预检、源哈希和纹理设置回读通过：[导入报告](evidence/P7_A1_ApprovedVFXAtlasImport_2026-11-11_10-49-20.json)、[引擎日志](evidence/P7_A1_ApprovedVFXAtlasImport_Commandlet_2026-11-11_10-49-00.log)。
- `ANaiwaP7GrayboxHUD` 现在从只读模拟快照识别射击、障碍受击/清除、升级、干扰传递，以及塔恢复后的真实下一次攻击，并按图集帧显示。播放状态仅保存在 HUD，不写入模拟器，也不改变规则或 `StateHash`。
- `Nai_LongEditor Development` 构建 `Result: Succeeded`，最终接线修订也已通过增量编译：[构建日志](evidence/P7_A1_VFXRuntimeBuild_2026-11-11_10-51-58.log)。未运行测试。
- PIE/Standalone 实际事件表现、图集帧时长、屏幕缩放和不同分辨率视觉验收仍待完成；本记录不代表 A1 全部资产通过，也不关闭 G0-04。

## 2026-11-11 后台探针

- 隔离 Editor 构建 `Result: Succeeded`，25/25 动作：[构建日志](evidence/P7_IsolatedEditorBuild_2026-11-11_10-09-53.log)。隔离自动化 16/16、进程退出码 0：[测试日志](evidence/P7_Automation_2026-11-11_10-09-53.log)。
- 主工程自动化 16/16、失败 0、缺项 0、退出码 0：[JSON 报告](evidence/P7_Automation_MainProject_2026-11-11_10-12-07.json)、[引擎日志](evidence/P7_Automation_MainProject_2026-11-11_10-12-07.log)。
- `RuntimeInputBindings` 检查 11 项鼠标/键位映射；`RuntimeInputDispatch` 经 Enhanced Input 注入驱动选塔、鼠标建造、升级、障碍目标设置/清除、路线选择、开波、战斗锁路和暂停/恢复，并检查扣币、等级、障碍目标、阶段及暂停状态。
- 本轮先发现沙箱默认 `%TMP%` 含 `!`，UE `Build.bat` 的延迟展开使锁文件路径解析失败并停在“already running”。隔离脚本现自动固定 `TMP/TEMP` 到 `tmp/P7ProbeTemp` 并恢复环境变量：[探针脚本](../../Scripts/build_p7_isolated_probe.ps1)。主工程自动化脚本也自动使用该安全临时路径：[主工程脚本](../../Scripts/run_p7_main_project_automation.ps1)。
- 受限会话中曾出现 Editor commandlet 秒退、退出码 0 但无引擎日志；该类运行没有计入通过。后续在能启动 Unreal 子进程的后台执行环境中完成了上述两套 16/16 验证。
- 重新启动打包 Development 窗口并验证地图/fixture/引擎初始化/clean exit：[启动结果 JSON](evidence/P7_InteractiveInput_2026-11-11_10-20-38.json)、[完整引擎日志](evidence/P7_InteractiveInput_2026-11-11_10-20-38.log)。没有发送任何游戏内输入，故该项仅算打包启动回归，不算实体键鼠验收。

## 2026-11-11 首批 A1 炮塔图接入

- 主工程已有九张批准炮塔纹理，来源和 UE 导入路径见 [`P7_A1_Assets_v04.csv`](../../SourceAssets/P7/Manifests/A1/P7_A1_Assets_v04.csv)。灰盒 HUD 现按 `TowerId + Level` 选择奶鸡、奶狗、奶鼠的 L1–L3 纹理：塔栏显示 L1 选择图，棋盘显示已放置塔的实际等级图；原有选中边框、干扰倒计时、目标状态和缺图回退继续有效。
- 实现位于 `NaiwaP7GrayboxHUD`，仍是只读展示层，不改模拟、数据表或状态哈希。隔离 Editor 模块构建及主工程 `Nai_LongEditor Development` 构建均成功；主工程构建记录见 [日志](evidence/P7_MainNaiLongEditorBuild_ArtIntegration_2026-11-11.log)。本轮未运行自动化测试。
- PIE/Standalone 画面预览尝试提前退出，没有生成 P7 引擎日志或截图；不能据此确认纹理实机尺寸、边缘裁切或 720p/1080p 可读性。此项仍是视觉验收遗留，实体键鼠试玩也未完成。

## 2026-11-11 A1 HUD 图标图集后台导入

- 用户已通过的 16 项 HUD/交互图标 v02 已导入 UE 5.8.3，作为 1144×1144 RGBA `Texture2D` 写入 `/Game/Naiwa/P7/UI/A1/T_P7_A1_HUDIconAtlas_v02`。导入前校验了 16 个切片坐标、源 SVG/PNG 哈希和 PNG 尺寸；导入后纹理设置与来源路径回读通过：[导入报告](evidence/P7_A1_HUDIconAtlasImport_2026-11-11_21-42-43.json)、[引擎日志](evidence/P7_A1_HUDIconAtlasImport_Commandlet_2026-11-11_21-42-12.log)。
- 图集导入当时只导入源纹理，没有打开或修改地图，也没有运行游戏。16 项 [48px 彩色/灰度缩略评审页](../../SourceAssets/P7/UI/A1/Review/P7_A1_HUDIconReadability_48px_v02.png) 已于 2026-11-11 获用户通过，图标可通过形状/符号辨认；运行时接线状态见本 checkpoint 的“已批准 HUD 图集运行时接入”条目。按钮 hitbox 与 720p/1080p 游戏内视觉验收仍待完成；不代表 A1 UI 整体验收通过。

## 2026-11-11 A1 结算徽章候选

- 已补齐计划中的两种结算徽章原创候选：守护成功、奶蛋失守。按用户反馈，v02 移除奶蛋中央裂缝，保留完整蛋面和难过表情；以圆形/盾形轮廓、上扬星标/下坠碎片及配色区分状态。透明 832×416 RGBA 图集每格 384×384；评审页附 48px 放大和彩色、灰度预览，v01 保留作历史版本。
- v02 源图、导出图、逐帧哈希清单和设计说明见 `../../SourceAssets/P7/UI/A1/P7_A1_ResultBadgeAtlas_v02.svg/.png`、`../../SourceAssets/P7/UI/A1/Review/P7_A1_ResultBadgeReview_v02.svg/.png` 与 `../../SourceAssets/P7/Manifests/A1/P7_A1_ResultBadgeAssets_v02.csv`、`P7_A1_ResultBadgeDesignBrief_v02.md`。
- 当前仍为待用户视觉评审候选，尚未导入 UE；不代表结算布局验收，也不关闭 A4 或 G0-04。

## 打包运行与 HUD 证据

- Development Cook/Stage：PASS，Pak 收录 10,674 项：[报告](evidence/P7_AlphaGrayboxCookStage_2026-11-09_11-12-44.json)。
- 2026-11-11 尝试刷新 Cook/Stage 时，沙箱先拒绝 AutomationTool 用户日志目录清理；将 UAT 日志与 Saved 目录重定向到工程临时区后，Cook/Stage 进入打包阶段，但写入 UE 安装目录的 `Engine/Intermediate/Build/BuildCookRun/StagedBuild_Nai_Long.ini` 被拒绝：[本轮失败报告](evidence/P7_AlphaGrayboxCookStage_2026-11-11_10-34-18.json)、[详细日志](evidence/P7_AlphaGrayboxCookStage_2026-11-11_10-34-18.log)、[首次权限错误日志](evidence/P7_AlphaGrayboxCookStage_2026-11-11_10-28-29.log)。这是沙箱目录写权限限制，不能据此判定 P7 Cook 内容失败；11-09 的 Cook/Stage PASS 仍是最近一次完整成功结果。
- Cooked Startup：目标地图加载、五张 DataTable fixture 装配、clean exit 均通过：[报告](evidence/P7_MainGameCookedStartup_2026-11-09_11-14-00.json)。
- 720p 窗口 RHI 捕获：[图像](evidence/P7_AlphaGraybox_1280x720_2026-11-09_11-14-16.png)、[报告](evidence/P7_AlphaGrayboxViewport_1280x720_2026-11-09_11-14-16.json)。建造位、障碍简码和操作说明均可辨认。
- 1080p 全屏 RHI 捕获：[图像](evidence/P7_AlphaGraybox_1920x1080_2026-11-09_11-14-36.png)、[报告](evidence/P7_AlphaGrayboxViewport_1920x1080_2026-11-09_11-14-36.json)。

## 遗留门槛

1. 实体键鼠/真实视口坐标试玩由用户稍后现场完成；操作顺序、准确 1280×720 点击坐标、预期金币/塔等级/路线/波次/暂停结果见[试玩卡](P7_MANUAL_INPUT_TRIAL_CARD_2026-11-11.md)。完成后补截图和每步实际结果；不能用输入注入测试替代设备级验收。
2. 由项目/设计负责人签署 G0 依赖登记表中的人员/投入、reference PC、玩法规则与五表 fixture、A0 视觉样片和工程入口边界。
3. 对已导入的 VFX v07 完成 PIE/Standalone 视觉与分辨率验收，并继续完成其余 A1 正式资源导入、Standalone/Shipping、目标机性能和主持试玩验收。

G0 逐项状态与候选数据见[当前 G0 依赖登记表](P7_G0_DEPENDENCY_REGISTER_2026-11-11.md)。
