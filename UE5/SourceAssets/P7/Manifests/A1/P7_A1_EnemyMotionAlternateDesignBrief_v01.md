# P7 A1 敌人压缩过步动作备选 v01

日期：2026-11-12（Asia/Shanghai）  
状态：**普通型与干扰型的 F01 压缩过步备选已获用户视觉通过，作为已通过替代候选存档；原 v01 动作帧仍保留。动作组最终选择、锚点与节奏验收及 UE 接入待后续处理。**

## 新候选

| AssetId | 类型 | 图像 | 姿势目标 | 参考身份帧 |
|---|---|---|---|---|
| `P7-ENM-A1-NORMAL-F02-ALT` | 普通型 | `Enemies/A1/P7_A1_Enemy_Normal_Frame02_v02.png` | 身体降低并略前倾；四肢聚到身体下方，前后手臂错开摆动，形成明确的压缩过步 | 已通过普通型 F00 `P7_A1_Enemy_Normal_v01.png` |
| `P7-ENM-A1-JAMMER-F02-ALT` | 干扰型 | `Enemies/A1/P7_A1_Enemy_Jammer_Frame02_v02.png` | 身体压低并前倾，后脚收拢、前爪错开，冠毛后拖，保留完整橙色闪电标记 | 已通过干扰型 F00 `Enemies/A0/P7_A0_Enemy_Jammer_v01.png` |

两图以内置 ImageGen 分别生成，参照已通过角色首帧；生成只要求改变奔跑姿态，保留角色身份、朝向、配色、轮廓、表情和标记。原始 ImageGen PNG 副本保存在 `Enemies/A1/GenerationSources/v02/`，交付 PNG 为 1254×1254 straight RGBA。生成提示词摘要、哈希和状态见 [`P7_A1_EnemyMotionAlternateAssets_v01.csv`](P7_A1_EnemyMotionAlternateAssets_v01.csv)。

## 对照和状态

- 总览同时显示每类角色的已通过 F00、现有 v01 F01、v02 新备选 F01 和 v01 伸步 F02，并附真实 48px 彩色/灰度缩略：[`P7_A1_EnemyMotionAlternateReview_v01.png`](../../Enemies/A1/Review/P7_A1_EnemyMotionAlternateReview_v01.png)。
- v01 帧与原三帧组保持不变；用户已于 2026-11-12 通过两张新姿势，批准作为动作候选。组合后的循环节奏、锚点、播放时长、战场比例、720p/1080p 画面仍待动作组接入时验收。
- 当前未做图集切片或 UE 导入；此项通过只覆盖两张姿势图的视觉外观，不代表整组动画已完成运行时验收。
- 总览和清单可由 `UE5/Scripts/build_p7_enemy_motion_alternates_v01.mjs` 重建；此脚本只写 v02 备选 PNG、评审板和备选清单，不修改原 F00/F01/F02。

## ImageGen 提示词摘要

- **普通型 F01 备选：**保留珊瑚红矮胖角色、奶油色脸腹、双色卷冠、青色耳鳍、琥珀眼、描边与朝向；身体压低并轻微前倾，脚收到身下，手臂反向错摆，形成明显过步。
- **干扰型 F01 备选：**保留淡紫圆团、奶油胸口、深靛描边、橙闪电、扫冠、琥珀眼与朝向；身体压低约15%，后脚收拢，前爪错开，冠毛轻微后拖，闪电保持完整。

两项均要求真实透明背景、全身居中、匹配参考图的 3D 卡通渲染；避免地面、阴影、道具、武器、文字、新标记、改色、液体、黏液、奶油滴落和湿亮效果。
