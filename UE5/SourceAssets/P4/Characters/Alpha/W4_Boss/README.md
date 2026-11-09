# P4 W4 奶桶队长美术交付

状态：**方向 B（桶盖低挂髋侧）已通过；待机三向循环及七个非待机状态的双姿势候选保留。贴地冲锋角色帧按用户反馈移除，待依原角色形象重制。完整动画和 UE 验收仍待完成。**

## 文件

- `boss_nai_tong_turnaround_v01_concept.png`：正面、背面、右侧面与主角比例对照；桶盖放在身前。
- `boss_nai_tong_turnaround_v02_concept.png`：正面、背面、右侧面与主角比例对照；桶盖低挂髋侧。已按用户确认作为当前方向。
- `boss_nai_tong_action_keyposes_v01_concept.png`：待机、冲锋预警、冲锋出击、桶盖举起、砸地五个关键姿势。
- `boss_nai_tong_phase_two_v01_concept.png`：二阶段姿势候选。
- `RuntimeWIP/SourceFrames/`：历史源帧目录；当前 manifest v06 收录 10 张 Boss 单帧候选，冲锋出击帧不在当前清单内。
- `RuntimeWIP/Frames512/`：对应的 512×512 RGBA 单帧候选，脚底初始锚点提案 `(256,466)`。
- `RuntimeWIP/Sequences/Frames512/`：待机 3 个朝向的 4 帧循环候选，每帧 0.20 秒，按脚底锚点 `(256,466)` 做轻微全身缩放；这是时序预览，不含独立肢体/桶盖运动或逐姿势重绘。
- `RuntimeWIP/Sequences/SourceFrames/`：历史相邻动作姿势源帧目录；当前 manifest v06 收录七组双姿势序列，冲锋出击序列已移出当前清单。
- `RuntimeWIP/SourceFrames/Variants/` 与 `RuntimeWIP/Frames512/Variants/`：已归档、未纳入交付清单的腾跃式冲锋旧候选。

首批概念使用 P4 已通过 A1 奶蛙视觉和已通过的奶盾转面稿作为风格/区分参考。图中 Boss/玩家比例约 1.45 倍是美术提案值，不改变工程碰撞与战斗数值。

## 第二批覆盖与限制

- 已通过候选覆盖 `idle`（正/背/右侧）、`move`（右侧）、`charge_telegraph`（右侧）、`lid_slam_telegraph` / `lid_slam_impact`（正面）、`hit` / `phase_two` / `death`（正面）。`charge_attack` 需要重制角色姿势后再加入。
- `idle` 现有 front/back/side_right 各 4 帧的低幅呼吸缩放预览；其余七个已保留状态均有两个姿势候选并分配了对拍显示时长。所有动作仍缺关键中间帧与完整持续/收尾细节。
- 附件所示贴地冲锋姿势与原角色形象不符，已从当前候选 manifest 和新版评审页移除；原角色形象对齐后再补冲锋动作。
- 候选 PNG 为 512×512、RGBA；四角及画布外边缘透明像素检查通过。完整抗锯齿/白边/黑边检查和固定机位验收仍待执行。
- 候选不含红色预警、攻击文字、血条、场地阴影或砸地冲击 VFX。工程模拟时序、距离、半径和伤害不由角色图改变。

## 后续制作约束

- 延续已通过的桶盖低挂方向，按统一 512×512 透明帧补齐 `move`、两种攻击完整起动/执行/收势、`hit`、`phase_two`、`death`。
- 保持脚底锚点稳定；初始建议 `(256,466)`，沿用 P4 现有预览口径，需在导出帧和实际机位复核后冻结。
- 桶盖应作为独立层或独立源件保存；正面、背面、右侧面需看得出手与桶盖，侧向镜像前复核道具朝向。
- 当前关键姿势板、WIP 单姿势和变换预览不可作为最终逐帧动画、生产图集或 UE 验收素材。

## 2026-11-01 工程预览接入

当前 manifest v06 的 26 张 WIP 序列帧已由工程导入 P4 A4 预览；三向待机和七组双姿势按 Boss 模拟状态展示，Boss 击败后显示 0.36 秒 death 预览。该接入不提升 `ue_imported` 或 `runtime_ready`，不代表固定机位、尺寸、动画质量或生产边缘验收完成。完整接线与探针结果见[Boss UE 预览接入记录](../../../../../Records/p4_alpha/evidence/W4/P4_W4_Boss_UE_RuntimePreview接入记录_2026-11-01.md)。

评审记录：[首批交付记录](../../../../../Records/p4_alpha/evidence/W4/P4_W4_美术首批交付记录_2026-10-29.md)；[第二批候选记录](../../../../../Records/p4_alpha/evidence/W4/P4_W4_第二批美术候选交付记录_2026-10-30.md)；[待机序列记录](../../../../../Records/p4_alpha/evidence/W4/P4_W4_BossIdleSequence推进记录_2026-10-30.md)；[角色形象反馈修订记录](../../../../../Records/p4_alpha/evidence/W4/P4_W4_角色形象反馈修订记录_2026-10-30.md)；[攻击动作评审页 v02](../../../../../Records/p4_alpha/evidence/W4/P4_W4_BossActionSequenceReview_v02.html)；[美术评审页 v03](../../../../../Records/p4_alpha/evidence/W4/P4_W4_ArtReview_v03.html)；[manifest v06](../../../Manifests/P4_W4_BossAndVFX_manifest_v06.json)。
