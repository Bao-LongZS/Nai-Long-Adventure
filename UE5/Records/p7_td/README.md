# P7 奶罐守卫战计划包

日期：2026-11-01  
状态：截至 2026-11-02，P7 v1.1 是当前范围基线；P7-only 和真实主工程内 14/14 自动化通过。5 张 `P7_Alpha01` DataTable 已由源 CSV 导入，自动化还验证了真实 fixture 的校验、购买塔和首波刷怪；独立灰盒地图已绑定 P7 Alpha GameMode，Development cook/pak/stage 后确认五表装配成功。随后以 Direct3D 12 启动真实图形视口并生成 1280×720 截图：地图、板格、双路线、建造位、障碍和 HUD 均已绘出；当前明确缺陷是 720p HUD 字体偏小，键鼠输入尚未实测（桌面读屏审批超时）。详见[当前交接 checkpoint](P7_PROGRESS_CHECKPOINT_2026-11-02.md)和[视口截图/报告](evidence/P7_AlphaGrayboxViewportSmoke_2026-11-02_12-52-21.json)。其余证据见[数据导入](evidence/P7_AlphaFixtureImport_2026-11-02_12-10-27.json)、[主工程自动化](evidence/P7_Automation_MainProject_2026-11-02_12-17-49.json)、[地图创建](evidence/P7_AlphaGrayboxMapCreation_2026-11-02_12-13-45.json)、[cook/stage](evidence/P7_AlphaGrayboxCookStage_2026-11-02_12-15-38.log)及[打包启动](evidence/P7_MainGameCookedStartup_2026-11-02_12-16-40.json)。这些平衡值仍是技术候选，G0 负责人/测试 PC/数值签收未完成；A1 导入与实机比例、Standalone/Shipping、性能和试玩也未完成，故尚不是可玩 Alpha。

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

## 当前美术推进记录（2026-11-01）

P7 A1 环境 v01 已被用户否决，反馈为信息表达不够直接、识别不够明显；v01 保留追溯，路线与清障物采用 v02。终点按用户指示改为参考角色奶蛋，去掉眼泪，沿用 10 点耐久并以 10 格耐久灯显示；奶蛋 v04 外观已获用户通过，金币沿用 v02。HUD v01 因“意义不明确”被否决；HUD v02 已获用户视觉通过。VFX v01 因意义不明确被否决，v02 与 P7 实际玩法关联不足；v03 因画面粗糙由 v04 精修取代。v04 表现攻击、清障、升级、干扰停火与恢复，等待视觉评审。均尚未导入 UE 或绑定运行时。详情见[美术变更记录](P7_美术变更记录_2026-11-01_奶蛋终点.md)、[`UE5/SourceAssets/P7/`](../../SourceAssets/P7/README.md)、[HUD v02 清单](../../SourceAssets/P7/Manifests/A1/P7_A1_HUDIconAssets_v02.csv)、[HUD v02 说明](../../SourceAssets/P7/Manifests/A1/P7_A1_HUDIconDesignBrief_v02.md)、[VFX v04 清单](../../SourceAssets/P7/Manifests/A1/P7_A1_VFXAssets_v04.csv)、[VFX v04 帧序说明](../../SourceAssets/P7/Manifests/A1/P7_A1_VFXDesignBrief_v04.md)、[路线清单](../../SourceAssets/P7/Manifests/A1/P7_A1_EnvironmentAssets_v02.csv)、[清障物清单](../../SourceAssets/P7/Manifests/A1/P7_A1_ObstacleAssets_v02.csv)、[奶蛋清单](../../SourceAssets/P7/Manifests/A1/P7_A1_GuardianMilkEggAssets_v04.csv)、[耐久灯清单](../../SourceAssets/P7/Manifests/A1/P7_A1_GuardianEggAssets_v03.csv) 与[金币清单](../../SourceAssets/P7/Manifests/A1/P7_A1_GuardianAndCoinAssets_v02.csv)。





