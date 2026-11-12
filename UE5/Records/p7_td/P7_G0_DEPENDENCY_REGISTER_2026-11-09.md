# P7 G0 依赖登记表 · 2026-11-09

> 历史快照。最新验证与 G0 状态见[2026-11-11 登记表](P7_G0_DEPENDENCY_REGISTER_2026-11-11.md)。

状态：工程验证已完成本轮更新；G0 决策项待项目/设计负责人登记与签收。本文把技术证据、待决依赖和当前候选 fixture 汇总到一个可评审入口，不替任何负责人代签。

## 最新工程证据

- 最新主工程 Development Game Cook/Stage：`PASS`，Pak 收录 10,674 项，stage 位于 `tmp/P7CookedAlphaGraybox_2026-11-09_11-12-44/Windows`。见[Cook/Stage 报告](evidence/P7_AlphaGrayboxCookStage_2026-11-09_11-12-44.json)。
- 打包地图启动、五张 DataTable 装配、引擎 clean exit：`PASS`。见[Cooked Startup 报告](evidence/P7_MainGameCookedStartup_2026-11-09_11-14-00.json)。
- 最新打包图像通过 RHI 捕获和尺寸校验：1280×720 [图像](evidence/P7_AlphaGraybox_1280x720_2026-11-09_11-14-16.png) / [报告](evidence/P7_AlphaGrayboxViewport_1280x720_2026-11-09_11-14-16.json)，1920×1080 全屏 [图像](evidence/P7_AlphaGraybox_1920x1080_2026-11-09_11-14-36.png) / [报告](evidence/P7_AlphaGrayboxViewport_1920x1080_2026-11-09_11-14-36.json)。
- 隔离探针及主工程 P7 自动化各 16/16；主工程报告含退出码 0：[隔离结果](evidence/P7_Automation_2026-11-09_11-05-34.log)，[主工程报告](evidence/P7_Automation_MainProject_2026-11-09_11-09-22.json)。Enhanced Input 注入用例覆盖选塔、点格建造、升级、障碍目标设/清、路线、开波、战斗锁路和暂停/恢复。

这些记录证明当前技术链路状态，G0 决策仍需由对应负责人签署。

## 待登记/签收事项

| ID | G0 输入 | 当前掌握 | 需要的签收记录 |
|---|---|---|---|
| G0-01 | 项目负责人、System/Game Designer、Gameplay Engineer、TA、QA/试玩主持人 | 记录中没有实名和人力可用窗口 | 角色姓名、规则与范围复核人、可用投入窗口、项目优先级及签收日期 |
| G0-02 | Reference test PC | 本轮运行环境可作为参考机候选：Windows 11 25H2 build 26200.9711；Intel Core Ultra 9 275HX；24 逻辑处理器；NVIDIA GeForce RTX 5070 Ti Laptop GPU，专用显存 11,856 MB；驱动 572.84；UE 5.8.3。工程构建日志显示约 31.47 GB 物理内存。RHI 截图覆盖 1280×720 窗口与 1920×1080 全屏 | 项目/QA 负责人确认此机器为参考机，或登记替代机器的 CPU/GPU/显存/RAM/OS/驱动/UE 版本/显示分辨率；确认后将压力目标绑定该机器 |
| G0-03 | Alpha01 玩法规则与五表 fixture | 5 张候选 CSV 已导入 UE、通过 16 项自动化和 cooked fixture startup；仍是技术验证候选 | Designer 对关卡路线、起始经济、三塔成长/职责、三敌数据、清障经济、八波刷怪节奏、干扰规则逐项确认；批准版本号、数据 hash 与日期 |
| G0-04 | A0 视觉输入和批量生产安排 | 当前证据是灰盒视口；正式 A1 美术资源尚未导入。现有 A1 VFX v07 资产包处于待评审状态 | 负责人确认风格关键词、明确拒绝项、A0 引擎样片范围与验收人，并登记实际艺术/TA/UI 人力与人天；样片通过后再冻结批量规格 |
| G0-05 | 路径/责任边界与工程入口 | P7 已独立为 `NaiwaP7Runtime`，源 CSV 位于 `UE5/SourceAssets/P7/Data/`，运行时资产位于 `/Game/Naiwa/P7/` | 负责人确认 P7 菜单/存档集成仍属后续阶段的接口边界；登记获批目录与唯一规则数据源 |

## 当前技术验证候选数据摘要

完整数据仍以五个带版本 CSV 为准：[Level](../../SourceAssets/P7/Data/P7_Alpha01_Level_v01.csv)、[Towers](../../SourceAssets/P7/Data/P7_Alpha01_Towers_v01.csv)、[Enemies](../../SourceAssets/P7/Data/P7_Alpha01_Enemies_v01.csv)、[Obstacles](../../SourceAssets/P7/Data/P7_Alpha01_Obstacles_v01.csv)、[Waves](../../SourceAssets/P7/Data/P7_Alpha01_Waves_v01.csv)。

- 地图 20×11；A 路线沿 `Y=5` 横穿，B 路线在 `Y=4` 绕行；共享干扰格 `(1,5)`。起始金币 350；8 个可见建造位；障碍 A/B 分别解锁隐藏位 `(5,9)` 和 `(14,9)`。
- 奶鸡单体塔：L1/L2/L3 成本 100/160/240，伤害 10/18/30，间隔 30/27/24 steps，射程 4000/4500/5000 milli-cells。
- 奶狗范围塔：成本 120/180/260，伤害 6/10/15，间隔 45/40/35 steps，射程 3500/4000/4500，溅射半径 1200/1500/1800 milli-cells。
- 奶鼠减速塔：成本 130/190/270，减速 25%/35%/45%，持续 90 steps；无直接伤害。
- 普通敌：24 HP、30,000 milli-cells/s；快速敌：18 HP、45,000 milli-cells/s；干扰敌：42 HP、24,000 milli-cells/s，半径 2500 milli-cells、持续 90 steps。
- 清障 A `(4,9)`：90 HP、奖励 45；清障 B `(15,9)`：130 HP、奖励 65。
- 八波敌群数量依次为：`4N`、`4N+1F`、`5N+2F`、`5N+3F`、`6N+1J`、`6N+3F+1J`、`7N+4F+2J`、`8N+5F+3J`。具体首次延迟与刷怪间隔记录在 Waves CSV；`N/F/J` 表示普通/快速/干扰敌。

## G0 关闭条件

负责人完成 G0-01 至 G0-05 的责任分配和决策后，保存签署版规则表及机器登记，再更新五张 CSV/DataTable 的获批版本。数据变更后重跑 fixture 装配、模拟/战斗、键鼠输入、Cook/Stage 与 Cooked Startup；新视觉样片需要重新验收 720p/1080p。当前记录保留为候选基线，签收前不得描述成批准平衡方案。
