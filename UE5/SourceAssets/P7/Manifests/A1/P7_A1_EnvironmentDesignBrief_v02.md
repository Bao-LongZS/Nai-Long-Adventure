# P7 A1 地面路线与分岔门 v02

日期：2026-11-01（Asia/Shanghai）  
状态：针对 v01 “信息不够直接、识别不够明显”的反馈重做；当前候选待评审，未导入 Unreal。

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
- 待检查拼接后是否无缝、与 20×11 CellId 对齐、720p/1080p 下的道路边界与灰度可读性。
- 尚未导入 UE，也未完成游戏内 A/B 路线覆盖、建造位辨识或 G1/A0 验收。
