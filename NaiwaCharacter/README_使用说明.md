# 奶蛙 3D 角色：H2 / E2 整体修正完成

2026-10-09 后续联合求解修订已完成本轮 A/B 检查。B 数值资产/动作与独立复建通过，但反射和实际局部曲面显示侧颈转向更加集中，未通过整体不退化要求，不采用。按指令停止、不追加第三候选，推荐保持 E2。见 [联合求解本轮结果](versions/20261009_E2_C3_solver_revision/README_本轮结果.md) / [图文总览](versions/20261009_E2_C3_solver_revision/index.html) / [本次中断与恢复记录](versions/20261009_E2_C3_solver_revision/执行记录.md)。

2026-10-09 已生成 C3 第一候选 A 并完成同协议关键对照，未确认明显形体改善，固定参考前颈误差略增；按停止条件未采用、不生成B，正式推荐继续为E2。试验工程及原因见 [C3 本轮结果](versions/20261009_E2_C3_reference_refinement/README_本轮结果.md)。本轮未进入完整资产/动作/FBX/复建交付验证。

2026-10-08 最新推荐为 **E2**：`versions/20261007_H2_E2_overall_refinement/candidate_E2/Naiwa_Character.blend`，FBX同目录；`candidate_H2`保留独立形体阶段。头颈肩连续曲面与眼部先后修改，实际曲面、多视图、原生资产/动作、FBX重导入和独立复建全部通过。固定参考残差、远眼遮挡差距、强掠射眼窝及浅纹仍有保留，详见 [交付说明](versions/20261007_H2_E2_overall_refinement/README_交付说明.md) / [三版本图文总览](versions/20261007_H2_E2_overall_refinement/index.html)。

本轮复建入口为 `versions/20261007_H2_E2_overall_refinement/rebuild_command.ps1 -Output 新目录`，从固定R1顺序生成H2与E2并独立比对。根目录既有基础入口对应历史球面眼 A/B；最新资产使用上述E2路径。生日视频及游戏资产保持独立。

## 上一阶段：N2 圆润精修 R1

2026-10-07 最新推荐为 **R1**：`versions/20261007_N2_round_refinement/candidate_R1/Naiwa_Character.blend`，FBX 同目录。完整说明与固定条件图文总览见 [R1 交付说明](versions/20261007_N2_round_refinement/README_交付说明.md) / [图文总览](versions/20261007_N2_round_refinement/index.html)。

从固定 N2 重开生成，圆顺嘴下至喉部、侧颈/后肩及后脑至后颈；实际中线曲率峰 33.77219→26.78389，下降20.69%。全部顶点Z、五官与眼体安装、冠顶、肩根、拓扑/UV/颜色/权重、骨架和完整动画保持。原 N2 工程及 FBX 保留。Raw/Idle与三种动作姿态、固定参考、FBX真实表面及独立复建检查通过；强反射下仍保留少量既有细纹和自然嘴下阴影。

R1 生成入口为 `revise_n2_round_refinement.py --output 新目录`，验收为 `validate_n2_round_refinement.py`。最终参数、脚本快照及文件校验清单都在 R1 版本目录；可用该目录 `rebuild_command.ps1` 生成独立复建。根目录既有默认入口仍对应历史球面眼 A/B，新推荐工程以上述 R1 路径为准。

## 上一阶段：下脸—颈部 N1/N2

2026-10-06 最新修改位于 `versions/20261006_head_neck_refinement`。仅完成下脸—颈部 N1/N2，推荐 **N2**：`candidate_N2/Naiwa_Character.blend`，FBX同目录。说明与对比入口为 [第一阶段交付说明](versions/20261006_head_neck_refinement/README_交付说明.md)。固定参考前颈/后颈误差减少约49%/54%；几何、肩根姿态、独立复建及FBX重导入验收通过。上部头壳未修改。

第一阶段生成入口为 `revise_head_neck_refinement.py --candidate neck --output 新目录`，分别从固定球面眼 A 重开生成，不累加变形。独立验收入口为 `validate_head_neck_refinement.py --candidates N1 N2`。根目录既有默认入口继续对应下述球面眼 A/B；本次原生推荐模型以颈部 N2 路径为准。

## 上一阶段：独立球面眼体 A/B

2026-10-06 最新眼体迭代位于 `versions/20261006_spherical_eyes`。A/B 各有原生工程、FBX、实际球面验收和复建核对；最终说明见该目录 `README_交付说明.md`，图文总览为 `index.html`。三列 Original/A/B 使用共同相机与尺度，包含头部彩色/灰模、眼部近景、全身正面、剖面与测量表。

固定输入仍是 `versions/20261005_head_side_continuity/Naiwa_Character.blend`。仅替换左右眼网格并重新贴合高光，黄色主体、嘴、头部截面、骨架及姿态保持。每眼使用单一球心/半径的完整球面；皮肤只计算安装与遮挡，不再逐顶点叠加脸面坡度。黑绿共享球面，基础候选保持前方注视；外展20°/35°属于另存的诊断图。

当前默认 `build_naiwa.py` 调用 `revise_spherical_eyes.py`；默认 `validate_head_revision.py` 调用 `validate_spherical_eyes.py`。可用 `--output` 指定新目录、`--candidate A/B/both` 选择输出。原固定工程与历史版本保留，不用历史 `--from-primitives` 分支替代当前基准。校准后的实际 R/h、球心、安装法线以及参考视角残差以新交付报告为准。

## 文件清理（2026-10-06）

补充清理清单见 `cleanup_report_20261006.json`。全部原生 `.blend`、当前与各正式版本 FBX、参考图、诊断材料、头颈修整中的资料、生成脚本和验收记录保留。重复完整压缩包在逐文件校验后移除；历史说明中的完整 ZIP 不再作为唯一内容来源。诊断包的独有说明和清单保存在 `deliveries/diagnostic_package_metadata_20261006.json`。

部分 `iterations` 历史目录仅保留原生工程，移除可重建的 FBX；带独立 FBX 验收脚本的历史目录仍保留导出。`before_eye_muzzle_body_revision` 的重复预览统一使用角色根目录的同名图片。需要恢复历史 FBX 时，在项目根目录运行（将历史目录替换为所需版本）：

```powershell
& 'D:\STEAM\steamapps\common\Blender\blender.exe' --background --python-exit-code 1 --python 'NaiwaCharacter\export_saved_fbx.py' -- --blend 'NaiwaCharacter\iterations\before_eye_muzzle_body_revision\Naiwa_Character.blend'
```

工具直接导出保存工程内的绑定网格与动作，不执行造型重建，也不保存或修改输入 `.blend`；默认在其旁生成 FBX，已存在的输出需要显式 `--overwrite`。

以下保留上一轮诊断及历史交付记录，其中“当前”均指对应记录形成时的版本。

---

## 上一轮：头部几何诊断与侧面连续性版本

2026-10-06头部几何诊断已完成：[图文总览](diagnostics/20261005_head_geometry/index.html) · [中文诊断报告](diagnostics/20261005_head_geometry/DIAGNOSTIC_REPORT.md)。包括眼球剖面和灰模、头部截面与测量表、PDF图1.3参考并排图、生成参数及代码快照。本轮模型几何、尺寸与位置保持，工程版本继续使用下述侧面连续性版本。

当前新工程为 `versions/20261005_head_side_continuity/Naiwa_Character.blend`，FBX、12 张统一协议头部前后渲染及彩色、灰模两张对比图均在该目录。完整说明见 `versions/20261005_head_side_continuity/README_交付说明.md`。

本轮只修整眼下至上颊的侧面连续性。D=0.1172448127；形变支持范围 Z=2.3066443659～2.4700919717，实际修改顶点范围 Z=2.3080956936～2.4692780972。最大实际位移 0.0042443871，最大法向分量 0.0041347479≈0.0353D。眼线头宽 0.4475002099、眼径、眼距、眼心高度、头顶、嘴形、自然喉线、肩部和身体保持。

`build_naiwa.py` 默认调用 `revise_head_side_continuity.py`，从上一轮 `versions/20261005_head_reshape/Naiwa_Character.blend` 固定基准生成；支持 `--output`，不会累积位移。默认验收入口 `validate_head_revision.py` 调用 `validate_head_side_continuity.py`。独立保存网格测量、FBX 重导入及重复重建一致性检查均通过。

根目录工程是更早历史版本；本轮以以上新版本路径为准。下面保留历史说明，其中“当前”均指当时版本。

---

## 上一轮头部主形体重塑

当前新工程为 `versions/20261005_head_reshape/Naiwa_Character.blend`，FBX、14 张统一协议前后渲染及 3 张对比图均在该目录。完整说明见 `versions/20261005_head_reshape/README_交付说明.md`。

本轮固定原始眼径 D0=0.1020112182、黄色眼线头宽 W0=0.4068188484；采用眼径 D1=0.1173129009、眼距 0.2346258018，黄色眼线头宽实测约 0.4475002099。撤销上一轮新增深咽喉凹陷，短嘴总前伸量控制在约 0.15D1。站立、双臂垂放、原眼中心高度、头顶高度和身体保持。

`build_naiwa.py` 默认调用 `revise_head_reshape.py`，从 `versions/20261005_eye_muzzle_body_delivery/Naiwa_Character.blend` 固定基准生成；支持 `--output`，不会对已生成头型重复缩放或移动。默认验收入口 `validate_head_revision.py` 调用 `validate_head_reshape.py`。`run_blender_stage.ps1` 检查 Python 退出码和完成标记，完整输出记录到日志。

根目录的 `Naiwa_Character.blend`、旧预览与旧 ZIP 是历史版本；本轮以这里的新版本路径为准。下面保留上一轮与更早的历史说明，其中“当前”均指当时版本。

---

## 上一轮眼睛、嘴部与身体调整

当前新工程为 `versions/20261005_eye_muzzle_body_delivery/Naiwa_Character.blend`，对应 FBX、12 张前后原图及全身/头部对比汇总图均在该目录。完整说明见 `versions/20261005_eye_muzzle_body_delivery/README_交付说明.md`。

本轮已完成面部上移 0.4D、体积眼最终凸出 0.20D、黑瞳 0.53D、短吻部与真实深嘴缝、肩根平顺、腹部色区收窄下延和整脚棕色。固定原始尺寸 H=2.63683057、D=0.10238124；站立与双臂垂放保持。

`build_naiwa.py` 和 `revise_eye_muzzle_body.py` 默认均读取 `iterations/before_eye_muzzle_body_revision` 中不可变的修改前工程，支持 `--output` 指定输出目录。默认独立验收入口为 `validate_head_revision.py` 或 `validate_eye_muzzle_delivery.py`；FBX 验收通过 `validate_fbx.py -- --fbx <文件>`。

独立验收、FBX 重导入和生成脚本重复重建的逐项一致性检查均已通过，记录在新交付目录。临时重建工程、旧草稿、重复旧 ZIP、自动备份和可重建缓存已清理，验收日志和各历史 `before_*` 原生工程保留；清单见 `cleanup_report.json`。

主目录 `Naiwa_Character.blend` 继续保留为本轮修改前版本。下面是该旧版本的历史说明，其中“当前”和脚本入口描述均指当时版本，不能替代上面的新交付入口。

---

## 连续浅前脸与细口缝旧版说明

目标角色是**奶蛙**：从奶龙衍生的网络二创形象，并非奶龙官方角色。本工程依照用户给出的站立姿态、身体比例、面部及材质规格建立基础体型，暂不塑造捧腹大笑表情。它是程序化建模的近似造型，**不是完成的高精度雕刻**。请先看同目录的正面、侧面和四分之三视角预览。

当前迭代是 **2026-10-05 连续浅前脸与细口缝**。本轮以已保存的圆润眼圈版为基准，完整前版位于 `iterations/before_continuous_front_face`。调整限定在眼下至嘴下的前侧曲面：回收上下唇凸条及嘴上方中央隆起，由整片前脸提供小幅前伸，再连续退入已有喉弧。头顶、后颈、头宽、颈长及眼圈正面圆度保持。更早版本位于各自历史目录。

## 文件

- `Naiwa_Character.blend`：Blender 原生工程；包括角色、骨架、四段动画和预览灯光。
- `Naiwa_Character.fbx`：供虚幻引擎导入的骨骼网格和动画。
- `naiwa_front.png`、`naiwa_three_quarter.png`、`naiwa_side.png`：三张正常站立预览图。
- `naiwa_review_front.png`、`naiwa_review_side.png`、`naiwa_review_three_quarter.png`：本轮统一相机和柔光的彩色头颈验收近景。
- `naiwa_surface_gray_front.png`、`naiwa_surface_gray_side.png`、`naiwa_surface_gray_three_quarter.png`：对应的中性灰色曲面检查图。
- `naiwa_face_close.png`、`naiwa_hand_close.png`、`naiwa_foot_close.png`：面部、手部和脚部的彩色近景，供检查瞳孔、腕掌连接及指趾根部。
- `naiwa_face_side_close.png`、`naiwa_face_three_quarter_close.png`：面部侧面与斜侧面近景，供检查短脸前伸、嘴下浅喉弧及眼周嵌入。
- `naiwa_foot_side_close.png`：双脚的侧视近景，用于检查后跟、脚背坡面与脚底接地轮廓。
- `naiwa_silhouette_front.png`、`naiwa_silhouette_side.png`：用于检查头肩、腹部和腿脚大轮廓的黑色剪影。
- `naiwa_torso_silhouette_front.png`、`naiwa_torso_silhouette_side.png`：隐藏眼睛、口部、手臂及腿脚，只检查头和躯干连续轮廓。
- `naiwa_surface_check_side.png`：单色侧视诊断图，用于区分几何起伏与腹部颜色渐变。
- build_naiwa.py：完整再生成脚本；依赖 connection_sculpt.py、head_volume_sculpt.py、neck_extension_sculpt.py、neck_blend_sculpt.py、neck_direction_sculpt.py、longitudinal_surface_sculpt.py、short_round_face_sculpt.py、broad_face_arc_sculpt.py、lip_eye_sculpt.py、eye_projection_measure.py、continuous_front_face_sculpt.py、render_surface_diagnostics.py 同目录。直接重现当前工程、FBX 和全部预览请运行 revise_continuous_front_face.py；该入口始终读取本轮前版备份，避免重复叠加形变。
- connection_sculpt.py：上一轮头胸连接曲面模块，为当前基础造型的依赖。
- head_volume_sculpt.py：前一轮的头部体量收减模块，作为当前形状的基础。
- revise_head_volume.py：历史体量修改驱动。当前请使用 revise_continuous_front_face.py。
- head_volume_validation.json：前一轮体量收减的历史核验数据。
- neck_extension_sculpt.py：本轮保持头部刚体、延长颈带的模块。
- revise_neck_extension.py：历史头部上移驱动。当前请使用 revise_continuous_front_face.py。
- neck_extension_validation.json：上一轮头部平移的历史核验数据。
- naiwa_neck_front_close.png、naiwa_neck_side_close.png：新增的颈部正面和侧面近景。
- revise_connection.py：历史连接修改驱动。当前请使用 revise_continuous_front_face.py。
- validate_head_revision.py：当前验证入口，转交 validate_continuous_front_face.py 核验。
- connection_validation.json：早期连接的历史记录；当前请查看 continuous_front_face_validation.json。
- `validate_fbx.py`：FBX 重新导入检查，包括骨架、动画、身体颜色和左右眼的 `EyeColor` 顶点颜色。
- `render_details.py`：从最新 Blender 工程重新渲染面部、手部和脚部的细节近景。

## 当前内容

- 总高度约 2.645 Blender 单位。本轮身体和诊断壳的全部 X/Z 坐标逐项保持；后半曲面 Y 保持；Z≤2.20、Z≥2.46 及两颊外缘 |X|≥0.19 的身体完全保持。头顶、头宽、后颈長弧、颈长和胸肩臂不变，形变在下侧既有喉弧内归零。
- 前脸沿上下采用宽缓连续曲线，去掉口缝上下窄条凸起和嘴上鼻状隆起；最前缘位于口缝附近，向上接眼下、向下回收。横向变化向脸颊两侧逐渐减弱。身体实际调整 4,776 个顶点，最大位移约 0.00867 Blender 单位。
- 口缝的 X/Z 及浅弧形状保留，只重新贴合新皮肤：宽约 0.10977，中央厚约 0.00132，两端厚约 0.00016，中央低于两端约 0.00170，表面间距约 0.00045。唇附近的起伏由前脸整体曲面提供。
- 绿色眼圈保持上一版轻微竖椭圆，瞳孔圆润；眼部全部 X/Z 坐标、原瞳孔正面锚点、双色边界及布局保持。眼圈下缘仅沿前后方向随所在皮肤重新贴合，避免回收前脸后留下眼下鼓边。身体颜色、双眼 EyeColor、网格拓扑、十根骨骼及全部骨骼权重保留。
- 手臂根部上提，从上胸侧面以连续扫掠曲面分出；根部先埋入躯干，再沿两侧向下展开，避免独立肩球。上臂、肘、前臂保持柔和曲线，双手与腹部保留少量空隙；腕掌、短指和脚部保持前版造型与配色。
- 本轮正面、侧面及四分之三均为平视正交预览，角度为 0°、90°、35°；同一组使用相同距离、镜头设置、正交尺度和灯光。保留剪影、彩色近景与柔光灰色检查图。
- 10 根骨骼；Idle、Walk、Laugh、Jump 四段基础动作。Laugh 目前只是身体摆动占位动作，脸部保持平静。
- 动作均属于演示级，需要按实际玩法继续调整节奏、幅度和过渡。

## 在 Blender 中查看

用 Blender 5.2.2 或更新版打开 `.blend`。在右侧 Outliner 中选 `Naiwa_Rig`，将模式切到 Pose Mode；在 Dope Sheet 的 Action Editor 中可选 `Naiwa_Idle`、`Naiwa_Walk`、`Naiwa_Laugh`、`Naiwa_Jump`。

## 导入 UE 5.8 项目

1. 打开 `Nai_Long`，在内容浏览器中新建 `Characters/Naiwa` 文件夹。
2. 点“导入”，选择 `Naiwa_Character.fbx`。
3. 导入窗口中启用“骨骼网格体”和“导入动画”，Skeleton 留空，让 UE 建立新骨架；将生成的资源放在 `Characters/Naiwa`。保留网格顶点颜色。
   在身体材质中加入 `Vertex Color` 节点，将其 RGB 接到 `Base Color`，以显示米色肚皮和棕色手指、前掌。左右眼各自的眼睛材质也需要同样连接 `Vertex Color` 的 RGB 到 `Base Color`，以显示绿色虹膜、黑色瞳孔与外圈颜色。身体的 `NaiwaColor` 和眼睛的 `EyeColor` 均随 FBX 导出；`validate_fbx.py` 会检查重新导入后颜色数据是否保留。UE 中使用导入网格的顶点颜色，不需按属性名分别创建节点。
4. 打开现有的 `BP_SideScrollingCharacter`，选 Mesh 组件，将 Skeletal Mesh 改为导入的奶蛙网格。
5. 暂时把 Animation Mode 设为“使用动画资产”，指定 `Naiwa_Idle`，编译、保存并运行。这样先验证外观和角色移动；之后再做 Animation Blueprint，把 Walk、Jump、Laugh 接入角色状态。
6. 根据实际导入后的高度，调 Mesh 的相对位置、缩放和 Capsule 碰撞体。目标是脚底与地面齐平，身体不穿模。

这版使用独立骨架，不能直接套用模板人形角色的动画蓝图。当前仅导出了文件，并未修改你的 UE 项目，也未在 UE 中验证外观与比例。

## 下一步需要打磨

对照更多正面、背面和侧面参考进行手工雕刻；尤其需要细化面部形体、肩肘连接、手指和皮肤质感；重新拓扑并做更平滑的骨骼权重；在 UE 里制作动画蓝图和碰撞调校。当前是可继续修改的站立基础模型。

## 本轮对照的公开参考

- **首要依据**：用户提供的 `Big_Laugh.png` 与后续明确要求。保留原图的浅米色腹部、黄皮肤、深棕色指趾及侧面梨形轮廓；双臂采用用户指定的自然垂下姿势。
- [奶蛙站立姿态图片](https://dl-file.cyberlink.com/web/content/nai_long_frog_beff1a0fe4.jpg)：辅助推断不大笑时的侧面腹部轮廓和双臂位置；这是另一张二创图，并非用户原图的正交视图。
- [奶蛙二创示例](https://www.vgover.com/news/198094)：核对这一网络变体的小头、较大身体与捧腹手势。

奶龙官方形象仅说明奶蛙的衍生来源，**不作为本模型的头身比例、脸部或四肢造型参考**。网上同名图片存在差异，发生冲突时以用户提供的原图和后续反馈为准。

## 本轮记录（2026-10-03）

前版保存在 `iterations/before_continuous_head_torso_revision`。已在 Blender 5.2.2 中重新生成预览和 FBX；FBX 重导入检查骨架、四段动作、身体及双眼顶点颜色。各 Python 脚本以自身目录定位输入输出，已取消旧的 Documents 固定路径。

## 头部收减记录（2026-10-03）

前版位于 iterations/before_smaller_head_depth_revision。本轮收窄两颊、减薄后脑并缩短厚重脸颈过渡；更新 Blender 工程、FBX、全部预览及可重复生成脚本。revise_head_volume.py 每次固定从前版工程开始，避免对已收减的模型再次累积收减。

## 头部上移与颈段延长记录（2026-10-03）

本轮前版完整备份位于 iterations/before_raised_head_neck_revision。头部和全部五官上移 0.14；只对原头身连接带分配新增长度，并作宽缓前颈与轻微侧向截面调整。保护区身体坐标误差为零，头部与五官的平移误差小于 0.000001 Blender 单位。已保留十根骨骼、四段动画、颜色和权重。

重新执行本轮修改：在 Blender 后台运行 revise_neck_extension.py；它始终读取本轮前版备份。validate_head_revision.py 对比保存工程；validate_fbx.py 重新导入 FBX。完整脚本重建检查另存于 iterations/raised_head_neck_rebuild_check，不覆盖主工程。

## 颈部两端宽范围融合记录（2026-10-03）

本轮前版完整备份位于 iterations/before_broad_neck_blending_revision。已消除头根和颈下的横向直边；通过覆盖接缝两侧的几何平滑改变曲率，不仅磨圆一条窄带。窄颈中段保持原位，前颈保留宽缓喉弧。轮廓修顺后，检查法线方向、单位长度、平滑面、自定义法线及锐边。

- neck_blend_sculpt.py：本轮上下接缝宽范围几何融合模块。
- revise_neck_blend.py：上一轮接缝平滑驱动，会生成头部前移之前的版本。当前请用 revise_short_round_face.py。
- validate_neck_blend.py：上一轮宽范围平滑的历史核验脚本。当前请用 validate_short_round_face.py。
- neck_blend_validation.json：上一轮接缝平滑的历史核验记录。当前请查看 short_round_face_validation.json。
- naiwa_neck_gray_three_quarter.png：灰色材质斜侧近景，供检查几何与法线连续性。
- finish_neck_blend.py：上一轮交付打包脚本。当前请用 finish_short_round_face.py。

前一轮记录描述当时的保护范围；本轮已按最新要求将形体调整扩展进入下脸、后脑下缘和上胸。完整重建检查另存 iterations/broad_neck_blend_rebuild_check。

## 头部前移与颈部走向记录（2026-10-04）

本轮前版完整保存在 iterations/before_forward_neck_direction_revision。头部及上颈适度向前移动，在上胸归零；不再抬高头顶，也不缩放头部。仅重新整理颈部的截面、前后位置和走向。

前侧用宽缓曲线形成下脸—喉部—上胸的关系；上颈用共享曲率的连续曲线减弱立即向两侧展开的趋势。后颈采用全长度连续斜弧，并将调整扩到整个后侧弧面。表面整理仅作用于 X/Y，Z 轴完全锁定；避免改变颈段长度。

- neck_direction_sculpt.py：本轮头部前移和颈部形状、方向调整模块。
- revise_neck_direction.py：固定读取本轮前版，生成当前工程、FBX 和十七张预览，避免位移累积。
- validate_neck_direction.py：独立核验头部刚体平移、固定高度、五官世界坐标、上颈截面、肩臂、拓扑、颜色、骨架、权重和法线。
- neck_direction_validation.json：本轮核验结果。
- finish_neck_direction.py：打包当前工程、预览、脚本、记录和各轮必要前版。

完整脚本重建检查保存在 iterations/forward_neck_direction_rebuild_check。原先“窄颈中段原坐标固定”属于上一轮记录；本轮保持其长度，并按照最新要求调整它的截面和前后位置。

## 头侧与颈部纵向曲面修顺记录（2026-10-04）

本轮以实际打开的 Blender 场景为基准，完整备份在 `iterations/before_longitudinal_surface_revision`。该场景尚未应用上节的前移版本；项目目录原存档另备份在 `iterations/repository_before_longitudinal_surface_revision`。下面的“保持”均指实际打开场景。

身体所有顶点 Y/Z 坐标完全锁定，侧面整个投影及四个主轮廓关键点保持。采用贯穿整个连接段的纵向厚度拟合与连续曲面插值，修顺眼后下方、头侧和颈中段的横向层带，并将嘴下两侧的展宽柔和分配到下方。前侧喉弧和后侧主轮廓不变。

- `longitudinal_surface_sculpt.py`：本轮曲面模块。
- `revise_longitudinal_surface.py`：当前修改入口，每次固定读取打开场景备份，生成工程、FBX、17 张常规预览及 3 张柔光灰色诊断图。
- `validate_longitudinal_surface.py`：当前独立验证；包含四点锁定、全部 Y/Z、五官、拓扑、权重、颜色、法线、重复顶点、重复面和曲面自交检查。
- `longitudinal_surface_validation.json`：当前验证结果。
- `naiwa_surface_gray_side.png`、`naiwa_surface_gray_front.png`、`naiwa_surface_gray_three_quarter.png`：柔和中性照明的曲面近景；前后对照保存在 `iterations/longitudinal_surface_draft`。
- `render_character_delivery.py`、`render_surface_diagnostics.py`：导出与预览辅助脚本。

历史 `revise_neck_direction.py` 和 `finish_neck_direction.py` 对应原前移版本。当前修改与打包入口见下节。本轮检查段未发现重复顶点、重复面、退化面或非相邻曲面交叉；全部面平滑，法线朝外，无自定义法线或锐边。

## 短圆前脸、后收喉弧与连续后颈历史记录（2026-10-04）

基准为上一轮已展示的曲面修顺版，完整前版在 `iterations/before_short_round_face_revision`。本轮允许重新分配前脸与颈部 X/Y 体积，上一轮“全部 Y 坐标锁定”仅描述上一轮约束。本轮继续保持身体全部 Z 坐标、上部头宽、头顶高度和颈段长度，并在上胸连接处将形变归零。

前脸、喉部、后颈和颈侧展宽采用相邻截面间位置、斜率和曲率连续的曲线。前脸前伸量从中央向两侧减弱；眼球与高光刚体贴合，口缝直接贴合皮肤曲面。独立检查覆盖高度、头宽、保护区、瞳孔布局、眼球刚体误差、口缝贴合、截面单调性、骨骼、权重、颜色、法线、重复顶点、重复面与非相邻曲面自交。

- `short_round_face_sculpt.py`：前一轮体积模块，仍是完整建模的依赖；本轮复用其刚体五官贴合函数，保持口缝高度。
- `revise_short_round_face.py`：历史修改入口，生成本次精修之前的版本。
- `validate_short_round_face.py`：前一轮独立检查。
- `short_round_face_validation.json`：前一轮检查记录。
- `short_round_face_camera_settings.json`：三视图机位记录；三个全身视图均为平视正交，距离 7、镜头设置 50 mm、正交尺度 3.45，角度为正面 0°、侧面 90°、四分之三 35°，使用相同场景灯光。柔光灰色近景同样保持三视图的距离、镜头、尺度与照明一致。
- `finish_short_round_face.py`：历史压缩包更新入口。

前后灰模对照保存在 `iterations/short_round_face_draft`。全身彩色视图和常规近景均更新至当前版本。历史前移与纵向修顺驱动仍用于恢复对应历史版本。

## 宽钝前脸与舒缓喉弧历史记录（2026-10-04）

本轮基准为上一版已保存的短圆前脸工程，备份在 `iterations/before_broad_face_arc_refinement`。全部身体与诊断壳顶点的 Z 保持；上胸根部、肩臂保护区、头冠及上部头宽与基准逐项一致。形变仅重新分配 X/Y 体积，向胸根归零，不再次叠加上一轮前伸。

嘴下中央暗影在灰模检查中偏集中，本轮收回最前缘、缓和喉部转弯，并以两侧一阶、二阶导数均归零的连续角向权重扩散到脸颊。正面下颈的展宽分散到更长范围，后颈保持上一版的连续长弧。口缝宽度、高度和正面位置保持，逐点贴合新皮肤。独立检查对保存网格射线采样，量测下脸最大斜率与曲率变化，并核验轮廓单调性、几何完整性、五官刚性、权重、骨骼与颜色。

- `broad_face_arc_sculpt.py`：本轮精修模块。
- `revise_broad_face_arc.py`：历史工程、FBX 和 23 张预览交付入口。
- `validate_broad_face_arc.py`、`broad_face_arc_validation.json`：前一轮独立检查入口与结果。
- `broad_face_arc_camera_settings.json`：当前相机与照明记录。彩色、灰色头颈三视图各自使用距离 7、镜头设置 50 mm、正交尺度 1.03、相同目标位置和同一套柔光；全身三视图正交尺度 3.45。
- `finish_broad_face_arc.py`：前一轮 ZIP 打包入口。

本轮灰模草稿在 `iterations/broad_face_arc_draft`。完整建模脚本独立重建记录在 `iterations/broad_face_arc_rebuild_check`。当前检查段未发现重复顶点、重复面、退化面或非相邻曲面自交；法线朝外、全部面平滑，无自定义法线或锐边。FBX 重导入确认 6 个角色网格、10 根骨骼、4 段动作及身体/眼睛顶点颜色。

## 紧凑浅唇与圆润眼圈历史记录（2026-10-04）

本轮从 `iterations/before_lip_eye_refinement` 固定备份继续修改，形变仅覆盖前侧嘴周的 z=2.18–2.435 区域。用连续截面曲线替换宽大的嘴下凸起，保留一小段浅唇前伸，向两颊、上下柔和衰减。口缝采用细窄浅弧，两端自然收细。

眼圈原斜长轮廓来自完整球体与弯曲脸部的交线，单纯旋转球体无法稳定控制正面圆度。本轮保留网格拓扑、双色纬圈和绑定，将黑色边界与绿色外边界分别重排为正面圆形和轻微竖椭圆，再沿皮肤贴合形成低凸前表面，后部仍埋入皮肤。外边界明确穿入皮肤，避免共面三角形产生边缘闪烁。高光随瞳孔重新贴合。

- `lip_eye_sculpt.py`：上一轮嘴周与眼圈调整模块，仍是完整重建的依赖。
- `eye_projection_measure.py`：按实际眼面与皮肤深度、EyeColor 分类测量正面投影面积和圆度。
- `revise_lip_eye.py`：上一轮工程、FBX、23 张预览交付入口。
- `preview_lip_eye.py`：另存灰模和彩色草稿，不覆盖主工程。
- `validate_lip_eye_refinement.py`、`lip_eye_validation.json`：上一轮独立核验入口与结果，包括主轮廓锁定、眼圈面积/质心/圆度、嘴下回收、浅弧口缝、拓扑/骨骼/权重/颜色及法线/重复/退化/自交。
- `lip_eye_camera_settings.json`：上一轮统一三视图相机和柔光参数。
- `finish_lip_eye.py`：上一轮交付打包入口，检查验证结果、成功标记与 ZIP 完整性。

灰模和彩色对照在 `iterations/lip_eye_draft`；完整脚本重建检查在 `iterations/lip_eye_rebuild_check`。本轮未改变后颈或原喉部截面，身体全部 X/Z 及后半曲面逐点保持。保存工程和 FBX 均独立复检，预览均对应最新模型。

## 连续浅前脸与细口缝记录（2026-10-05）

本轮固定读取 `iterations/before_continuous_front_face` 的圆眼版工程。前侧 z=2.20–2.46 之间以宽范围连续曲线重新分配体积，向两颊及上下边界平滑归零；身体和诊断壳仅改变 Y。取消窄条唇凸起，消除嘴上中央隆起，下脸提前回收，保留嘴缝附近的小幅整体前伸。

眼圈正面轮廓和全部 X/Z 坐标保持。眼圈下根随局部皮肤沿 Y 重新贴合，最大前后调整约 0.00824，避免冻结眼下皮肤后出现鼓边。瞳孔布局、眼圈双色边界、拓扑、颜色和权重保持，口缝浅弧及端部收细保持。

- `continuous_front_face_sculpt.py`：当前局部连续前脸调整与五官贴合模块。
- `revise_continuous_front_face.py`：主工程、FBX 和 23 张对应预览的交付入口。
- `preview_continuous_front_face.py`：固定读取前版，另存独立灰模和彩色草稿。
- `validate_continuous_front_face.py`、`continuous_front_face_validation.json`：独立核验固定边界、眼部实际投影、口缝贴合、前脸截面、拓扑/骨架/权重/颜色和曲面质量。
- `continuous_front_face_build_report.json`：实际形变范围和眼圈根部贴合记录。
- `continuous_front_face_camera_settings.json`：三视图相机和柔光参数；同组视图使用相同镜头、距离、尺度和灯光。
- `finish_continuous_front_face.py`：核对独立验证、FBX 重导入、完整重建及压缩包完整性后更新 ZIP。

草稿保存在 `iterations/continuous_front_face_draft`，完整脚本重建在 `iterations/continuous_front_face_rebuild_check`。本轮只更新磁盘上的原生工程和导出文件；已打开的 Blender 窗口需重新打开工程查看。
