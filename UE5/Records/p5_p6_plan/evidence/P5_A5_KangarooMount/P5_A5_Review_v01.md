# P5 A5 袋鼠坐骑首版 Atlas 候选复核 v01

日期：2026-10-28  
状态：**初版 WIP；可作造型与动作讨论底稿，不能导入运行时**

## 输入与产物

- 造型参考：`UE5/SourceAssets/P5/References/C_Kangaroo_Mount_Reference_v01.png`。
- 风格/色彩参考：项目所有者通过的 `UE5/SourceAssets/P5/References/P5_A0_SkiScene_Concept_v02.png`。
- 新候选：`UE5/SourceAssets/P5/Environment/Source/C_Kangaroo_Ride_Source_v01_WIP.png`，PNG RGBA，原始输出 1254×1254，SHA256 `6B1D4F43D68B073F40365AF2263CEEBB1A4CDE03D1BF1AB1636CC26EA90FF901`。
- 六格候选与机器可读帧矩形见 `UE5/SourceAssets/P5/Manifests/P5_A5_KangarooMount_manifest_v01.json`。

## 初步结果

- 六格按待机、承载准备、下坡、弹跳、受击、恢复坐姿排列；每格独立表现袋鼠道具，没有画入奶蛙骑手。
- 黄色身体、竖耳、橙色鼻头和米白肚袋与项目所有者确认的参考方向相符；使用 P5 自然色概念的暖黄与冷蓝灰阴影方向。
- 通过系统图像读取器确认 PNG 为带 alpha 的 RGBA；画布空白点 alpha 为 0。此项是抽样检查，不代表完整 alpha 边缘验收。

## 阻塞项

- 透明区域仍可见少量红/黄杂点，需要清理后才能交给 UE；当前没有清理或遮掩这些像素。
- 当前 1254×1254 原生尺寸不符合运行时 1024×1024 目标；尚未生成符合规格的导出图。
- 六帧脚底/承载锚点、动画时序、运行时角色叠放关系、缩放可读性和 UE 过滤均未检查。
- 候选仍需项目所有者确认造型方向；不得因为入库而视为批准资产。

下一步先清理透明区杂点并按规格输出 1024×1024 atlas，再依据可骑乘、首次摔倒育儿袋保护、撞击甩脱和自然结束等玩法确认帧序与锚点，随后进入真实关卡样片复核。
