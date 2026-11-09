# P4 W4 特效 UE 运行时预览接入记录（2026-10-30）

## 结果

13 组、57 帧 WIP 特效已进入 UE 的 A4 美术集成预览，并由 `ANaiwaP4ArtIntegrationPawn` 按事件或角色动作状态播放。UE 5.8.3 Editor Development 编译成功；预览地图启动后从 DataTable 读到 13 个序列和 57 行。运行截图取自游戏已稳定运行数秒后的画面，而不是开局未完成曝光适配的首帧。

这代表**预览接入完成**，不代表特效的生产美术验收已通过。Manifest v06 的 `ue_imported` / `runtime_ready` 全局标记保持未提升。

## UE 资产

- 57 张 512×512 PNG 纹理：`/Game/Naiwa/P4/Art/Alpha/W4_FX/Sequences/<effect_id>/`。
- DataTable：`/Game/Naiwa/P4/Data/DT_P4_W4_VFX_RuntimePreviewFrames`；row struct 为 `FNaiwaP4VfxFrameRow`，57 行。
- 运行预览地图：`/Game/Naiwa/P4/Maps/L_NaiwaP4_ArtIntegrationPreview`。
- 57/57 Texture2D 保存后读回检查通过：`TC_EditorIcon` RGBA8 无压缩、Sharpen0 mip、Character 纹理组、never-stream。
- 数据表 CSV：`UE5/SourceAssets/P4/Data/P4_W4_VFX_RuntimePreviewFrames.csv`。

## Presenter 绑定

实现位于 `UE5/NaiwaP0/Source/NaiwaRuntime/P4/Presentation/NaiwaP4ArtIntegrationPawn.cpp`；行定义和播放数据结构位于同目录 `NaiwaP4ArtIntegrationPawn.h` 与 `../Data/NaiwaP4DataTypes.h`。

- 投射物发射、近战起手、冲刺起手、主角受击、近战命中、冲刺命中、敌人受击和死亡、Boss 桶盖砸地命中/落空、Boss 冲锋命中/落空、Boss 二阶段：使用对应 simulation event。
- 大笑 burst 与防御护层：使用主角 ActionCue 进入/保持状态。
- 命中特效只跟对应命中事件；近战 Miss 不播 impact，桶盖砸地命中与落空都在 Boss 落点播地面反馈。
- 奶球与主角防御是循环序列；奶球跟随发射物，投射物消失时清理；防御离开对应状态时清理。
- 模拟冲锋在一个 tick 内完成。为避免改写模拟事件和时间，本接入把 `fx_boss_charge_trail` 绑定到已确认的 `boss_charge_hit` / `boss_charge_missed`，播放 225ms 结果后尾迹；不绑定 `boss_telegraph_charge` 预警。这不是冲锋全过程的连续拖尾。
- 视觉层独立于模拟结果，没有增加事件 ID、伤害、碰撞或冲锋距离。

## 构建与运行复核

- 编译命令结果：`Result: Succeeded`；最终 build log 为 [P4_W4_VFX_RuntimePreview_build_final_20261030.log](P4_W4_VFX_RuntimePreview_build_final_20261030.log)。
- A4 运行日志确认：`P4 W4 VFX preview presenter loaded 13 sequences from 57 rows.`
- 本次运行日志出现四种 VFX 组件：`fx_milk_shot`、`fx_player_hit`、`fx_enemy_hit`、`fx_enemy_death`。剩余九种已完成代码绑定，但未在这一张运行截图中逐项触发和检查。
- 初次早期截图是在地图加载后的第 5 帧触发，画面过曝且角色尚未可读，因此没有作为证据交付。随后等待场景运行和曝光适配后重新截图；[HighresScreenshot00016.png](../../../../NaiwaP0/Saved/Screenshots/WindowsEditor/HighresScreenshot00016.png) 为 1920×1080 的运行时画面。
- 初次运行还暴露出 VFX 动态材质以另一个 MID 为父级的 UE 警告。已改为以共享基础 `UMaterialInterface` 为父级创建每个特效 MID，重新编译后运行日志中该类警告为 0。
- 预览日志仍有原 A3/W3 集成的非本批告警（部分 A3 运行时图标/DataTable 未找到并回退到现有预览数据）；这些没有阻止 W4 的目录和 DataTable 装载。

## 尚需完成

57 帧来自静态候选的缩放、透明度和小幅 pivot 变化，没有逐帧轮廓重绘；完整边缘精修和 0.82 机位尺寸验收仍未完成。需要逐项实际触发余下九种特效，审核各锚点和层级，制作低闪烁替代帧并取得美术 owner 生产批准后，才能将本批标为生产 ready。

## 相关报告

- [UE 导入资产报告](P4_W4_VFX_RuntimePreview_import_report_v01.json)
- [运行时预览报告](P4_W4_VFX_RuntimePreview_runtime_report_v01.json)
- [逐帧评审页](P4_W4_VFXSequenceReview_v01.html)
- [W4 特效源资源 README](../../../../SourceAssets/P4/Effects/Alpha/W4/README.md)

## 2026-11-01 工程补充

此前“冲锋在一个 tick 完成”的限制已修正：模拟现在按固定步连续推进约 1200 uu/s，并沿移动路径检测命中。新增 `boss_charge_started` 作为拖尾起始事件；`boss_charge_hit` / `boss_charge_missed` 结束拖尾，命中另播短撞击效果。代码更新和验证状态见[战斗手感修复记录](../../P4_战斗手感修复记录_2026-11-01.md)。W4 美术序列仍为 RuntimeWIP，运行时美术验收状态不变。
