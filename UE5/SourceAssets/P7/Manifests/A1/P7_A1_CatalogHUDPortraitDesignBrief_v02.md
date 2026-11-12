# P7 A1 图录直用 HUD 姿态图 v02

日期：2026-11-11（Asia/Shanghai）  
状态：**按用户指示从《奶蛙学 必修第一册.pdf》直接裁切图录原图；用户于 2026-11-12 通过四姿态及 HUD 用图方案。四张 v02 RGB 图已导入 UI Texture2D 并接入 P7 灰盒 HUD；720p/1080p viewport 视觉验收待完成。**

## 来源与范围

- 原始文件：`C:\Users\31314\Desktop\奶龙大冒险素材库\奶蛙学 必修第一册.pdf`；SHA-256：`1b3d6198eb393e735d34c0ac0ccd6fbc8f1cfc5fb7e91847a7028e25057c9361`。
- PDF 元数据题名为《奶蛙学：基础教程》，使用页标题为“奶蛙形态图谱”。本轮选用图录 079、106、121、139，分别位于 PDF 第 113、115、117、118 页（书内第 105、107、109、110 页）。
- 所选图录页页脚显示来源标记“小红书号：hongyx”。源图来源/页码记在逐项 manifest；本地源 PDF 保留在用户素材库，不复制进仓库。
- 《捧腹大笑思想与奶龙学概论》用于核对角色语义背景，没有从该 PDF 裁切本轮资产。

## 资产映射

| AssetId | 图录姿态 | P7 HUD 用途 | 图像尺寸 |
|---|---|---|---:|
| `P7-CHR-A1-CAT-139` | 139 仰首静立 | 布防状态头像 | 583×583 |
| `P7-CHR-A1-CAT-079` | 079 团身翻滚 | 防守状态头像 | 604×604 |
| `P7-CHR-A1-CAT-121` | 121 滑跪庆祝 | 胜利结算反馈 | 604×604 |
| `P7-CHR-A1-CAT-106` | 106 失落蹲坐 | 失守结算反馈 | 604×604 |

逐项哈希、页码、裁切框、来源标记和复用状态见 [`P7_A1_CatalogHUDPortraitAssets_v02.csv`](P7_A1_CatalogHUDPortraitAssets_v02.csv)。v01 保留为追溯版本：它由页裁切工具导出为索引调色板 PNG，出现可见色阶带；v02 改用 Poppler 300dpi 页面渲染和 24-bit RGB 页面裁切，没有调色或重绘。用 [`extract_p7_catalog_hud_figures_v02.ps1`](../../../../Scripts/extract_p7_catalog_hud_figures_v02.ps1) 重建到临时目录后，四张导出均与 v02 manifest SHA-256 完全一致。

## 与现有 A0 头像关系

- A0 v04 微笑→大笑两帧仍是已通过的连续 HUD 头像资产，不被图录姿态替换。
- 本组四图是相互独立的静态 HUD 反馈图：139 在布防/非终局阶段显示，079 在防守阶段显示，121/106 分别用于胜利/失守结算。不会把不同来源图拼成逐帧动画，也不放进战场。
- 图录图没有透明通道，因此按白色头像卡片来布局；结算构图板使用现有 HUD 深色结果面板与此白卡片并置，确认直接图裁切可清楚区分。

## 生产与验收边界

- 采用 PDF 页面原有白底；裁切框只保留图录单幅姿态图，页面页脚的来源标记在清单和预览板另行注明；不做透明抠图、不改角色表情/颜色、不重绘。
- 四张图作为彼此独立的 UI 姿态，不是循环动画；不用于战场角色或敌人。
- 预览板 v03 展示大图和实际 48×48 下采样结果；缩略图以最近邻 2× 放大便于检查像素：[`P7_A1_CatalogHUDPortraitReview_v03.png`](../../Characters/A1/Review/P7_A1_CatalogHUDPortraitReview_v03.png)。v02 预览保留追溯；其中缩略图按 96px 绘制，标签不准确，已由 v03 替代。
- 结算构图 v01 将图录 121/106 两图放入 P7 当前 480×228 HUD 结算面板尺寸，分别制作 1280×720 和 1920×1080 版：[`P7_A1_CatalogResultPanelMockReview_v01.png`](../../Characters/A1/Review/P7_A1_CatalogResultPanelMockReview_v01.png)。白底作为明确的头像卡片保留；结算数值是示意。该静态构图沿用当前 P7 灰盒截图和 HUD 缩放逻辑，仅供布局评审，不是 UE 运行画面。
- 2026-11-12 已导入 `/Game/Naiwa/P7/Characters/A1/CatalogDirect/`，作为 4 张 sRGB、无 Mipmap、UI 组 RGB `Texture2D`；HUD 准备/防守状态头像和结算面板接入已随 `Nai_LongEditor Development` 编译通过。
- UE 导入及设置/源路径回读报告：`UE5/Records/p7_td/evidence/P7_ApprovedCatalogPortraitAndForkGateImport_2026-11-12_10-14-23.json`；HUD 源码编译及 Editor 目标增量构建记录：`UE5/Records/p7_td/evidence/P7_MainNaiLongEditor_ApprovedArt_2026-11-12_10-25-52.log`。
- 仍待 PIE/Standalone 下实际裁切、布局、DPI 与 720p/1080p 画面验收；该导入不替代 G0-04 的 A0 样片/负责人签收。

可复建 v03 姿态预览板：`UE5/Scripts/build_p7_catalog_hud_review_v03.ps1`。可复建结算构图板：`UE5/Scripts/build_p7_catalog_result_panel_mock_v01.ps1`。
