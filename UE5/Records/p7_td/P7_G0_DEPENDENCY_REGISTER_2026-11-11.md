# P7 G0 依赖登记表 · 2026-11-11

状态：技术验证基线更新至 2026-11-12。G0-04 已补记获批图录姿态、分岔门、三塔弹效及结算徽章 v02 接入；其余 G0 决策项仍待项目/设计负责人登记与签收。本文汇总可复验的技术证据、待决依赖和候选 fixture，不替负责人代签。

## 当前工程证据

- 主工程自动化：`PASS`，16/16、退出码 0、无缺项/失败：[JSON 报告](evidence/P7_Automation_MainProject_2026-11-11_10-12-07.json)、[引擎日志](evidence/P7_Automation_MainProject_2026-11-11_10-12-07.log)。
- 隔离 Editor 后台探针：构建 `Result: Succeeded`、25/25 动作；自动化 16/16、退出码 0：[构建日志](evidence/P7_IsolatedEditorBuild_2026-11-11_10-09-53.log)、[测试日志](evidence/P7_Automation_2026-11-11_10-09-53.log)。
- Enhanced Input 注入回路覆盖选塔、点格建造、升级、障碍目标设/清、路线、开波、战斗锁路和暂停/恢复；其中逻辑格使用仅开发测试启用的输入替身，不代表实体鼠标坐标验收。
- 最新 Development Game Cook/Stage：`PASS`，stage 位于 `tmp/P7CookedAlphaGraybox_2026-11-12_10-59-04/Windows`：[报告](evidence/P7_AlphaGrayboxCookStage_2026-11-12_10-59-04.json)、[日志](evidence/P7_AlphaGrayboxCookStage_2026-11-12_10-59-04.log)。Game 构建成功，包含敌人击杀金币、清障目标与攻击反馈和放大后的炮塔绘制：[构建日志](evidence/P7_MainNaiLongGameBuild_DirectUBT_ClearBoxAndTowerScale_2026-11-12.log)。8 个普通炮位移至 Y=7、两个障碍解锁炮位移至 Y=8，DataTable 回读通过：[报告](evidence/P7_LevelBuildSitesReimport_2026-11-12_10-41-39.json)。最新 1280×720 viewport 确认地图及五表加载：[截图](evidence/P7_AlphaGraybox_1280x720_2026-11-12_11-00-55.png)、[报告](evidence/P7_AlphaGrayboxViewport_1280x720_2026-11-12_11-00-55.json)。普通/快速/干扰敌速候选为 2.4/4.5/1.8 格/秒；击杀奖励候选为 10/15/20 金币。清障与金币平衡仍待 G0 签收；最新 viewport 处于布防阶段，清障/击杀逻辑未进行实体键鼠验收。
- 2026-11-11 较早刷新尝试未能完成 Stage：重定向 UAT 用户日志后，AutomationTool 因沙箱拒绝写入 UE 安装目录 `Engine/Intermediate/Build/BuildCookRun/StagedBuild_Nai_Long.ini` 退出：[早期报告](evidence/P7_AlphaGrayboxCookStage_2026-11-11_10-34-18.json)、[日志](evidence/P7_AlphaGrayboxCookStage_2026-11-11_10-34-18.log)；首次启动还被 `%APPDATA%` 日志目录清理权限拦截：[日志](evidence/P7_AlphaGrayboxCookStage_2026-11-11_10-28-29.log)。当晚在桌面权限下已完成上述最新版 Cook/Stage PASS。
- Cooked Startup：目标地图加载、五张 DataTable 装配、引擎 clean exit 均通过：[报告](evidence/P7_MainGameCookedStartup_2026-11-09_11-14-00.json)。
- 2026-11-11 又从打包 Development 窗口启动同一地图，确认引擎初始化、P7 地图与五张表加载、clean exit：[启动报告](evidence/P7_InteractiveInput_2026-11-11_10-20-38.json)、[引擎日志](evidence/P7_InteractiveInput_2026-11-11_10-20-38.log)。未发送实体输入，桌面试玩仍未验收。
- A1 VFX v07 于 2026-11-11 获美术视觉通过，随后作为 848×848 RGBA Texture2D 导入 `/Game/Naiwa/P7/VFX/A1/T_P7_A1_VFXAtlas_v07`，16 帧清单和纹理设置回读通过：[导入报告](evidence/P7_A1_ApprovedVFXAtlasImport_2026-11-11_10-49-20.json)、[引擎日志](evidence/P7_A1_ApprovedVFXAtlasImport_Commandlet_2026-11-11_10-49-00.log)。HUD 事件播放接线已通过 `Nai_LongEditor Development` 编译：[构建日志](evidence/P7_A1_VFXRuntimeBuild_2026-11-11_10-51-58.log)；PIE/Standalone 视觉验收尚未完成。
- 2026-11-12 按用户要求用图录003站立捧腹替换大笑图录079，并新增奶狗/奶鼠各 4 帧弹效；奶鸡继续使用 v07 尖弹。4 张当前 HUD 姿态纹理及 848×432 RGBA 新图集均完成 UE 导入，源路径、尺寸和设置回读通过：[导入报告](evidence/P7_A1_Catalog003AndTowerProjectileVFXImport_2026-11-12_10-54-44.json)。HUD 已按 `TowerId` 将奶狗接至共振范围弹、奶鼠接至薄荷减速晶体弹；玩法/模拟数据未改。图录/弹效接线的 Editor 构建通过：[构建日志](evidence/P7_MainNaiLongEditor_ApprovedArt_2026-11-12_10-57-00.log)；PIE/Standalone 下新头像与动态弹效验收仍待完成。逐帧清单与设计说明见 [`P7_A1_TowerProjectileVFXAssets_v01.csv`](../../SourceAssets/P7/Manifests/A1/P7_A1_TowerProjectileVFXAssets_v01.csv) 和 [`P7_A1_TowerProjectileVFXDesignBrief_v01.md`](../../SourceAssets/P7/Manifests/A1/P7_A1_TowerProjectileVFXDesignBrief_v01.md)。
- 已获用户视觉通过的 HUD 图标 v02（16 项）于 2026-11-11 导入为 1144×1144 RGBA Texture2D `/Game/Naiwa/P7/UI/A1/T_P7_A1_HUDIconAtlas_v02`；16 格/哈希预检、纹理设置和来源回读通过：[导入报告](evidence/P7_A1_HUDIconAtlasImport_2026-11-11_21-42-43.json)、[引擎日志](evidence/P7_A1_HUDIconAtlasImport_Commandlet_2026-11-11_21-42-12.log)。用户已通过 48px 彩色/灰度缩略预览：[评审页](../../SourceAssets/P7/UI/A1/Review/P7_A1_HUDIconReadability_48px_v02.png)。之后已完成 Canvas 4×4 UV 切片及波次、金币、路线 A/B、奶蛋状态/终点接线。获批 A0 地面主底图（1672×941）与 A1 奶蛋角色 v04（1254×1254）也已导入并接到地图底层/终点绘制：[地图呈现资源导入报告](evidence/P7_ApprovedMapPresentationAssetImport_2026-11-11_22-36-25.json)。Game 参数回归已修正并全量重建，最新 Cook/Stage 于 23:14 通过；22:41 viewport 失败对应修复前包，新包尚未进行画面检查。
- 首批 A1 炮塔九图已按 `TowerId + Level` 接入灰盒 HUD 塔栏和棋盘实例显示；主工程 Editor 构建成功：[构建日志](evidence/P7_MainNaiLongEditorBuild_ArtIntegration_2026-11-11.log)。PIE/Standalone 画面尝试未产生日志或截图，纹理尺寸/裁切视觉验收仍待完成；本轮未运行测试。
- RHI 图像捕获及尺寸校验通过：最新 1280×720 [图像](evidence/P7_AlphaGraybox_1280x720_2026-11-12_11-00-55.png)/[报告](evidence/P7_AlphaGrayboxViewport_1280x720_2026-11-12_11-00-55.json)；1920×1080 全屏历史图像见 [图像](evidence/P7_AlphaGraybox_1920x1080_2026-11-09_11-14-36.png)/[报告](evidence/P7_AlphaGrayboxViewport_1920x1080_2026-11-09_11-14-36.json)。
- 后台脚本已固定 UE 临时路径：默认沙箱 `%TMP%` 含 `!` 会被 UE `Build.bat` 的延迟展开错误解析。隔离与主工程脚本现自动使用 `tmp/P7ProbeTemp`；受限启动中无引擎日志的 commandlet 运行不计为测试通过。
- 实体输入后续由用户现场完成；[试玩卡](P7_MANUAL_INPUT_TRIAL_CARD_2026-11-11.md)记录当前包路径、1280×720 客户区坐标、按键序列和预期状态，待补实际结果与截图。

这些记录证明当前技术链路状态，G0 决策仍需对应负责人签署。

## 待登记/签收事项

| ID | G0 输入 | 当前掌握 | 需要的签收记录 |
|---|---|---|---|
| G0-01 | 项目负责人、System/Game Designer、Gameplay Engineer、TA、QA/试玩主持人 | 记录中没有实名和人力可用窗口 | 角色姓名、规则与范围复核人、可用投入窗口、项目优先级及签收日期 |
| G0-02 | Reference test PC | 本轮运行环境可作为参考机候选：Windows 11 25H2 build 26200.9711；Intel Core Ultra 9 275HX；24 逻辑处理器；NVIDIA GeForce RTX 5070 Ti Laptop GPU，专用显存 11,856 MB；驱动 572.84；UE 5.8.3。工程构建日志显示约 31.47 GB 物理内存。截图覆盖 1280×720 窗口与 1920×1080 全屏 | 项目/QA 负责人确认此机器为参考机，或登记替代机器的 CPU/GPU/显存/RAM/OS/驱动/UE 版本/显示分辨率；确认后将压力目标绑定该机器 |
| G0-03 | Alpha01 玩法规则与五表 fixture | 5 张候选 CSV 已导入 UE、通过 16 项自动化和 cooked fixture startup；仍是技术验证候选 | Designer 对关卡路线、起始经济、三塔成长/职责、三敌数据、清障经济、八波刷怪节奏、干扰规则逐项确认；批准版本号、数据 hash 与日期 |
| G0-04 | A0 视觉输入和批量生产安排 | A1 炮塔九图、VFX v07、用户通过的 HUD 图标 v02，以及已通过并导入的 A0 地面主底图（1672×941）和 A1 奶蛋角色 v04（1254×1254）均已进入 UE；炮塔、VFX、HUD 图标和地图呈现已接线。2026-11-12 用户通过四张《奶蛙学》图录 HUD 姿态及 Alpha01 T 形分岔门 v03；随后以图录003站立捧腹替换079，并为奶狗/奶鼠增加不同弹效，奶鸡沿用 v07 尖弹。用户现选择结算徽章 v02 原候选；832×416 RGBA atlas 已导入并接到胜负面板，v03 标记为未选方案。对应导入报告见 `evidence/P7_A1_SelectedResultBadgeV02Import_2026-11-12_16-02-08.json`。本轮 Editor 构建中 P7 HUD 目标编译并链接成功；整套目标被 P6 `NaiwaP6InvestigationGameMode.cpp` 的既有 `Layers` 重名 C4458 错误阻断，日志见 `evidence/P7_MainNaiLongEditor_ApprovedArt_2026-11-12_16-02-30.log`。结算面板及新增头像/动态弹效的 PIE/Standalone、720p/1080p 视觉验收待完成。A0 风格输入、引擎样片签收和实际艺术/TA/UI 人力仍未登记 | 已签收图录姿态、分岔门、三塔弹效和结算徽章 v02 原稿；负责人仍需确认风格关键词、拒绝项、A0 引擎样片范围及验收人，并登记实际艺术/TA/UI 人力与人天；其他候选样片通过后再冻结批量规格 |
| G0-05 | 路径/责任边界与工程入口 | P7 已独立为 `NaiwaP7Runtime`，源 CSV 位于 `UE5/SourceAssets/P7/Data/`，运行时资产位于 `/Game/Naiwa/P7/` | 负责人确认 P7 菜单/存档集成仍属后续阶段的接口边界；登记获批目录与唯一规则数据源 |

## 当前技术验证候选数据摘要

完整数据以五个带版本 CSV 为准：[Level](../../SourceAssets/P7/Data/P7_Alpha01_Level_v01.csv)、[Towers](../../SourceAssets/P7/Data/P7_Alpha01_Towers_v01.csv)、[Enemies](../../SourceAssets/P7/Data/P7_Alpha01_Enemies_v01.csv)、[Obstacles](../../SourceAssets/P7/Data/P7_Alpha01_Obstacles_v01.csv)、[Waves](../../SourceAssets/P7/Data/P7_Alpha01_Waves_v01.csv)。

- 地图 20×11；A 路线沿 `Y=5` 横穿，B 路线在 `Y=4` 绕行；共享干扰格 `(1,5)`。起始金币 350；8 个可见建造位均在 `Y=7`；障碍 A/B 分别解锁隐藏位 `(5,8)` 和 `(14,8)`。
- 奶鸡单体塔：L1/L2/L3 成本 100/160/240，伤害 10/18/30，间隔 30/27/24 steps，射程 4000/4500/5000 milli-cells。
- 奶狗范围塔：成本 120/180/260，伤害 6/10/15，间隔 45/40/35 steps，射程 3500/4000/4500，溅射半径 1200/1500/1800 milli-cells。
- 奶鼠减速塔：成本 130/190/270，减速 25%/35%/45%，持续 90 steps；无直接伤害。
- 普通敌：24 HP、2,400 milli-cells/s、击杀奖励 10；快速敌：18 HP、4,500 milli-cells/s、击杀奖励 15；干扰敌：42 HP、1,800 milli-cells/s、击杀奖励 20，半径 2500 milli-cells、持续 90 steps。击杀奖励为尚待 G0 签收的工程候选。
- 清障 A `(4,9)`：90 HP、奖励 45；清障 B `(15,9)`：130 HP、奖励 65。
- 八波敌群数量依次为：`4N`、`4N+1F`、`5N+2F`、`5N+3F`、`6N+1J`、`6N+3F+1J`、`7N+4F+2J`、`8N+5F+3J`。具体首次延迟与刷怪间隔记录在 Waves CSV；`N/F/J` 表示普通/快速/干扰敌。

## G0 关闭条件

负责人完成 G0-01 至 G0-05 的责任分配和决策后，保存签署版规则表及机器登记，再更新五张 CSV/DataTable 的获批版本。数据变更后重跑 fixture 装配、模拟/战斗、键鼠输入、Cook/Stage 与 Cooked Startup；新视觉样片需要重新验收 720p/1080p。当前记录保留为候选基线，签收前不得描述成批准平衡方案。
