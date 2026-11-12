# P7 A1 可破坏物 v02

日期：2026-11-01（Asia/Shanghai）  
状态：针对 v01 “不够直接和明显”的反馈重做；当前候选待评审，未导入 Unreal。

## v02 改动

- 每种道具都使用一致正视图和固定占位，避免等距细节抢走损伤状态的注意力。
- 完好态没有裂纹；裂损态有横跨主体的大深色锯齿裂纹；将碎态加宽裂口、露出暗色断面并增加大块缺口和少量碎片。
- 两类轮廓更容易分辨：奶油饼干箱是方形箱体、绑带和扣件；燕麦饼叠垛是两层圆角饼块和烘焙孔。
- 损伤依靠形状、负形和碎块表达，不只依赖颜色；奖励与建造位开放仍由 UI 状态显示。

## 图集规格与状态顺序

可编辑源：`Environment/A1/P7_A1_DestructibleObstacleAtlas_v02.svg`。PNG：`Environment/A1/P7_A1_DestructibleObstacleAtlas_v02.png`，1664×1120，8-bit RGBA 透明背景。每格 512×512，三列两行，单元与外边距为 32 px。

| AssetId | 道具/状态 | AtlasRect (x,y,w,h) |
|---|---|---|
| P7-ENV-A1-OBST-CRATE-INTACT | 饼干箱 / 完好 | 32,32,512,512 |
| P7-ENV-A1-OBST-CRATE-CRACKED | 饼干箱 / 裂损 | 576,32,512,512 |
| P7-ENV-A1-OBST-CRATE-NEARBREAK | 饼干箱 / 将碎 | 1120,32,512,512 |
| P7-ENV-A1-OBST-OAT-STACK-INTACT | 燕麦饼叠垛 / 完好 | 32,576,512,512 |
| P7-ENV-A1-OBST-OAT-STACK-CRACKED | 燕麦饼叠垛 / 裂损 | 576,576,512,512 |
| P7-ENV-A1-OBST-OAT-STACK-NEARBREAK | 燕麦饼叠垛 / 将碎 | 1120,576,512,512 |

资产哈希、目标路径、状态序号和坐标也记录在 `P7_A1_ObstacleAssets_v02.csv`。

## 状态

六项均为 v02 视觉候选；待用户评审。2026-11-11 已完成 48px 彩色/灰度缩略和透明边缘预览，见[评审页](../../Environment/A1/Review/P7_A1_DestructibleObstacleReadabilityReview_v01.png)。三态的裂纹/缺口在缩略下仍可辨；场内相对尺寸、受击状态切换节奏、路线遮挡与 UE 导入仍待验证。

评审页可用 `UE5/Scripts/build_p7_obstacle_readability_review_v01.ps1` 复建；脚本只读取图集，不改写 SVG 或 PNG 源文件。

## 2026-11-12 棋盘组合补充

新增[Alpha01 A/B 棋盘拼装评审板 v02](../../Environment/A1/Review/P7_A1_TerrainObstacleIntegrationReview_v02.png)，用于同时检查场内相对尺寸与两类障碍在实际位置的遮挡关系。预览在 `(4,9)`、`(15,9)` 各放置一种候选外形作示例；当前运行数据仅标记 `CrateA/CrateB`，两者与图录外形的最终映射未确认。组合预览不代表导入、批准或实际受击状态切换。
