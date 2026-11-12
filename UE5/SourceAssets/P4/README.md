# P4 美术源资产

归属：`Nai-Long-Adventure` 项目的 P4 奶蛙竞技场子模式。W1 技术样图保留在 `Portraits/W1/`，仅用于静态帧材质/脚底验证；不作为正式动画。已通过视觉验收的 A0/A1 角色 PNG 源图和逐帧导出放 `Characters/Alpha/`（当前无 PSD/KRA/ORA 图层工程）；P4 专用环境适配放 `Environment/`，HUD/卡图放 `UI/`，导入/朝向/帧锚点说明放 `Manifests/`，参考图放 `References/` 并标 non-runtime。

正式运行时导入件只进 `UE5/NaiwaP0/Content/Naiwa/P4/`。P2/P3 候选资源通过原内容路径引用，不复制源文件或修改原资产；角色身份参考见 `UE5/SourceAssets/Common/CharacterReferences/`。

P4 HUD/结算肖像直接采用《奶蛙学》附录图录 139/003/121/106，白底原样保留；四张 UI Texture2D 已导入 P4 Content，并已在 Native UMG 源码中接线：139/003 随 HUD 动作状态切换，121/106 用于胜负结算。素材、映射与导入状态见 [`UI/Alpha/CatalogDirect/`](UI/Alpha/CatalogDirect/) 和[导入报告](../../../UE5/Records/p4_alpha/evidence/CatalogDirect/P4_A1_CatalogPortraits_Import_2026-11-12_19-42-59.json)，代码及构建结果见[接入报告](../../../UE5/Records/p4_alpha/evidence/CatalogDirect/P4_A1_CatalogPortraits_HUDIntegration_2026-11-12.json)。这是静态 UI 肖像，不覆盖 A1 透明动作帧；PIE/游戏内分辨率复核待完成。

角色资产索引：[`Characters/Alpha/README.md`](Characters/Alpha/README.md)。新接手者先读 [`P4 美术资产接手说明`](../../Records/p4_alpha/P4_美术资产接手说明_v1.md)。工时和验收按 `UE5/Records/p4_alpha/P4_美术执行计划_v1.md`；资产引用状态按 `UE5/Records/p4_alpha/P4_资产归属与复用台账_v1.md`。

W3 奶丁/奶盾概念方向已由用户通过；六状态姿势板 WIP 尚非可导入生产帧：[`Characters/Alpha/W3_Enemies/RuntimeWIP/`](Characters/Alpha/W3_Enemies/RuntimeWIP/)。为核验实际机位比例，另从批准概念图提取两张静态正面预览 PNG，现已导入 UE Content 静态预览目录；这些图不作为正式动画交付：[`Characters/Alpha/W3_Enemies/RuntimePreview/`](Characters/Alpha/W3_Enemies/RuntimePreview/)。审阅页：[静态接入预览](../../Records/p4_alpha/evidence/W3/P4_W3_StaticPreviewReview_v01.html)、[六状态姿势板](../../Records/p4_alpha/evidence/W3/P4_W3_EnemyAnimationReview_v01.html)；动画 manifest：[`P4_W3_EnemyAnimation_manifest_v01.json`](Manifests/P4_W3_EnemyAnimation_manifest_v01.json)，静态预览 manifest：[`P4_W3_StaticPreview_manifest_v01.json`](Manifests/P4_W3_StaticPreview_manifest_v01.json)，接入状态：[P4_W3_StaticPreview_integration_status_v01.json](../../Records/p4_alpha/evidence/W3/P4_W3_StaticPreview_integration_status_v01.json)。

## Beta 六章方向预览（2026-11-11）

奶丁等敌人、奶甲将军方向及章 2–6 五张独立场景概念均已由负责人通过。五张 scene v02 见[场景差异化重做](../../Records/p4_alpha/P4_Beta6_场景差异化重做_v02_2026-11-11.md)；UE 已导入场景纹理/材质并生成五张固定机位预览地图，待运行画面检查，仍不作为最终生产清理件。构建报告：[P4_Beta6_ScenePreview_build_report_v01.json](../../Records/p4_alpha/evidence/Beta6/P4_Beta6_ScenePreview_build_report_v01.json)；地图脚本：[p4_build_beta6_scene_previews.py](../../Scripts/p4_build_beta6_scene_previews.py)。v01 估算与首批综合色板保留历史记录，不覆盖本轮方向。

方向稿来源/哈希见 [`P4_Beta6_ArtDirection_manifest_v02.json`](Manifests/P4_Beta6_ArtDirection_manifest_v02.json)。
