# P7 A1 奶罐守护物与金币 v02

日期：2026-11-01（Asia/Shanghai）  
状态：奶罐主体与液面遮罩已因用户指示“终点改成奶蛋”而被 v03 取代；金币图形仍为当前候选，未导入 Unreal。

## v02 改动

- 奶罐用宽身罐体、提环和大面积青蓝液面强化目标物与剩余量识别。
- 正面有 10 个深色刻度线，长短节奏稳定；PNG 预览显示约半罐液量，方便检查颜色和刻度对比。
- 单独提供完整白色 alpha 遮罩，限定液面的最大填充区；运行时可按血量从底部裁切。遮罩本身不代表当前液量。
- 金币采用加厚金色圆币、双层币缘和中央星形压纹，确保缩略时仍明显像货币。

## 图集规格

可编辑源：`Environment/A1/P7_A1_GuardianAndCoinAtlas_v02.svg`。PNG：`Environment/A1/P7_A1_GuardianAndCoinAtlas_v02.png`，1664×576，8-bit RGBA 透明背景。三格各 512×512，32 px 间隔和外边距。

| AssetId | 内容 | AtlasRect (x,y,w,h) |
|---|---|---|
| P7-ENV-A1-GUARDIAN-CAN | 奶罐主体、半量示意和 10 格刻度 | 32,32,512,512 |
| P7-ENV-A1-GUARDIAN-LIQUID-MASK | 罐身内部完整 alpha 遮罩 | 576,32,512,512 |
| P7-ENV-A1-COIN | 星形压纹金币 | 1120,32,512,512 |

哈希、目标路径和导出参数记录在 `P7_A1_GuardianAndCoinAssets_v02.csv`。

## 状态

三项均为 v02 视觉候选，待用户确认刻度/液面表达。待做 HUD 实际尺寸、720p/1080p、灰度/色盲辨识、alpha 边缘及 UE 遮罩填充测试；尚无 HUD 布局或液量动画。
