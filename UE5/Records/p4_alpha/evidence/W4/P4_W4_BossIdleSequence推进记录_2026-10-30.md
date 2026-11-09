# P4 W4 Boss 待机循环推进记录（2026-10-30）

状态：**已补正面、背面、右侧面 3 组待机呼吸循环预览；只覆盖 `idle`，不代表 Boss 动画组完成。**

## 本批交付

- 3 个方向，每个方向 4 张独立 512×512 RGBA PNG，共 12 帧；每帧 0.20 秒，0.80 秒循环。
- 循环通过对完整角色做轻微均匀缩放表现呼吸。输出时将可见图像底边固定到 provisional foot anchor `(256,466)`，逐帧检查测得可见底边为 y=465，避免脚底基线跳动。
- 帧路径、SHA-256、缩放值、显示时长、循环模式与源帧见 [manifest v04](../../../../SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v04.json) 的 `boss_sequence_batch`。
- [Boss 待机评审页](P4_W4_BossIdleSequenceReview_v01.html)按视角并排显示 4 帧循环。

## 当前限制

- 全身缩放是节奏/锚点预览，不包含胸腹呼吸变形、眼睑/眨眼、手足动作或桶盖/挂带摆动，也不是可编辑分层工程。
- 其余 `move`、`charge_telegraph`、`charge_attack`、`lid_slam_telegraph`、`lid_slam_impact`、`hit`、`phase_two`、`death` 仍是各一张 WIP 姿势候选。攻击序列需明确预警、出手/冲击、收势的关键帧及逐帧时长。
- 本批未导入 UE；仍要完成边缘修整、固定机位 0.82 缩放检查和工程 PIE 验收。

## 后续推进补记（2026-10-30）

上述八个非待机状态随后各补入一个相邻姿势，组成双姿势节奏样本；当前每个样本仍缺少完整动画中间帧。详情见[Boss 动作序列记录](P4_W4_BossActionSequence推进记录_2026-10-30.md)和 [manifest v05](../../../../SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v05.json)。
