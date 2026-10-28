# P5 A4 奶蛙六姿态 atlas 复核 v01

日期：2026-10-28  
状态：**项目所有者确认原稿造型；atlas 预览通过静态分格复核；游戏内集成仍待验收。**

## 输入与输出

- 项目所有者确认的源稿：`UE5/SourceAssets/P5/Characters/Source/C_Naiwa_Ski_ApprovedSixPose_Source_v01.png`，1536×1024 RGBA。
- 导出候选：`UE5/SourceAssets/P5/Characters/Exports/C_Naiwa_Ski_v01_WIP.png`，2048×1024 RGBA，3 列 × 2 行。
- 六格预览：[`C_Naiwa_Ski_AtlasPreview_v01.png`](C_Naiwa_Ski_AtlasPreview_v01.png)。
- 重建：`UE5/Scripts/p5_export_a4_character_candidate.py`；逐帧坐标见 `UE5/SourceAssets/P5/Manifests/P5_A4_NaiwaSki_manifest_v01.json`。

## 处理与结果

首版用固定等分源图裁切时，底排伸展手和雪板跨过了假定格线，造成相邻帧残片。现改为以 alpha≥32 的连通图形分离每个完整姿态，边缘向外保留 3 px 的 alpha 软边，再缩放并放入目标格；只做像素隔离与排版，没有重新生成角色外观。

| 帧 | 姿态 | 源图 alpha 连通边界（左、上、右、下） |
|---:|---|---|
| 1 | Glide | 31, 58, 556, 470 |
| 2 | Preload | 578, 104, 1047, 433 |
| 3 | AirTuck | 1101, 49, 1511, 410 |
| 4 | AirTurn | 48, 527, 541, 950 |
| 5 | Land | 544, 598, 1036, 911 |
| 6 | Fall | 1066, 593, 1518, 908 |

复核结果：六个角色与雪板均留在各自格内，当前静态预览中没有切手、切板或邻格残片。透明通道保留，预览底色仅为检查用途。

## 后续验收

- manifest 中每格 y=476 的底部中心是占位锚点；特别是空中与摔倒姿态，不能把图像外接框底部误当作雪板接触点。
- 在真实坡面和相机下逐姿态校正板面接触、尺寸、排序、动作切换与 HUD 遮挡；同时检查 1080p/720p 清晰度和 UE alpha 过滤。
- 本记录不代表运行时导入、性能或玩法验收通过；导出文件仍保留 `_WIP`。
