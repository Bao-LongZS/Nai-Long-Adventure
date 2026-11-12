# P7 A1 敌人 F01 压缩过步备选 v02

日期：2026-11-12（Asia/Shanghai）  
状态：普通型与干扰型的 F01 备选已获用户视觉通过；快速型 F01 新候选已生成，等待用户评审。原始 v01 帧与其他帧保持不变；本轮快速型候选未获批准、未导入 Unreal。

## 快速型新候选

- AssetId：`P7-ENM-A1-FAST-F02-ALT`。
- 图像：[`P7_A1_Enemy_Fast_Frame02_v02.png`](../../Enemies/A1/P7_A1_Enemy_Fast_Frame02_v02.png)，1254×1254，straight RGBA。
- 动作：身体压低并前倾；长腿收至身体下方，一脚经过躯干、另一脚后抬；手臂轻微反向摆动，长鳍向后拖，使 48px 下仍能读出快速过步。
- 身份锁定：保留原角色蓝青与蓝紫色块、奶油色脸腹、琥珀眼、脸部表情、鳍形和深海军蓝描边，只改变姿势。
- 生成方式：内置 ImageGen，以已通过的快速型 F00 `P7_A1_Enemy_Fast_v01.png` 为身份参考生成姿势备选；原始生成图副本在 `Enemies/A1/GenerationSources/v03/P7_A1_Enemy_Fast_Frame02_v02_Source.png`。

## 三类动作对照

- [评审页](../../Enemies/A1/Review/P7_A1_EnemyMotionAlternateReview_v02.png)每行展示一类敌人的 F00、F01 v01、F01 v02 和 F02，并附 48×48 彩色/灰度缩略图（2×最近邻展示）。
- [资产清单](P7_A1_EnemyMotionAlternateAssets_v02.csv)记录三张 F01 v02 候选的 AssetId、源图/导出 SHA-256、帧索引与审阅状态。普通型、干扰型保持用户已通过；快速型仍标为待评审。
- 原 v01 F01、F00 身份帧和 F02 伸步帧均未覆盖；快速型 v02 目前只是可选动作帧，不改变当前组帧选择。

## 待后续验收

- 快速型 F01 v02 的用户视觉评审；通过前不得标记批准或导入。
- 三类角色 F00→F01→F02 的循环节奏、身份连续性、锚点与角色尺寸。
- UE 图集切片、720p/1080p 实际战场比例、遮挡和运行时动画节奏。
