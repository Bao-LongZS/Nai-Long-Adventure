# P7 A1 分岔门图集 v03

日期：2026-11-11（Asia/Shanghai）  
状态：用户已于 2026-11-12 通过；v03 已导入 Unreal 并接入 A/B 路线 HUD 绘制，viewport 视觉验收待完成。

## 修订目标

Alpha01 的 Route A 沿 `y=5` 西向东直行。Route B 在 `(1,5)` 由西转北，在 `(18,5)` 由北转东。两个节点都是“西—东主路加北支路”的 T 形。v02 把它画成南侧来路、东北/东南斜向分支的 Y 形，并且图块 A 的粗描边侵入图块 B 的透明 gutter；v02 留作历史版本，不覆盖。

v03 按 Alpha01 路径拆成三种有向状态：A 直行（两端共用）、B 分岔、B 汇合。开放路径用青色高亮和出口箭头，关闭支路使用红色横闸与白色叉纹。路面外沿、黄面和白色虚线沿用地面 v02 色彩与线宽。每个门片分别裁切在自己的 960×528 单元内。

## 单元与坐标

源图 `Environment/A1/P7_A1_ForkGateStates_v03.svg`；输出 `Environment/A1/P7_A1_ForkGateStates_v03.png`，尺寸 2928×528，RGBA 透明底，sRGB。相邻图块间 24 px gutter；输出 alpha 扫描结果为 **0 个 gutter 非透明像素**。

| AssetId | AtlasRect | 对应路线 | 表达 | 
|---|---:|---|---|
| `P7-ENV-A1-GATE-A-STRAIGHT` | `0, 0, 960, 528` | `(1,5)`、`(18,5)` | 西→东开放，北支路封闭；A 两端共用 | 
| `P7-ENV-A1-GATE-B-SPLIT` | `984, 0, 960, 528` | `(1,5)` | 西→北开放，东支路封闭 | 
| `P7-ENV-A1-GATE-B-MERGE` | `1968, 0, 960, 528` | `(18,5)` | 北→东开放，西支路封闭 | 

单元背景透明，审阅合成时叠加到地面 v02 的 T 格（缺南支）。箭头整体内缩，完整留在单元边界内。分辨率试摆板同时显示叠加大图与真实 48×26 下采样（彩色/灰度，最近邻放大供观察）：`Environment/A1/Review/P7_A1_ForkGateTopologyReview_v03.png`。

## 来源、哈希与复现

拓扑依据为 `Data/P7_Alpha01_Level_v01.csv` 的 RouteACells/RouteBCells。原始地面 T 格来自 `Environment/A1/P7_A1_TerrainTileAtlas_v02.png` 的 `x=1008, y=1128, w=960, h=528`。

- SVG SHA-256：`04BF1F14163B699F8B8519BEBA89E6B6C56E6356237B57F5C9367F0E63B49AF1`
- PNG SHA-256：`20C7B9BF245D27034DFADD02B3ADBAA7097AE2692D75B099A69EF6B6D997C099`
- 图集坐标与逐图哈希：`Manifests/A1/P7_A1_ForkGateStates_v03.csv`
- 重建源图、PNG、合成审阅板和清单：`Scripts/build_p7_fork_gate_v03_review.mjs`（需 Node 与 sharp；系统环境通过 `NODE_PATH` 提供 sharp）。

## 验收边界

- 已核对 Alpha01 两个节点的 T 拓扑、开放/关闭支路方向、地面 T 格对位、48px 彩色/灰度预览及 24px gutter 透明度；两条 gutter 的非透明像素均为 0。
- 用户通过后，`T_P7_A1_ForkGateStates_v03` 已作为 sRGB、无 Mipmap 的 2928×528 RGBA UI `Texture2D` 导入；Canvas 在 `(1,5)` / `(18,5)` 按当前路线选择分别绘制 A 共用直行格或 B 分岔/汇合格。路线模拟与关卡数据未更改。
- 导入报告：`UE5/Records/p7_td/evidence/P7_ApprovedCatalogPortraitAndForkGateImport_2026-11-12_10-14-23.json`；`Nai_LongEditor Development` 源码构建及增量确认：`UE5/Records/p7_td/evidence/P7_MainNaiLongEditor_ApprovedArt_2026-11-12_10-25-52.log`。
- 尚未完成 PIE/Standalone 下棋盘 CellId、720p/1080p viewport、路线切换画面和门闸碰撞验收。v02/Y 形审阅页保留供追溯：`Environment/A1/Review/P7_A1_ForkGateTopologyAudit_v01.png`。
