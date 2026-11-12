# P4 A1 图录直用角色肖像 v01

按负责人要求，P4 直接采用《奶蛙学 必修第一册》附录图录中的四张原图，作为 HUD/结算静态肖像候选：139 仰首静立、003 捧腹大笑、121 滑跪庆祝、106 失落蹲坐。

这些图片与 P7 已核验的原图裁切逐字节相同。保留图录原有白底、造型和颜色；不抠透明、不重绘、不调色、不缩放。它们适合 UI 肖像牌，不替代 P4 已通过的 33 帧透明角色动画，也不作为敌人/Boss 战斗帧。

来源页码、图录编号、PDF 哈希、图像哈希、尺寸、UE 资产路径和导入状态见 [manifest](P4_A1_CatalogPortraits_manifest_v01.csv)。四张原图的并排核对页：[预览](P4_A1_CatalogPortraitsReview_v01.html)。

UE 纹理目标：`/Game/Naiwa/P4/UI/Alpha/CatalogDirect/`。四张 Texture2D 已于 2026-11-12 导入 UE 5.8.3，设置为 sRGB、UI 纹理组、关闭 Mipmap、保留原 RGB 白底；读回通过。导入报告：[P4_A1_CatalogPortraits_Import_2026-11-12_19-42-59.json](../../../../../Records/p4_alpha/evidence/CatalogDirect/P4_A1_CatalogPortraits_Import_2026-11-12_19-42-59.json)。Native UMG 已接入：HUD 用 139 仰首静立作常态、003 捧腹大笑作大笑状态；结算卡用 121 滑跪庆祝/106 失落蹲坐区分胜负。`Nai_LongEditor` UE 5.8.3 增量编译已成功（18 actions）；源码/编译记录见[接入报告](../../../../../Records/p4_alpha/evidence/CatalogDirect/P4_A1_CatalogPortraits_HUDIntegration_2026-11-12.json)。PIE/720p/1080p 游戏画面验收仍待完成。
