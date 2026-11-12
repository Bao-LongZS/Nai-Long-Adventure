# P7 A1 地面路线与分岔门 v02

日期：2026-11-01（Asia/Shanghai）  
状态：v02 为历史候选，未导入 Unreal；T 形分岔门的独立修订候选见 v03 说明。

## v02 改动

- 地面与路面明度拉开；路面使用深棕外沿、亮黄色面和白色中心虚线，缩小后仍能一眼追踪路线。
- 路宽统一为 256/528 格高（约 48%），虚线沿中心线贯穿直路、弯道、T 形和 Y 形单元。
- A/B 分岔门叠加层直接显示分支：启用支路用青色、白色行进虚线和大箭头；关闭支路用红色带白叉的横栏。中心勾记确认该状态已选。
- 保持路线、门状态、建造位与障碍状态分层；背景格只包含地面纹理。

## 图集规格

- 可编辑源：`Environment/A1/P7_A1_TerrainTileAtlas_v02.svg`；PNG：`Environment/A1/P7_A1_TerrainTileAtlas_v02.png`，4944×1680，8-bit RGB。
- 14 个地面单元各 960×528，间隔 24 px；AtlasRect 与 v01 一致，具体坐标见 `P7_A1_EnvironmentAssets_v02.csv`。
- 分岔门源：`Environment/A1/P7_A1_ForkGateStates_v02.svg`；PNG：`Environment/A1/P7_A1_ForkGateStates_v02.png`，1940×576，8-bit RGBA 透明背景；两格各 960×528，间隔 20 px。

## 状态

- 这是针对反馈重做的候选，仍需用户确认路线宽度、标记强度及门的含义。
- 静态 A/B 拼装已完成路线拓扑与坐标初审，见 v02 组合评审板；纹理边缘无缝采样、实际 UE CellId 覆盖、720p/1080p 下道路边界与灰度可读性，以及 G1/A0 验收仍待完成。
- v02 未导入 UE；独立 v03 门片现已导入并接入 HUD。地面图块的游戏内 A/B 路线覆盖、建造位辨识及 G1/A0 验收仍待完成。

## 2026-11-11 分岔门预检

- Alpha01 路线表在 `(1,5)` 和 `(18,5)` 形成西—东—北三向 T 形节点；分岔门 v02 SVG 的 `fork-base` 却是南侧来路、东北/东南斜向两支的 Y 形。把这张全路面门图叠在当前 T 形瓦片上，路径方向与边缘不能直接对齐。对照页：[`P7_A1_ForkGateTopologyAudit_v01.png`](../../Environment/A1/Review/P7_A1_ForkGateTopologyAudit_v01.png)。
- 当前 `P7_A1_ForkGateStates_v02.png` 声明每格宽 960 px、两格间距 20 px，但第一格描边最右延至 x=995；名义 gutter x=960–979 有 4,582 个 alpha 非零像素，第二格左边缘 528 行中有 202 行带入第一格残影。按 manifest 的第二格裁切会带入窄边残片。
- v02 保留为历史候选；T/Y 几何和 atlas bleed 修复前不导入 UE。需要按 Alpha01 的 T 分岔/汇合方向重排门片与 AtlasRect，再重新检查透明 gutter 和合成预览。
- 上述修订已作为独立 v03 完成，并于 2026-11-12 获用户通过：按 Alpha01 路表制作 A 直行（两节点共用）、B 分岔、B 汇合三格；24px gutters 均为全透明。预览见[`P7_A1_ForkGateTopologyReview_v03.png`](../../Environment/A1/Review/P7_A1_ForkGateTopologyReview_v03.png)，图集清单和说明见[`P7_A1_ForkGateStates_v03.csv`](P7_A1_ForkGateStates_v03.csv)与[`P7_A1_ForkGateStatesDesignBrief_v03.md`](P7_A1_ForkGateStatesDesignBrief_v03.md)。v02 源图仍保留为历史版本；v03 已导入并接入灰盒 HUD，viewport 视觉验收待完成。

## 2026-11-12 A/B 棋盘拼装复核

- 新增[组合评审板 v02](../../Environment/A1/Review/P7_A1_TerrainObstacleIntegrationReview_v02.png)：按 `P7_Alpha01_Level_v01.csv` 的 20×11 逻辑格绘制 A 直行与 B 绕行，展示当前可视第 4–10 行、建造位、两类障碍示例、14 个地面图块缩略和障碍六种损伤状态。
- 组合板使用地面图块 v02 与已通过的门片 v03；路线节点为 `(1,5)`、`(18,5)`，建造位与障碍坐标依 Alpha01 数据表。网格/建造位框为评审坐标辅助，非运行时网格；两种障碍外形仅作示例，运行数据里的 `CrateA/CrateB` 与图录外形映射尚未确认。
- 该图是静态源图拼装预览，不是 UE 运行截图或导入证据。地面路线 v02 和障碍 v02 仍为候选，待用户视觉评审；实际棋盘采样、破坏状态切换与 viewport 验收仍待完成。
