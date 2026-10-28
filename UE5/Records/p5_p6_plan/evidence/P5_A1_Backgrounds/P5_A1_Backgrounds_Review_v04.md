# P5 A1 近景雪脊导出复核 v04

日期：2026-10-28  
状态：**v02 导出为当前最佳静态拼接候选；保留 WIP，等待 UE 场景复核**

## 候选与来源

- 未改动原始近景图及 v02–v04 源稿；当前选用的绘画源稿仍是 `UE5/SourceAssets/P5/Backgrounds/Source/BG_Ridge_Near_Source_v04_WIP.png`（2060×763 RGBA）。
- 按 `UE5/Scripts/p5_export_a1_ridge_seamless_candidate.ps1` 输出 `UE5/SourceAssets/P5/Backgrounds/Exports/BG_Ridge_Near_v02_WIP.png`（2048×768 RGBA）。
- 双拼静态预览：`P5_A1_RidgeTiling_v08.png`；机器可读台账：`UE5/SourceAssets/P5/Manifests/P5_A1_Backgrounds_manifest_v04.json`。

## 处理与量测

- 从 v04 源稿移除不连于雪脊主体的孤立 alpha 组件，主体边缘保留 2 px 软 alpha；按规格缩放至 2048×768。
- 在 tile 两侧各 96 px 的带宽内平滑收敛预乘颜色差异，alpha 轮廓保持原样。该处理不覆盖原稿，也不改变雪脊内部的大面积笔触。
- 导出首尾边缘每行 RGBA 平均绝对差：**[2.374, 2.547, 2.720, 5.206]**；逐通道最大绝对差：**[139, 125, 119, 245]**。平均颜色差比 v04 源稿下降，但最大差与 alpha 轮廓差仍表明不应宣称无缝通过。
- 视觉双拼未见 v04 原有的明显色彩竖缝；仍能辨出轻微轮廓高低差。v05/v06 为处理中间候选，产生了横向色带，不得用于导入。

## 判定

v02 是目前更合适的静态候选，但近景图重复的 alpha 外轮廓仍有轻微差异；不得标记为最终或运行时通过。需在三层视差、相机运动与 1080p/720p 下复核，必要时再决定是否采用此图或回到雪脊源稿修形。
