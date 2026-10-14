# 奶蛙 E2 游戏跑动副本 v1

日期：2026-10-11。针对「大摆臂、身体前倾」「符合奶蛙人物形象」「肚子有弹性」制作。

动作保持圆肚、短腿和憨厚表情，用两侧交替的短促蹬地、弯肘摆动、轻微腾空和腹部迟滞表现着急的小跑。没有改头脸、眼睛、身材或增加骨头。正式 E2 文件不保存、不覆盖。

## 文件

| 文件 | 内容 |
|---|---|
| `Naiwa_E2_Run.blend` | E2 独立副本；原四个动作保留，新增 `Naiwa_Run`，身体网格增加三个腹部 shape key。 |
| `Naiwa_Elastic_Run.fbx` | 六个原角色网格、原骨架、三个腹部 Morph 和一个 Run 动画栈。供 UE 导入新的游戏 Mesh 并绑定既有 Skeleton。 |
| `Naiwa_Run.fbx` | 只含骨架与一个 Run 动画栈；不含腹部 Morph 曲线。 |
| `Naiwa_Run_side.mp4` / `Naiwa_Run_three_quarter.mp4` | Blender 原生预览；512×512、24 fps、48帧、三次循环、2秒。不是 UE 实机录像。 |
| `Naiwa_Run_pose_sheet.png` | 侧视与三分之四关键姿态对照。 |
| `run_parameters.json` | 输入/输出 SHA、完整动作参数、96 Hz 取样轨迹、逐帧 Morph 权重。 |
| `local_validation.json` | 原模型、骨架、四个旧动作一致性及 FBX 动画栈/Morph 检查。 |

## 动作参数与接入条件

- 24 fps，1–17帧，首尾姿态重复；完整双步周期 0.666667 秒。
- `Torso` 前倾 18–20°；源模型朝 -Y，前倾绕世界 +X。上臂的设计相对摆角前 65°、后 45°，肘部折弯52–82°，略外展以给腹部留空间。使用骨静止矩阵转换局部轴。
- 游戏角色高度180 cm，比例0.682638，目标速度300 cm/s。每侧脚支撑约0.1067秒，脚掌保持平贴，之后收腿并短暂腾空。
- 骨骼 `Root` 水平移动严格为零，垂直轻微弹跳；FBX 额外顶根 `Naiwa_Rig` 静止。UE仅锁额外顶根参考变换，并由 CharacterMovement 推动角色胶囊。
- 96 Hz 导出取样，FBX simplify=0；不导出NLA和所有动作。
- 现有骨架无踝骨，支撑段抵消胫骨旋转，避免把脚底的滚动/滑步当作正常跑动。腾空时脚掌随小腿转动。

## 局部腹部弹性

所有形态由腹部 X/Z 椭圆、上下/左右边界、躯干皮肤权重共同平滑限制。源坐标：中心 Z=1.10 m，X半径0.72 m、Z半径0.73 m；Z≤0.69 m、Z≥1.70 m、|X|≥0.635 m 位移严格零。其他五个眼球、反光点、嘴部网格未增加形态。

| Morph | 作用 | 最大游戏位移 |
|---|---|---:|
| `Belly_Compress` | 躯干局部 Z 压缩并轻微扩宽/加深 | 1.55 cm |
| `Belly_Rebound` | 压缩后的局部拉伸回弹 | 1.07 cm |
| `Belly_SoftLag` | 局部向后/向下迟滞；负值允许回甩 | 2.12 cm |

两次落脚相位为0、0.5，压缩峰值在0.11、0.61，回弹峰值在0.31、0.81；约延后1.76个源帧才压至峰值。`Belly_SoftLag` 权重可为负。`run_parameters.json` 的每个取样行保存三条实际权重，可供 UE 原生代码对齐动画时间兜底驱动。

FBX 已检查三个 BlendShapeChannel 和三个 `DeformPercent` 动画节点均存在。UE 导入后仍应检查 Morph 名称、Run曲线与实机播放；只有导入和驱动验证后，才能记录为「在游戏中已生效」。

## 本地验证

- 六个原网格的基准顶点坐标 SHA 与 E2 完全相同。
- 原10骨静止矩阵、名称、父子关系完全相同。
- 原四个动作关键帧、插值和 Euler 旋转模式完全相同。
- 两个FBX各仅一个 `Scene` 动画栈；完整FBX含三个Morph。
- 原生姿态取样期间支撑足最低5 mm顶点的接触中心，折合游戏回撤速度299.999–300.002 cm/s，20个相邻取样，均值300.000 cm/s。该检查验证制作时的足轨迹，不替代 UE 中的胶囊/动画混合/帧率检验。
- 腹部形态在头部、下腿和手臂外侧边界均无顶点变化。眼部和嘴部几何不变。

## 重建

`UE5/Scripts/create_run_animation.py` 从固定 E2 打开后重新制作本目录副本、两个FBX和预览；`--probe` 只做骨轴与颜色记录；`--quick` 只渲染关键pose；`--no-render` 只导出副本/FBX；`--encode-only` 仅编码已有16帧PNG。运行前应关闭引用此目录FBX的导入流程，避免在读取中覆盖。

正式源 SHA：`317f62278d63217636bf393f831e2fb41ce21acfefbe21beed6ad6d4dc75f322`。

本地只读校验：`validate_export.py`。关键姿态图拼接：`compose_review.py`。

## 参考

- [Adobe：跑步循环的接触、下压、经过和腾空关键姿态](https://www.adobe.com/uk/creativecloud/animation/discover/animation-run-cycle.html)
- [Animation Mentor / Jason Martinsen：动态跑步循环、前倾、摆臂与 follow-through](https://www.animationmentor.com/blog/animating-dynamic-run-cycles/)

采用动作节奏和姿态原则，参数按奶蛙短腿圆肚体型重新制作。
