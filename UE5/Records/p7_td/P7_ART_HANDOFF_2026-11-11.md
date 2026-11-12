# P7 美术侧交接记录 · 基线 2026-11-11（2026-11-12 更新）

状态：美术工作交接快照，不等同于 A1/A5 或 G0 验收完成。范围基线为 [P7 综合计划书 v1.1](P7_奶罐守卫战_综合计划书_v1.1.md)；源资产索引见 [`UE5/SourceAssets/P7/README.md`](../../SourceAssets/P7/README.md)。

## 已通过并导入 UE

| 资源组 | 当前批准范围 | UE 路径 | 交接状态 |
|---|---|---|---|
| A0 地面主底图 | 用户通过的 1672×941 田野底图 | `/Game/Naiwa/P7/Environment/A0/T_P7_A0_GroundBoard_Field_v01` | 2026-11-11 已导入并绘制在地图底层；2026-11-12 1280×720 RHI 截图确认新版裁切与地图内容可见：[截图](evidence/P7_AlphaGraybox_1280x720_2026-11-12_10-34-14.png)。 |
| A1 奶蛋终点 | 用户通过的 1254×1254 奶蛋角色 v04 | `/Game/Naiwa/P7/Environment/A1/T_P7_A1_GuardianMilkEgg_v04` | 已导入并接入地图终点；2026-11-12 实机截图确认锚点可见且在地图安全区内。 |
| A1 炮塔 | 3 类 × 3 级，共 9 张 | `/Game/Naiwa/P7/Towers/A1` | 用户已通过；塔栏图标在 720p 截图中清晰。棋盘实例纹理的绘制尺寸已提高约 26%，但当前待试玩截图没有放置炮塔，实例在场比例仍待实体操作复核。 |
| A1 HUD 图标 v02 | 16 项交互图标 | `/Game/Naiwa/P7/UI/A1/T_P7_A1_HUDIconAtlas_v02` | 48px 彩色/灰度缩略评审通过，Texture2D 与切片回读通过；HUD 已接入波次、金币、路线 A/B、奶蛋状态及终点图标。2026-11-12 720p RHI 截图确认状态栏和炮塔选择卡可见，实体点击 hitbox 仍待试玩。 |
| A1 VFX v07 | 4 组 × 4 帧，共 16 帧 | `/Game/Naiwa/P7/VFX/A1/T_P7_A1_VFXAtlas_v07` | 用户已通过；848×848 RGBA 图集已导入、HUD 事件播放层已接线、Game Cook/Stage 成功。当前静态布防截图不触发战斗 VFX，帧时长和动态遮挡仍待试玩复核。 |
| A1 三塔独立弹道特效 v01 | 奶鸡沿用 v07；奶狗、奶鼠新各 4 帧 | `/Game/Naiwa/P7/VFX/A1/T_P7_A1_TowerProjectileVFX_v01` | 848×432 RGBA 图集已导入；HUD 根据 `TowerId` 分别播放共振范围弹和薄荷减速晶体弹，玩法数值未改。本轮 Editor 编译通过；动态 viewport 复核待完成。 |
| A1 图录直用 HUD 姿态 v03 | 图录 139/003/121/106 四张独立姿态 | `/Game/Naiwa/P7/Characters/A1/CatalogDirect/` | 站立捧腹图录003已替代大笑状态的079；4 张纹理已导入，接入布防/防守头像和胜负卡。720p 截图中的状态头像此前已显示；本轮新图的实际 viewport 和 DPI/1080p 布局待复核。079 原图和纹理保留追溯。 |
| A1 结算徽章 v02 | 守护成功/奶蛋失守 2 格 atlas | `/Game/Naiwa/P7/UI/A1/T_P7_A1_ResultBadgeAtlas_v02` | 用户选择 v02 原候选，Texture2D 已导入并切片接入胜负面板；v03 保留为未选方案。P7 HUD 模块编译/链接通过；整套 Editor 目标被现存 P6 `Layers` 重名 C4458 错误阻断。新结算画面 viewport 验收待完成。 |
| A1 分岔门 v03 | A 直行、B 分岔、B 汇合，3 格 atlas | `/Game/Naiwa/P7/Environment/A1/T_P7_A1_ForkGateStates_v03` | 用户通过并接入；2026-11-12 720p RHI 图确认分岔门绘制在路线节点。切换路线后的姿态切片和 CellId 对齐仍待实体试玩。 |

关键导入证据：地图底图/奶蛋角色[导入报告](evidence/P7_ApprovedMapPresentationAssetImport_2026-11-11_22-36-25.json)；HUD [报告](evidence/P7_A1_HUDIconAtlasImport_2026-11-11_21-42-43.json)；VFX v07 [报告](evidence/P7_A1_ApprovedVFXAtlasImport_2026-11-11_10-49-20.json)；图录003与炮塔弹效[导入报告](evidence/P7_A1_Catalog003AndTowerProjectileVFXImport_2026-11-12_10-54-44.json)；结算徽章 v02 [导入报告](evidence/P7_A1_SelectedResultBadgeV02Import_2026-11-12_16-02-08.json)，图录姿态/分岔门历史导入报告见 [旧报告](evidence/P7_ApprovedCatalogPortraitAndForkGateImport_2026-11-12_10-14-23.json)。本轮 P7 HUD 模块编译/链接见[构建日志](evidence/P7_MainNaiLongEditor_ApprovedArt_2026-11-12_16-02-30.log)，整套 Editor 构建受 P6 既有 C4458 错误阻断。最新 Game 构建、Cook/Stage 和 720p RHI 实机截图见[构建日志](evidence/P7_MainNaiLongGameBuild_DirectUBT_RouteSeams_2026-11-12.log)、[Cook 报告](evidence/P7_AlphaGrayboxCookStage_2026-11-12_10-32-25.json)、[截图](evidence/P7_AlphaGraybox_1280x720_2026-11-12_10-34-14.png)。视口图处于布防阶段，实体放塔、开波、路线切换后的动态验证仍待完成。

HUD v02 的 48px 彩色/灰度评审页：[`P7_A1_HUDIconReadability_48px_v02.png`](../../SourceAssets/P7/UI/A1/Review/P7_A1_HUDIconReadability_48px_v02.png)。批准状态和逐图资产 ID 记录在 [`P7_A1_HUDIconAssets_v02.csv`](../../SourceAssets/P7/Manifests/A1/P7_A1_HUDIconAssets_v02.csv)。

## 已通过用户静态评审、局内动态呈现待验收

- A0 风格样片 v04 与地面主底图已通过；地图裁切已在 720p 视口检查。其他 A0 候选仍未导入。记录：[`P7_A0_StyleSample_v04.md`](../../SourceAssets/P7/Manifests/A0/P7_A0_StyleSample_v04.md)。
- A1 奶蛋终点主体 v04 已通过并接入路线终点，720p 视口确认可见；耐久灯候选和动态耐久呈现待完成。记录：[`P7_A1_GuardianMilkEggDesignBrief_v04.md`](../../SourceAssets/P7/Manifests/A1/P7_A1_GuardianMilkEggDesignBrief_v04.md)。

## 待用户视觉评审的候选资产

- **A1 敌人动作 v02**：普通、快速、干扰三类各 3 帧；6 张基线新动作帧已完成透明边缘、48px 彩色/灰度预览检查。普通型与干扰型 F01 压缩过步备选已通过且原 v01 帧保留；快速型 F01 v02 压缩过步新候选已生成，尚待用户评审，未导入 UE。三类 F01 对照页同时显示 F00、v01、v02 和 F02，并附真实 48px 彩色/灰度缩略：[v02 对照页](../../SourceAssets/P7/Enemies/A1/Review/P7_A1_EnemyMotionAlternateReview_v02.png)。源图哈希、状态及提示词见[备选清单 v02](../../SourceAssets/P7/Manifests/A1/P7_A1_EnemyMotionAlternateAssets_v02.csv)和[备选说明 v02](../../SourceAssets/P7/Manifests/A1/P7_A1_EnemyMotionAlternateDesignBrief_v02.md)；其余基线帧、身份连续性、整组图集切片、锚点与节奏仍待评审和接入。原九帧评审页见[动作可读性页](../../SourceAssets/P7/Enemies/A1/Review/P7_A1_EnemyMotionReadabilityReview_v01.png)，初版备选页保留在[对照页 v01](../../SourceAssets/P7/Enemies/A1/Review/P7_A1_EnemyMotionAlternateReview_v01.png)，路线比例见[评审页](../../SourceAssets/P7/Enemies/A1/Review/P7_A1_EnemyTrackContrastReview_v01.png)，基线帧序见[说明](../../SourceAssets/P7/Manifests/A1/P7_A1_EnemyDesignBrief_v02.md)。
- **A1 环境 v02**：地面路线和两类可破坏物各三态仍待评审。6 张障碍图块已完成 48px 彩色/灰度和透明边缘审阅，见[预览板](../../SourceAssets/P7/Environment/A1/Review/P7_A1_DestructibleObstacleReadabilityReview_v01.png)；实际尺寸/破坏节奏待评审。新增[Alpha01 A/B 棋盘拼装评审板 v02](../../SourceAssets/P7/Environment/A1/Review/P7_A1_TerrainObstacleIntegrationReview_v02.png)，使用实际路线和建造位坐标；障碍外形仅作示例，CrateA/CrateB 图录映射未确认。分岔门 v02 的 Y 形和 atlas bleed 作为历史问题保留，细节见[拓扑预检页](../../SourceAssets/P7/Environment/A1/Review/P7_A1_ForkGateTopologyAudit_v01.png)。分岔门 v03 已获用户通过、导入并接入 HUD，状态/坐标/哈希见[说明](../../SourceAssets/P7/Manifests/A1/P7_A1_ForkGateStatesDesignBrief_v03.md)；实际 viewport 下路线切换与 CellId 对齐待复核。奶蛋主体 v04 的通过范围不自动覆盖地面路线和障碍候选。

候选未获用户通过前不得标记批准或导入；v01 历史稿保留，不覆盖源文件。

## 后续顺序建议

1. 已通过 RHI 截图确认 1280×720 静态 HUD；实体键鼠试玩仍需复核按钮 hitbox、建造 CellId 和 1920×1080 缩放。
2. 复核已导入的炮塔图在放置后的比例，以及 VFX v07 在真实战斗中的帧时长与遮挡。
3. 逐组评审敌人和环境候选；结算徽章 v02 已选定并接入，待实际胜负结算画面验收。
4. 试玩中完成地面底图、奶蛋终点遮挡、路线切换后门状态和敌速的动态检查；随后再评审 A1 环境候选，不把未通过资源接入运行包。

负责人、人力投入、Reference test PC 与 G0/A0 样片签收仍见 [P7 G0 依赖登记表](P7_G0_DEPENDENCY_REGISTER_2026-11-11.md)；该记录未代替项目负责人签收，也未关闭 A1/A5。
