# P7 奶罐守卫战计划包

更新：2026-11-12

2026-11-12 针对试玩反馈收紧 P7 地图画幅：裁掉上下空置行，移除全板网格，只在可建造炮位绘制格子提示，并压平路线分段接缝；炮塔实例绘制尺寸先提高约 26%，最新改为 1.18 格宽。普通/快速/干扰敌速改为 2.4/4.5/1.8 格/秒并已重导入 DataTable。另加入敌人击杀金币（普通/快速/干扰 +10/+15/+20）及 HUD `+N 金币` 短提示，将普通炮位从 Y=8 移到 Y=7、清障解锁炮位从 Y=9 移到 Y=8。清障方面，零伤害奶鼠不能再被指定为清箱炮塔；有效伤害炮塔切换目标后首击立即执行，并显示目标连线、高亮和更明显的耐久条。最新 Game 编译、Cook/Stage 和 1280×720 RHI 视口截图均通过；地图/五表正常加载。清障/击杀实体操作以及放大后的炮塔实机尺寸仍待现场试玩。详见[当前进度 checkpoint](P7_PROGRESS_CHECKPOINT_2026-11-11.md)、[美术交接](P7_ART_HANDOFF_2026-11-11.md)与[实体试玩卡](P7_MANUAL_INPUT_TRIAL_CARD_2026-11-11.md)。

状态：P7 v1.1 仍是范围基线。主工程与隔离探针既有自动化各 16/16 通过，隔离 Editor 构建 25/25 动作成功；本轮未运行测试。2026-11-12 Game 编译、Cook/Stage 和 1280×720 RHI 截图均通过；画面确认地图/五表、HUD 图标、底图、奶蛋角色及新炮位加载。地图可见范围收紧为逻辑行 3–9，只在可建造炮位显示网格提示，路线段接缝已消除；炮塔实例绘制尺寸为 1.18 格宽，敌速调整表已重导入；新增击杀金币 10/15/20 与 HUD 短提示并进入新包。清障已修复零伤害奶鼠空打、换目标首击等待，并加入连线/目标高亮/耐久反馈；实际点击验证仍待现场试玩。详见[当前进度 checkpoint](P7_PROGRESS_CHECKPOINT_2026-11-11.md)、[试玩卡](P7_MANUAL_INPUT_TRIAL_CARD_2026-11-11.md)和[G0 登记表](P7_G0_DEPENDENCY_REGISTER_2026-11-11.md)。G0 平衡签收、其余环境美术、Standalone/Shipping 与性能验收仍待完成。

P7 是《Nai-Long-Adventure》内的新独立子模式计划代号，不自动表示主线第七关。模式入口、存档和运行时代码仍按项目内既有 P4/P5/P6 边界处理。

## 权威顺序

1. [P7 综合计划书 v1.1](P7_奶罐守卫战_综合计划书_v1.1.md)：当前权威基线；玩法范围、工程/美术分轨、阶段门、预算、风险、最新版本依据与机制转译。
2. [P7 资产归属与复用台账 v1.1](P7_资产归属与复用台账_v1.1.md)：P7 专属、Common 候选与导入唯一路径。
3. [P7 综合计划书 v1](P7_奶罐守卫战_综合计划书_v1.md)：上一版历史基线，仅供版本差异追溯。
4. [原始资料快照](design_reference/README.md)：用户提供的原始材料，只作评估输入；与 v1.1 综合计划冲突时，以 v1.1 为准。
5. `evidence/`：保存实际小型机制验证、Editor/Standalone 画面、构建和试玩证据，不存放观感概念图。

## 工程与美术输入边界

本次聊天生成并展示的预览图仅供委托人查看，不属于 P7 参考资料、视觉规范或验收依据；不得据此制作资产、调整布局或推导技术实现。工程按本计划书 §5、§8、§11 执行；美术按 §6、§9 与 G1/A0 正式样片执行。

## P7 资产归属

- 美术源文件归本项目，放在 [`UE5/SourceAssets/P7/`](../../SourceAssets/P7/README.md)。
- 运行时导入资产归 UE 工程 `UE5/NaiwaP0`，目标 `/Game/Naiwa/P7/`。
- 只有通过复用验收、且至少被两个关卡实际引用的通用成品，才迁入 `UE5/SourceAssets/Common/`；竞品素材、截图、商标和未验收候选不进入 Common。

## 当前美术推进记录（2026-11-11）

最新美术侧批准记录、UE 导入路径、证据和待办见 [P7 美术侧交接记录](P7_ART_HANDOFF_2026-11-11.md)。

P7 A1 环境 v01 已被用户否决，反馈为信息表达不够直接、识别不够明显；v01 保留追溯，路线与清障物采用 v02。终点按用户指示改为参考角色奶蛋，去掉眼泪，沿用 10 点耐久并以 10 格耐久灯显示；奶蛋 v04 外观已获用户通过，金币沿用 v02。HUD v01 因“意义不明确”被否决；HUD v02 的图形与 48px 彩色/灰度缩略评审均已获用户通过，图集已导入 UE。VFX v01 因意义不明确被否决，v02 与 P7 实际玩法关联不足；v03 因画面粗糙由 v04 精修取代。v04 于 2026-11-09 被用户反馈“太粗糙”；v05 的复攻图标线条混杂、含义不清；v06 又被反馈太直白、缺少特效感。当前 v07 加回分层能量流、弧形弹道、闸束余辉与粒子余波，同时控制交叠，已于 2026-11-11 获用户美术视觉评审通过、导入 UE，并接入灰盒 HUD 事件播放层；Editor 编译通过，PIE/Standalone 视觉验收仍待完成。详情见[美术变更记录](P7_美术变更记录_2026-11-01_奶蛋终点.md)、[`UE5/SourceAssets/P7/`](../../SourceAssets/P7/README.md)、[HUD v02 清单](../../SourceAssets/P7/Manifests/A1/P7_A1_HUDIconAssets_v02.csv)、[HUD v02 说明](../../SourceAssets/P7/Manifests/A1/P7_A1_HUDIconDesignBrief_v02.md)、[VFX v07 清单](../../SourceAssets/P7/Manifests/A1/P7_A1_VFXAssets_v07.csv)、[VFX v07 说明](../../SourceAssets/P7/Manifests/A1/P7_A1_VFXDesignBrief_v07.md)、[v07 玩法评审页](../../SourceAssets/P7/VFX/A1/Review/P7_A1_VFXGameplayReview_v07.png)、[路线清单](../../SourceAssets/P7/Manifests/A1/P7_A1_EnvironmentAssets_v02.csv)、[清障物清单](../../SourceAssets/P7/Manifests/A1/P7_A1_ObstacleAssets_v02.csv)、[奶蛋清单](../../SourceAssets/P7/Manifests/A1/P7_A1_GuardianMilkEggAssets_v04.csv)、[耐久灯清单](../../SourceAssets/P7/Manifests/A1/P7_A1_GuardianEggAssets_v03.csv) 与[金币清单](../../SourceAssets/P7/Manifests/A1/P7_A1_GuardianAndCoinAssets_v02.csv)。





