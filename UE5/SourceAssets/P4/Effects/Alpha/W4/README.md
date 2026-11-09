# P4 W4 战斗特效与主角技能特效

状态：**13 项 / 57 帧已导入 UE，并接入 A4 运行时预览；仍是 WIP 时序预览，不代表生产级美术验收。**

## 资源

- `p4_w4_fx_sample_sheet_v01_concept.png`：J 近战命中、主角冲撞接触、Boss 桶盖砸地。
- `p4_player_fx_sample_sheet_v01_concept.png`：自动奶球、近战挥击弧、冲撞拖尾、大笑范围波、主角受击、防御护层。
- `RuntimeWIP/SourceFrames/`：13 张独立高分辨率效果源 PNG。
- `RuntimeWIP/Frames512/`：13 张对应的 512×512 RGBA 静态候选，当前以画布中心 `(256,256)` 为临时 pivot。
- `RuntimeWIP/Sequences/Frames512/`：13 组、共 57 张独立 512×512 RGBA 帧，帧名含效果 ID 和序号。
- UE 预览纹理：`/Game/Naiwa/P4/Art/Alpha/W4_FX/Sequences/<effect_id>/`。
- UE 播放表：`/Game/Naiwa/P4/Data/DT_P4_W4_VFX_RuntimePreviewFrames`，57 行；来源 CSV 为 `UE5/SourceAssets/P4/Data/P4_W4_VFX_RuntimePreviewFrames.csv`。

覆盖主角技能/受击 6 项、通用战斗接触/敌人反馈 4 项、Boss 专属 3 项。每项有 3–5 帧、帧时长和播放模式。`fx_milk_shot` 与 `fx_defend_guard` 循环播放，其余为一次性序列。

## A4 Presenter 触发绑定

| 特效 | 运行时触发 |
|---|---|
| `fx_milk_shot` | `player_projectile_fired`，跟随新生成的主角投射物，投射物消失时停止 |
| `fx_melee_swing` | `player_melee_started`，朝角色面向侧显示 |
| `fx_player_dash_trail` | `player_dash_started`，跟随主角 |
| `fx_laugh_burst` | 主角进入 Laugh action cue 时播放一次 |
| `fx_defend_guard` | 主角处于 Defend action cue 时循环，离开防御态后停止 |
| `fx_player_hit` | `player_damaged` |
| `fx_melee_impact` | 仅 `player_melee_hit`；Miss 不播 |
| `fx_dash_impact` | `player_dash_hit` |
| `fx_enemy_hit` | `enemy_damaged` |
| `fx_enemy_death` | `enemy_killed` |
| `fx_boss_lid_slam` | `boss_lid_slam_hit` 或 `boss_lid_slam_missed`，落点地面反馈 |
| `fx_boss_charge_trail` | `boss_charge_hit` 或 `boss_charge_missed` 已确认冲锋结果后播放；不绑定预警 |
| `fx_boss_phase_two` | `boss_phase_two`，跟随 Boss 播放 1.35 秒 |

没有新增或改写模拟事件 ID。Boss 冲锋在确定性模拟里单 tick 完成，因此目前的冲锋拖尾是冲锋结果后的 225ms 表现尾迹，尚不是跨冲锋过程的连续拖尾。

## UE 导入与验证

- 57/57 张纹理导入后读回设置通过：RGBA8 无压缩、Sharpen0 mip、Character 纹理组、不流送。
- 数据表载入 13 个序列、57 行；Presenter 运行日志确认 A4 地图启动及目录装载成功。
- 已在 `L_NaiwaP4_ArtIntegrationPreview` 抓取延迟运行画面；当前运行日志观察到 `fx_milk_shot`、`fx_player_hit`、`fx_enemy_hit` 和 `fx_enemy_death` 组件。其余序列已完成代码绑定，未在这次单帧运行捕获中逐项触发验收。
- 修正了 VFX 动态材质嵌套动态材质的父级错误；最新运行日志对应材质父级警告为 0。
- 不提升 W4 manifest 的 `ue_imported` / `runtime_ready` 全局标志。正式轮廓审核、锚点尺寸、低闪烁替代帧、全触发逐项验收和生产资源批准仍待做。

详细交接：[UE 运行时接入记录](../../../../../Records/p4_alpha/evidence/W4/P4_W4_VFX_UE_RuntimePreview接入记录_2026-10-30.md)；[导入报告](../../../../../Records/p4_alpha/evidence/W4/P4_W4_VFX_RuntimePreview_import_report_v01.json)；[运行报告](../../../../../Records/p4_alpha/evidence/W4/P4_W4_VFX_RuntimePreview_runtime_report_v01.json)；[运行截图](../../../../../NaiwaP0/Saved/Screenshots/WindowsEditor/HighresScreenshot00016.png)；[首批交付记录](../../../../../Records/p4_alpha/evidence/W4/P4_W4_美术首批交付记录_2026-10-29.md)；[第二批候选记录](../../../../../Records/p4_alpha/evidence/W4/P4_W4_第二批美术候选交付记录_2026-10-30.md)；[特效序列记录](../../../../../Records/p4_alpha/evidence/W4/P4_W4_VFX序列推进记录_2026-10-30.md)；[序列评审页](../../../../../Records/p4_alpha/evidence/W4/P4_W4_VFXSequenceReview_v01.html)；[manifest v06](../../../Manifests/P4_W4_BossAndVFX_manifest_v06.json)。
