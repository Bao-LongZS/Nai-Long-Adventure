# P4 美术源资产

归属：`Nai-Long-Adventure` 项目的 P4 奶蛙竞技场子模式。W1 技术样图保留在 `Portraits/W1/`，仅用于静态帧材质/脚底验证；不作为正式动画。已通过视觉验收的 A0/A1 角色 PNG 源图和逐帧导出放 `Characters/Alpha/`（当前无 PSD/KRA/ORA 图层工程）；P4 专用环境适配放 `Environment/`，HUD/卡图放 `UI/`，导入/朝向/帧锚点说明放 `Manifests/`，参考图放 `References/` 并标 non-runtime。

正式运行时导入件只进 `UE5/NaiwaP0/Content/Naiwa/P4/`。P2/P3 候选资源通过原内容路径引用，不复制源文件或修改原资产；角色身份参考见 `UE5/SourceAssets/Common/CharacterReferences/`。

角色资产索引：[`Characters/Alpha/README.md`](Characters/Alpha/README.md)。新接手者先读 [`P4 美术资产接手说明`](../../Records/p4_alpha/P4_美术资产接手说明_v1.md)。工时和验收按 `UE5/Records/p4_alpha/P4_美术执行计划_v1.md`；资产引用状态按 `UE5/Records/p4_alpha/P4_资产归属与复用台账_v1.md`。

W3 奶丁/奶盾概念方向已由用户通过；六状态姿势板 WIP 尚非可导入生产帧：[`Characters/Alpha/W3_Enemies/RuntimeWIP/`](Characters/Alpha/W3_Enemies/RuntimeWIP/)。为核验实际机位比例，另从批准概念图提取两张静态正面预览 PNG，现已导入 UE Content 静态预览目录；这些图不作为正式动画交付：[`Characters/Alpha/W3_Enemies/RuntimePreview/`](Characters/Alpha/W3_Enemies/RuntimePreview/)。审阅页：[静态接入预览](../../Records/p4_alpha/evidence/W3/P4_W3_StaticPreviewReview_v01.html)、[六状态姿势板](../../Records/p4_alpha/evidence/W3/P4_W3_EnemyAnimationReview_v01.html)；动画 manifest：[`P4_W3_EnemyAnimation_manifest_v01.json`](Manifests/P4_W3_EnemyAnimation_manifest_v01.json)，静态预览 manifest：[`P4_W3_StaticPreview_manifest_v01.json`](Manifests/P4_W3_StaticPreview_manifest_v01.json)，接入状态：[P4_W3_StaticPreview_integration_status_v01.json](../../Records/p4_alpha/evidence/W3/P4_W3_StaticPreview_integration_status_v01.json)。
