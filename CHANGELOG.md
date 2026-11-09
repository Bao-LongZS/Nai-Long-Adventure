# 开发记录

按时间倒序记录各产线阶段性结论。仓库主页说明见 [README.md](README.md)。

---

## 2026-11-02
**P7「奶罐守卫战」独立子模式：从骨架推进到真实数据链与灰盒视口。** 新增独立 UE 模块 `NaiwaP7Runtime`，不依赖 `NaiwaRuntime`、不含 P4/P5 头文件，主游戏与 Editor target 分别注册。五张 `P7_Alpha01` 候选 DataTable 由源 CSV 持久导入 `/Game/Naiwa/P7/Data/`（Level 1 行、Tower 9 行、Enemy 3 行、Obstacle 2 行、Wave 8 行），`ANaiwaP7AlphaGrayboxGameMode` 连接五表，专属地图 `/Game/Naiwa/P7/Maps/L_P7_Alpha01_Graybox` 已 cook/stage。主工程自动化 14/14 通过，验证真实 DataTable 装配、初始购买扣费、首波及首只敌人刷出。Development 包在 PCD3D_SM6 真实路径启动并生成 1280×720 实图，板格、双路线、8 个建造位、2 个障碍、奶蛋耐久、波次金币和交互提示均已绘出。

**P7 尚未达可玩 Alpha。** 720p HUD 字体明显偏小；键鼠输入（`1`–`3` 选塔、点击建造、`U` 升级、`C` 清障、`N` 开波、`A/B` 切路、`Space` 暂停、右键取消）尚未实测；G0 的项目/设计 owner、参考测试 PC、路线与经济数值签收均未完成，CSV 不能标为批准版。A1 美术（塔、敌人、奶蛋、HUD、VFX）只有塔贴图进了 UE，其余仍在源目录，未做游戏内比例检查。详见[进度交接](UE5/Records/p7_td/P7_PROGRESS_CHECKPOINT_2026-11-02.md)。

**P4 推进到 W3/W4。** 敌人序列与 Boss、VFX 的运行时预览接入；W4 Boss 与 VFX 各有一份 DataTable 驱动帧序；战斗中补了受击伤害数字、屏幕控件提示、卡牌下拉与敌人血条；A1 33 帧在 10-30 做过一轮质量重导，A2 精修竞技场与背景另起版本。同时记录了一轮像素探针，用来定位背景朝向与运行时像素偏差。α 竖切仍未接入选关页，也没有主菜单入口。

**P5 4-1 推进到 M3。** 定步长模拟、坡面、金币、树石、冰缝、雪崩 gap 全部由 fixture 驱动；袋鼠骑乘三点位、四皮肤能力、奶票结算、衣柜购买与装备、存档 v3 迁移、主菜单 4-1 入口都接上了。A0 运行时美术接入继续修：坐骑鞍点 UV 校正，v05 样片之外新增 v06 资源目录。**M1 的手感门、M2 试玩、M3 四个状态用例仍未运行验收**，A1 视觉出口门也未通过，A2 批量制作未启动。

**WebNative 网页原生版成立。** 独立于 UE 的 Three.js + Vite + TypeScript 工程，用 UE 只读入口导出 P1 v7 地图几何、角色骨骼网格与跑步动画（`.glb.gz` 侧车）、P2 基础蛋 GLB；导出器不写任何 `.umap` / `.uasset`。现在能玩 1-1 开场到 CP01；另有 P2 物理探针页走 Rapier WASM，按固定步扫掠记录接触与冲量。Rapier 与 Chaos 未做 parity 对照，手机真机帧基准未测，`p2_ballistics_fixture.json` 的显式 Euler 结果不作为物理真值。

**UE 5.8 编译适配。** 主线一批源文件补 `MakeShared<FJsonObject>().ToSharedPtr()`、`SetStringField(FStringView(...))` 与 `Engine/GameInstance.h`、`Containers/StringView.h`；`NaiwaRuntime.Build.cs` 补 `AudioMixerCore`，并在 Editor target 下显式链接 `UnrealEd`、`PropertyEditor`——从零重建 Runtime 模块时，共享 PCH 不再提供这两个模块的内联符号。`NaiwaRuntime.cpp` 增加 `-NaiwaWebExportP2Scene=` 启动分支，供 WebNative 导出 P2 场景数据。

## 2026-10-29
**P5 A0 背景补底二次修正。** 负责人 669 m 截图表明首版改动确实减少了雪纹条带，但蓝楔仍在。复核发现补底顶缘在路线下方 6.5 m，而侧视正交相机画面下沿约在路线以上 2.5 m，导致补底完全处于视锥下方。这次将沿路线剖面的补底顶缘提高到路线以上 7.5 m，运行版与 Editor Runtime 均编译链接成功（分别 3/3、4/4 actions）。新效果需关闭旧运行进程并重新启动后查看；尚无本次改动后的截图，不能记为视觉通过。详见[根因与修复记录](UE5/Records/p5_m2/P5_A0_背景补底错位与雪纹条带根因修复_2026-10-29.md)、[运行版构建日志](UE5/Records/p5_m2/P5_A0_BackgroundFrustumCorrection_GameBuild_2026-10-29.log)和[Editor 构建日志](UE5/Records/p5_m2/P5_A0_BackgroundFrustumCorrection_EditorBuild_2026-10-29.log)。
**P5 A0 背景横线与断崖可见深度修订（第八轮）。** 新截图仍有贯穿画面的水平雪线，复核发现补底顶缘仍在路线以上 360 cm，正切过山景 PNG 的透明区；曾下移到路线以下 650 cm，仍位于山景后、天空前。断崖暗腔从低侧崖唇下 450 cm 提至 120 cm，并加深至 2200 cm。`Nai_LongEditor` 全目标构建通过（79/79），`Nai_Long Win64 Development` 通过（78/78）；重编时发现的 `UnrealEd`、`PropertyEditor`、`AudioMixerCore` 链接依赖已显式补齐。`UnrealEditor-Cmd -NullRHI` 启动器静默返回 0，但未生成日志/报告，样片地图仍是旧时间戳，因此不记为重建成功。未开 Editor/PIE，实际运行画面和断崖近景待审核。详见[第八轮记录](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_v05_运行画面第八轮背景与断崖修订_2026-10-29.md)、[Editor 构建日志](UE5/Records/p5_m2/Nai_LongEditor_BackdropCliffFix_Eighth_retry2_2026-10-29.log)、[运行版构建日志](UE5/Records/p5_m2/Nai_Long_BackdropCliffFix_Eighth_2026-10-29.log)和[本轮地图重建尝试](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_EditorSample_v05_attempt_20261029_202229.json)。
**P5 A0 背景补底遮挡修复（第七轮）。** 负责人新截图显示上一轮把雪色补底放到远山前方后，形成横向雪墙并遮住远山/云层。现将补底恢复到全部景观图层之后、天空之前，并把顶缘下移到景观下沿附近，只填透明底部区域；断崖暗腔仍从低侧崖唇下 450 cm 开始。`Nai_LongEditor` 增量构建通过（4/4 actions），`Nai_Long Win64 Development` 运行版构建通过（78/78 actions）；v05 自有样片地图以 `NullRHI` 重建，12 张 WIP PNG 已重导入。未开 Editor/PIE，运行画面仍待负责人复核。详见[第七轮记录](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_v05_运行画面第七轮背景遮挡修订_2026-10-29.md)、[样片报告](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_EditorSample_v05_build_report.json)、[Editor 构建日志](UE5/Records/p5_m2/Nai_LongEditor_BackdropOverlapFix_final2_2026-10-29.log)与[运行版构建日志](UE5/Records/p5_m2/Nai_Long_BackdropOverlapFix_Game_2026-10-29.log)。

**P5 A0 背景补底与断崖暗腔第六轮修订。** 根据负责人最新运行图，雪色补底前移到远山前、森林与雪脊后，并抬高顶缘，覆盖坡面下缘的蓝色漏空；断崖暗腔改从低侧崖唇下 450 cm 开始，避免暗面高过崖唇而形成墙/坡，同时保留两侧窄肩和上方开放跳跃通道。v05 导图脚本现在每次都从 12 张 WIP 源 PNG 重导入，报告同步运行时图层锚点、补底深度顺序和暗腔尺寸。`Nai_LongEditor Win64 Development` 后台构建成功（80/80 actions）；v05 自有样片地图以 UE 5.8.3 `NullRHI` 命令行重建成功，所有权、GameMode、贴图数和背景组件检查通过。未打开 Editor 或运行 PIE，最终画面与断崖近景仍待负责人审核。详见[本轮记录](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_v05_运行画面第六轮背景与断崖修订_2026-10-29.md)、[地图报告](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_EditorSample_v05_build_report.json)与[构建日志](UE5/Records/p5_m2/Nai_LongEditor_BackdropCliffFix_2026-10-29.log)。

**Nai_LongEditor 全项目构建修复。** 修复 P1–P4 源文件在独立编译下缺失的类型声明与头文件、P1 UE 5.8 JSON 字段接口，以及 P4 临时 DataTable 外层类型。后台 `Nai_LongEditor Win64 Development` 编译并链接成功，79/79 actions；P5 最新轨道表现代码一并链接进 Runtime DLL。未打开 Editor 或 PIE。详见[构建修复记录](UE5/Records/p5_m2/Nai_LongEditor_全项目构建修复_2026-10-29.md)与[构建日志](UE5/Records/p5_m2/Nai_LongEditor_BuildFixPass1_NoSharedPCH_2026-10-29.log)。

**P5 A0 背景透明区与断崖边缘复修。** 455 m 截图中的蓝色三角来自雪脊/森林图层透明下沿与独立雪坡填充之间的空隙；新增跟随路线高度的雪色衔接层。断崖补入连续下凹暗部及两侧窄岩壁，保留中间跳跃通道。首轮构建发现的 P1–P4 编译阻断已在最新全项目构建记录中修复（79/79 actions）；未开编辑器或 PIE。详见 [P5 A0 背景衔接与断崖复修记录](UE5/Records/p5_m2/M2_A0_背景衔接与断崖复修_2026-10-29.md)。

**P4 W2/W3 受击伤害数字。** 表现层按玩家与敌人的实际 HP 损失显示浮动数字；敌人受击为黄色，玩家受击为红色，约 0.9 秒内上浮淡出。仅在 HP 确实下降时显示，因此格挡/减伤后展示的是实际扣血值。`Nai_LongEditor Win64 Development` 编译通过（6/6 actions）；尚未在 Editor/PIE 复核文字朝向与屏幕位置。P5 当前没有敌人 HP/伤害结算路径，本次数字接在 P4 竞技场上。实现见 [P4 ArtIntegrationPawn](UE5/NaiwaP0/Source/NaiwaRuntime/P4/Presentation/NaiwaP4ArtIntegrationPawn.cpp)。

**P5 A0 背景层间摆放与透明过渡修复。** 静态按 2300 cm / 16:9 相机窗口复合后发现，近景雪脊透明轮廓只到世界 Z≈+26 cm，低于平地相机视野下沿约 +73 cm，导致它在运行画面中基本缺席；森林底缘渐隐也被 Masked 材质截断为硬边。近景锚点已从 -20 调到 +260 cm，使雪脊进入画面下沿约 233 cm；UE 5.8.3 后台材质 Commandlet 已重建 12 张纹理及对应材质，far/mid/near 三层均读回 `BLEND_TRANSLUCENT`，三个材质 UAsset 已在 15:34 更新。新增 manifest v08 和 293/1123 m 静态复合预览。Runtime 与材质构建通过；仍需负责人开图确认真实雪道衔接、构图、透明排序与平铺观感。详见[修复记录](UE5/Records/p5_m2/P5_A0_BackgroundPlacement_修复记录_2026-10-29.md)。

**P5 A0 背景横线与雪道占屏二次修复。** 负责人在 293 m、1123 m 两张新图里确认固定高度横线、森林齐底硬边仍在，上一轮构图修订效果为负。定位后将三张背景做透明上下扩边（2048×1536 画布），保留原插画像素比例；森林底部 32 行渐隐并下移图层，镜头基准 600→720 cm 以进一步压低雪坡占屏。背景新资产横向首尾 RGBA 差异为 0；编译时发现 P4 状态结构中存在一个未引用重复成员，移除重复声明后 Editor 链接通过（13/13 actions）。材质专用 Commandlet 重建 12 张纹理/材质，报告确认未加载地图。按负责人安排未开图/PIE；运行时接合、雪坡占比和重复节奏仍待负责人复看。详见 [背景二次修复记录](UE5/Records/p5_m2/M2_A0_背景反馈复修_2026-10-29.md)。

**P5 A0 背景衔接、雪道占屏与清晰度修订。** 根据最新 657 m 画面修复远山/森林高度比例：两张 2048×512 背景不再按 2048×768 拉伸，按原始宽高比接入并与相邻图层重叠；远山和森林新导出图保留全部透明组件且横向首尾 RGBA 列完全一致。相机基准上调到 600 cm，关闭景深和运动模糊并增加轻度锐化；背景纹理 mip 调整为 `TMGS_SHARPEN2`。`Nai_LongEditor Win64 Development` 编译成功（5/5 actions）；材质专用 Commandlet 重导入 12 张纹理并重建对应材质，报告确认未加载地图。平铺预览未见硬接缝，游戏内层间接合、压缩过滤与重复节奏仍待负责人自行开图复核；按安排未打开关卡或运行 PIE。详见 [A0 背景与清晰度修订记录](UE5/Records/p5_m2/M2_A0_背景衔接与清晰度修订_2026-10-29.md)。

**P5 动态质感标准与连续角色动画需求补充。** 按项目负责人要求以《滑雪大冒险》的速度读感为表现标尺，新增慢速/巡航/高速/技巧加速四档反馈规范，并将奶蛙 A4 从静态状态帧补充为约 23 帧的连续动作序列要求。同步修订工程计划、美术执行计划和 A4/A5 资产要求书；复用现有雪粉/板迹资源优先，固定跳高、速度物理与清晰度原则不变。新增帧数和速度效果不在原 40 帧/10.5 美术人天基线内，A2 前需更新 manifest 与工时估算。A1 出口门仍未通过，A2 未启动；本轮仅文档，不开地图、不运行 PIE。详见[速度与连续动作美术需求](UE5/Records/p5_p6_plan/P5_速度与连续动作视觉表现美术需求_v01.md)。

**P5 A1 样片推进：修正静置袋鼠方向。** 负责人反馈 A5 图集左上 Idle 帧在雪道静置状态下左右反向；仅静置道具在图集单元内镜像 U，保留正立所需 V 翻转，骑乘态 DownhillGlide 采样不变。编辑器关闭后增量编译/链接完成（4/4 actions），Runtime DLL 已更新，UBA 有非致命缓存结果写入警告。A1 样片集成/验证标记为进行中；视觉出口证据尚缺，A2 批量制作仍未启动。按负责人要求未打开关卡或运行 PIE。详见 [A1 推进记录](UE5/Records/p5_m2/P5_A1_样片集成与验证推进_2026-10-29.md)。

**P5 A0 雪道占屏第二轮收敛与阶段门结论。** 镜头基准 Z 从 470 提至 520 cm，雪体厚度 95→80 cm、世界纹理重复间距 300→450 cm，中景林线 Z 100→60 cm。1688 m 运行截图支持进入 A1 样片复核，但最新参数尚待负责人开图确认；按负责人要求本轮未打开地图/PIE。当前不能进入 A2 批量制作：1080p/720p 校准、尺寸尺实测、gap 档位、断崖近景、循环接缝与 Editor p95 均未齐。P5 Pawn/TrackActor `.obj` 与 Runtime DLL 已生成，后续 UBT 增量复核为 up-to-date；首次全目标 UBA 执行受 PCH 访问拒绝及内部错误影响，未作为干净全目标构建通过记录。详见 [阶段门复核](UE5/Records/p5_m2/P5_A0_阶段门复核_2026-10-29.md) 与 [A0 表现修订记录](UE5/Records/p5_m2/M2_A0_角色断崖与图层表现调整_2026-10-29.md)。

**P4 ArtIntegrationWidget 构建阻塞复核解除。** 之前的构建日志在 P4 Widget 头/实现同步期间报成员未声明；核对当前文件后，预警控件与函数声明已齐全。重新构建编译 P4 PlayerController/Widget，随后 NaiwaRuntime `.lib`、`.dll` 和 target metadata 均完成（6/6 actions）；UBA 另报部分 action 结果缓存写入失败，但没有 C++ 编译错误。未运行 PIE。详见 [重编日志](UE5/Records/p5_m2/Editor_rebuild_after_P4_widget_sync_2026-10-29.log)。

**P5 A0 角色、断崖、图层与雪道占屏复调。** 中性空中状态改用完整 Glide 雪板剪影，角色放大，Atlas MID 创建失败时回退至 Glide 材质；断崖移除深色方块并用现有雪岩素材铺设下半部，保留上方断口；背景纹理放大并拉开深度层，雪体填充后移到前景树木之后；镜头高度提高 110 cm 以减少雪坡占屏。P5 Pawn 与 TrackActor 编译动作通过；Editor 全目标未能链接，停止于现有 P4 `NaiwaP4ArtIntegrationWidget.cpp` 编译错误。未运行 PIE。详见 [P5 A0 表现修订记录](UE5/Records/p5_m2/M2_A0_角色断崖与图层表现调整_2026-10-29.md) 和[构建日志](UE5/Records/p5_m2/P5_A0_visual_feedback_build_2026-10-29.log)。

**P4 A4 返修卡牌布局与敌人攻击提示。** 移除屏幕中央红条；训练敌攻击前摇时在敌人头顶显示红色“！”。Tab 选择栏仍为顶部下拉，但改用 420–480 高度、288 高卡片框，图标左置、文字右置并分区，防止内容挤叠。UE 5.8.3 隔离 scratch 编译通过 4/4 actions；没有启动 PIE，实际可读性待窗口复核。详见 [W2 工程实现记录](UE5/Records/p4_alpha/W2_工程实现记录.md) 与 [编译日志](UE5/Records/p4_alpha/evidence/W2/p4_w2_card_layout_enemy_marker_compile_20261029.log)。

**P4 A4 扩大场地并修正角色比例。** 按负责人反馈将隔离预览的逻辑纵深由 260 扩至 420 uu，场景块按比例延展到前后空白；保持横向尺寸、相机角度与 W1 默认值不变。A1 立绘高度由 180 调至 200 uu，宽度保持不变并同步脚底锚点补偿。UE 5.8.3 隔离 scratch 最新增量编译通过 4/4 actions；未运行 PIE，视觉效果待开图复核。详见 [W2 工程实现记录](UE5/Records/p4_alpha/W2_工程实现记录.md) 与 [编译日志](UE5/Records/p4_alpha/evidence/W2/p4_w2_expand_arena_taller_sprite_compile_20261029.log)。

**P4 A4 预览调整场地纵深与透视。** 保留已接近满屏的横向宽度，将 P4 隔离预览纵深由 185 提至 260 uu，并把相机俯角从 57° 调至 50°；A4 标记场景块仅运行时按比例扩展，角色阴影稍加宽，主工程设置和地图资产不变。隔离 P4 scratch 编译 5/5 actions 成功；未开图或运行 PIE，待负责人自行体验。详见 [W2 工程实现记录](UE5/Records/p4_alpha/W2_工程实现记录.md) 与 [编译日志](UE5/Records/p4_alpha/evidence/W2/p4_a4_depth_camera_compile_20261029.log)。

**P5 A0 起跳改为单按固定跳高。** 按负责人反馈取消按住蓄力和松键截断：空格按下一次立即起跳，跳高由固定初速/重力决定，按住不会继续加高。接地宽容缓冲保留；同步清除 HUD 中“按住增高”提示，并将预备姿势限制为 0.08 s 的视觉反馈，不延迟实际起跳。UE 5.8.3 `Nai_LongEditor Win64 Development` 编译成功（11/11 actions）。未打开关卡或运行 PIE。详见 [单按起跳调整记录](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_v05_单按起跳调整_2026-10-29.md) 与 [编译日志](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_VisualAdjustCompile_2026-10-29_v04.log)。

**P5 A0 v05 运行画面第四轮调整。** 负责人新截图反馈底部留白、骑乘时奶蛙消失、上坡雪板角度生硬和起跳卡手。A0 镜头构图下移并降低空中垂直跟随、背景层固定在路线高度；骑乘态保留奶蛙并叠在袋鼠前方；角色/坐骑立绘随路线切线旋转。跳跃改为相对雪道高度积分，加入松键短跳、按住增高和更宽松的缓冲/土狼时间，避免上坡地形抵消跳跃弧线。另整理 A4/A5 分层资产要求书，要求雪板、奶蛙、袋鼠独立分层并增加骑乘姿势。UE 5.8.3 `Nai_LongEditor Win64 Development` 编译成功（18/18 actions）；未开关卡或 PIE，实际构图与操作手感待负责人复核。详见 [第四轮调整记录](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_v05_运行画面第四轮调整_2026-10-29.md)、[A4/A5 资产要求书](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_A4雪板贴坡与骑乘角色资产要求书_2026-10-29.md) 与 [编译日志](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_VisualAdjustCompile_2026-10-29_v03.log)。

**P5 A0 v05 运行画面第三轮修复。** 根据负责人最新截图修正奶蛙 atlas 的左右朝向、翻正袋鼠骑乘帧；把天空与三层背景按玩家含跳跃高度跟随，避免跳跃时镜头越过背景出现黑屏；雪道改为按 1 m 采样的连续程序网格，并以三次 Hermite 插值平滑路线高度，保留既有冰隙/断崖断口。UE 5.8.3 `Nai_LongEditor Win64 Development` 编译通过（14/14 actions）。未打开关卡或运行 PIE；实际朝向、层次和跳跃画面需负责人自行开图复核。详见 [第三轮修复记录](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_v05_运行画面第三轮修复_2026-10-29.md) 与 [编译日志](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_VisualAdjustCompile_2026-10-29_v02.log)。

**P5 A0 v05 运行画面二次调整。** 负责人提供 898 m/1048 m 截图后定位背景视差图块落后于全速跟随镜头，改成图块贴镜头覆盖视野、UV 偏移负责视差；同时翻正 0° 空中奶蛙姿势并把可视雪面从 1.5 m 加厚至 2.2 m。UE 5.8.3 `Nai_LongEditor Win64 Development` 主体调整编译 18/18 actions 成功，最终增量编译 4/4 actions 成功。未打开关卡或运行 PIE；背景接缝/层级、角色姿态/锚点及雪面厚度待负责人自行开图复核。详见 [二次调整记录](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_v05_运行画面复核与二次调整_2026-10-29.md) 和 [编译日志](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_VisualAdjustCompile_2026-10-29.log)。

**P4 W2 负责人画面反馈修正。** 普攻键改为 J；修正角色立绘朝屏幕左右的翻转、奶球前置出生与金色辨识度；A4 预览增加运行时场地下衬面，补掉舞台外黑区。相机关闭景深/运动模糊、降低 Bloom 并增强 Sharpen，固定步呈现加插值以改善清晰度和移动观感。隔离 scratch 最终增量编译通过 5/5 actions；未开图或运行 PIE，画面和手感待负责人复核。详见 [W2 工程实现记录](UE5/Records/p4_alpha/W2_工程实现记录.md) 与 [编译日志](UE5/Records/p4_alpha/evidence/W2/p4_w2_feedback_responsiveness_compile_20261029.log)。

**P4 W2 接入战斗灰盒。** 固定步模拟现在支持 J 按住自动索敌奶球、Shift 冲撞/两格充能/冲刺与尾段无敌、Space 防御减伤、E 满能量大笑范围伤害/回血/清敌弹，并以临时追击敌供 A4 预览试玩；HUD 读取真实生命、能量和充能，`p4s2` 哈希扩展覆盖战斗状态。训练敌仅在运行时生成，不改关卡资产。实机手感、屏幕方向与 HUD 验收由项目负责人开图反馈；笑场/Boss 反制和 W2 试玩退出门仍未完成。详见 [W2 工程实现记录](UE5/Records/p4_alpha/W2_工程实现记录.md)。

**P5 A0 v05 材质显示故障修复。** 负责人提供的运行截图对应材质图在 PCD3D_SM6 编译失败：UV 的 float2 与向量参数 float3 直接相乘，UE 因此回退默认材质。现给 UV 缩放/偏移添加 RG 分量遮罩，材质专用 Commandlet 重建 12 张贴图材质和天空材质（0 errors）；骑乘态从绿色方块切换到 A5 袋鼠下坡 atlas 帧。Editor Development 增量编译通过 10/10 actions。未打开地图或运行 PIE；修复后画面和 A0 视觉验收待负责人复核。详见 [P5 A0 修复记录](UE5/Records/p5_p6_plan/P5_A0_运行时美术接入实施记录_2026-10-28.md)、[材质报告](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_v05_material_repair_report.json) 和 [编译日志](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_RideArtCompile_2026-10-29.log)。

**P5 单按高跳、摔倒恢复输入与雪坡画面修订。** 实机截图显示固定跳弧偏低，且恢复期玩家无法起跳；按当前单按固定高度方向，将起跳初速提至 13 m/s、重力保持 18 m/s²，纸面峰高约 4.6 m，并在恢复末段开放起跳缓冲。A0 侧视雪坡厚度由 220 cm 收至 95 cm，并沿坡面补雪体填充，冰缝/断崖仍保留开口，减少大块斜板与坡下空蓝。模拟状态哈希升至 v5。UE 5.8 Editor Development 最终增量编译通过 7/7 actions；未运行 PIE，需负责人检查修复后实际手感与画面。详见 [P5 跳跃与雪坡修复记录](UE5/Records/p5_m2/M2_跳跃与雪坡表现修复_2026-10-29.md) 和 [编译日志](UE5/Records/p5_m2/P5_jump_art_feedback_build_2026-10-29.log)。

**P4 W2 修正屏幕移动方向与预警文案换行。** 固定机位 Yaw 90° 使世界 X 正方向投影到屏幕左侧；A1 Presenter 现把逻辑屏幕 X 映射到反向世界 X，输入/模拟坐标保持“右为正”。右上预警样式栏加宽并调小字体、关闭自动换行。UE 5.8.3 `Nai_LongEditor Win64 Development` 隔离 scratch 编译通过；本轮未开关卡或运行 PIE，实际操控与 HUD 排版待负责人检查。场地尺寸/相机参数保持不变，扁平观感需结合负责人开图后的画面再定。详见 [W2 工程实现记录](UE5/Records/p4_alpha/W2_工程实现记录.md) 和 [编译日志](UE5/Records/p4_alpha/evidence/W2/p4_screen_controls_compile_20261029_final.log)。

## 2026-10-28

**P5 A0 运行时表现与 v05 样片已接入，首帧复核未通过。** 工程侧完成三层连续视差、路线贴合雪面、A4/A5 静帧表现和紧凑 HUD；侧视金币朝向与袋鼠静帧高度已调整，Editor Development 增量编译 4/4 actions 成功。v05 无界面建图成功，12 张 WIP 贴图和隔离材质已导入。负责人提供 308/1800 m 游戏窗口截图：远中近背景未显现，画面被灰网格/浅蓝空场切开；雪道仍是深灰分段块，断口醒目；奶蛙只呈小色块。HUD 位置较克制但字号偏小。今日只分析归档、不改代码；按负责人要求将工程开工留至 2026-10-29。截图和排查顺序见 [v05 运行画面简析](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_v05_运行画面简析_2026-10-28.md)。贴图/材质导入成功不代表运行时可视链路或视觉出口门通过。

**P4 W1 负责人放行并开始 W2。** 负责人接受当前 W1 标定画面和软影观感，决定轻微偏亮不再阻断。未采集的运行时卡表日志、动态 DepthY、A1 实际锚点与目标机性能转后续 A6 收口；不冒记为已验证。W2 已实现固定步输入缓冲、玩家移动/朝向和 P4 专属 Enhanced Input 控制器，隔离 UE 5.8.3 scratch 最近增量编译通过（7 actions）。详情见 [W2 工程实现记录](UE5/Records/p4_alpha/W2_工程实现记录.md)。

**P4 W1 软影材质小幅增强。** 按截图把软影不透明度强度从 0.45 调至 0.58；此前遗漏保存的 0.45 回退副本已在 retry2 显式保存并重新加载读回。UE 5.8.3 commandlet 退出码 0，主材质读回 0.58、回退副本读回 0.45。当时画面复核尚未完成；负责人随后接受了当前画面并放行 W2，详见上方放行记录。报告见 [retry2 软影调整记录](UE5/Records/p4_alpha/evidence/W1/P4_W1_soft_shadow_tune_report_retry2.json)。

**P4 A1 清理帧获批并完成首轮 UE 导入。** 按负责人审核结果，将 33 张逐帧清理副本作为独立 Texture2D 导入 `/Game/Naiwa/P4/Art/Alpha/A1/Frames/`；SHA-256/尺寸预检及 sRGB、alpha、Character 纹理组、Sharpen0 mip、默认压缩读回均通过 33/33。UE 5.8.3 命令行报告 0 errors / 67 warnings，导入状态为 `imported_pending_visual_calibration`。未打开或切换关卡，未运行 PIE/Standalone。脚底锚点、运行时边缘采样、720p/1080p 固定机位检查和 W1 门仍待负责人复核。报告见 [A1 导入报告](UE5/Records/p4_alpha/evidence/A1/P4_A1_clean_frame_import_report.json)。

**P5 M2 按新手感反馈扩展至 1800 m。** 空格改为按下即起跳、按住增高；跳跃缓冲/土狼时间放宽。路线新增两处自动弹射断崖，成功落地前空翻一圈/两圈分别触发 32/38 m/s 上限的一级/二级加速，并在 HUD 显示空中旋转和 boost。新路线 fixture 为 v5 / `BBB3D46E`，纸面无碰撞时长约 84.11 s。UE 5.8 Editor Development 首次完整构建通过 16/16 actions，修正断口高度/速度查询及路线 ID 后最终链接构建通过 5/5 actions。没有打开关卡或运行 PIE，地图由项目负责人自行打开体验。详见 [P5 M2 手感反馈修订记录](UE5/Records/p5_m2/M2_手感反馈修订记录_2026-10-28.md)。

**P4 固定步进不再丢弃过载模拟步。** 单帧 DeltaTime 对应步数超过 catch-up 上限时，旧实现会跳过剩余步数，导致过载帧率下模拟 tick 进度偏离输入时间。现保留积压并让后续 Tick 继续追赶，同时暴露 `GetPendingCatchUpSteps()`；隔离 UE 5.8.3 C++ 增量编译通过 **5/5 actions**。本次不启动 Editor/PIE，W1 的视觉与目标机帧时间验收仍待项目负责人开图检查。详见 [P4 工程计划](UE5/Records/p4_alpha/P4_工程计划_v2.md) 与 [G0/W1 记录](UE5/Records/p4_alpha/G0_W1_启动记录.md)。

**P4 增加 Upgrade DataTable 运行时目录。** P4 ArenaSubsystem 现在能校验并读取卡表为稳定排序快照，检查前置依赖环；W1 Preview GameMode 在实际启动时记录卡行数和 P0/P1/P2，便于负责人核对重导入值。UE 5.8.3 隔离增量 build **4/4 actions 成功**；未运行预览图，运行时读取证据待负责人开图。此项未实现卡牌战斗效果，也未关闭 W1。

**P5 美术推进状态更新。** 当日先暂停交接，之后项目所有者恢复推进并通过 A5 坐骑制作基底；A1 近景雪脊新增 v03 静态候选，RGBA 首尾边缘列相同，但切线/纹理及 UE 样片仍待验收。早期通过资产索引保留为当时交接快照，不代表后续停止。见 [当前美术执行记录](UE5/Records/p5_p6_plan/P5_美术资产执行记录_2026-10-28.md)、[A1 v05 复核](UE5/Records/p5_p6_plan/evidence/P5_A1_Backgrounds/P5_A1_Backgrounds_Review_v05.md) 与 [A1 v05 manifest](UE5/SourceAssets/P5/Manifests/P5_A1_Backgrounds_manifest_v05.json)。明确排除的旧奶蛙表和泛化滑雪袋鼠图仍不在 P5 资产目录。

**P5 A4 奶蛙六姿态首稿确认并完成透明 atlas 候选整理。** 项目所有者确认最新六姿态雪板图是正确造型。导出脚本从透明图层的六个连通角色轮廓分别提取像素，避免相邻外接框重叠导致手/雪板串格；预览六姿态完整、无邻格残片。atlas 与脚底锚点仍是 WIP，未接入 UE Editor。复核记录见 [P5 美术资产执行记录](UE5/Records/p5_p6_plan/P5_美术资产执行记录_2026-10-28.md)。

**P5 A5 袋鼠角色边界纠正。** 项目所有者提供的 `C_Kangaroo_Mount_Reference_v01.png` 确认是美团袋鼠，定位为关卡内坐骑道具，不是玩家操作角色。偏离参考的生成动作候选未纳入资产；当前只登记造型参考，坐骑源稿待按该参考制作。

**P5 A2/A3/A6 规格候选导出。** 雪面/滑痕、三种树与两种雪石、六类视觉特效（含 12 帧落地雪雾 atlas）已有可重建 WIP 导出与 manifest；贴图实际循环、碰撞轮廓、透明边过滤、帧时序和 Editor 场景验收仍待处理。A1 近景雪脊接缝继续保持未通过状态。

**P5 A7 建立 16 色专用候选色板。** 色值表从 P2 浅天空色与 P3 钢蓝/冷灰/低饱和暖色家族提取基准，并注明新增中间色的派生来源。静态色板 PNG 可由脚本按 JSON 重建；同屏明度、1080p/720p 可读性及用户确认待真实样片评估。见 [P5 A7 色板复核](UE5/Records/p5_p6_plan/evidence/P5_A7_Palette/P5_A7_Palette_Review_v01.md)。

**P5 M1 静态审查修复摔倒后无法再跳，并对照 P2/P3 建图成功日志。** 摔倒恢复结束时，灰盒模拟此前没有恢复雪面接触状态；现于恢复计时达到 2.5 s 时恢复 `bOnSnow` 并清零土狼计时，保留之后 0.5 s 的无敌窗。源码增量编译通过（4/4 actions）。P2 2-3 与 P3 日志确认 `UnrealEditor-Cmd.exe -run=PythonScript -script=...` 是本机已成功的方法；P5 按该方式重试仍未生成地图报告或 `.umap`，未打开 P5 关卡或运行 P5 PIE。详情见 [M1 灰盒实现记录](UE5/Records/p5_m1/M1_灰盒实现记录_2026-10-28.md)。

**P5 M1 工程侧增量推进。** 固定步进每帧仍最多处理 4 步，但改为保留超额时间并暴露待追赶步数，避免丢模拟步后跨过路线事件；灰盒地面延伸至完整 100 m，树与终点 marker 贴合地面。UE 5.8 `Nai_LongEditor Win64 Development` 编译通过（最近增量 4/4 actions；首轮完整 P5 编译 12/12 actions）。地图未生成或打开，PIE/Standalone 未验收；地图 Editor 工作由项目负责人接手。详见 [M1 灰盒实现记录](UE5/Records/p5_m1/M1_灰盒实现记录_2026-10-28.md)。

**P5 M0 数值层与 M1 灰盒运行时开工。** 新增固定步长的 P5 WorldSubsystem、跳跃/旋转/落地/gap/计分纯函数、共享皮肤目录，以及临时 100 m 灰盒 Pawn、赛道、HUD 和 GameMode。路线事件 fixture 与专属地图生成脚本已准备；初始源码状态未编译，现已由上方记录补充编译进度。灰盒坐标仅供工程切片，不是正式路线。见 [M0 工程切片记录](UE5/Records/p5_p6_plan/M0_工程切片记录_2026-10-28.md)、[M1 灰盒实现记录](UE5/Records/p5_m1/M1_灰盒实现记录_2026-10-28.md)。

**P5 A0 首轮美术方向样片。** 按全量 B 规格生成首版奶蛙滑雪透明姿态稿、清晰侧视雪坡方向稿与独立 HUD 占位，并记录输入、尺寸、透明信息和合成脚本。项目所有者看过放大肚皮/平静眼神的提议稿后选择保留首稿。当前交付供方向评审，尚非 Editor 样片或最终分层资产；需在真实路线 fixture 中复核尺度、层次、碰撞贴合和 720p 可读性，A0/A1 门通过后再批量生产。见 [P5 A0 美术记录](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_ArtDirectionReview_v01.md)。

**P5 A0 构图与自然色复评。** 按项目所有者意见创建场景 v02，压低高亮与电蓝饱和度，同时保留原构图及首稿角色。画幅接近标准 16:9，HUD 占比适中；静态投影估算发现最近岩石若按实际障碍放置只提前约 0.2 s 可见，真实玩法锚点/视野需在 Editor 样片中前移或增加预告。完整测量假设和限制见 [构图复评](UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_CompositionReview_v02.md)。

**项目所有者通过 P5 v02 方向样片并授权继续美术生产。** 按冻结清单进入可集成素材切片；Editor 标尺、实际坡面/碰撞、720p 细节和性能证据仍由后续样片验收补齐。

**P5 A1 三层背景候选稿。** 远山、林线、近景雪脊已整理为规格尺寸 PNG 并提供可重建导出脚本；双平铺预览显示远山/林线接缝暂可继续送 Editor 复核，近景雪脊首尾高度差明显、暂不通过循环验收。所有图标记为 WIP，未导入运行时；透明边、实际视差、角色/障碍同屏层级仍待真实 Editor 样片检查。详见 [P5 A1 背景清单](UE5/SourceAssets/P5/Manifests/P5_A1_Backgrounds_manifest_v01.json) 与 [平铺复核记录](UE5/Records/p5_p6_plan/evidence/P5_A1_Backgrounds/P5_A1_Backgrounds_Review_v01.md)。

**P3 正式关卡接入主线。** 关卡选择扩展为六关并显示 P3 解锁/通关记录；2-3 完成后接 3-1、3-1 通关解锁并接 3-2、3-2 结束返回选关，P3 支持 Esc 返回选关；3-1/3-2 地图加入打包清单。主理人确认两关手感及通关通过。收尾过程与核验范围见 [P3 收尾验收](UE5/Records/p3_formal/收尾验收_2026-10-28.md)。

**P3 音频与发版记录补齐。** 修复 BGM 播放组件重复淡入导致的停止/反复恢复，加入 BGM 恢复与片段交叉淡化；狂暴状态 BGM 提高 2 个半音强化紧迫感。P3 音频实施记录与好友增量更新/本地存档保护方案已归档；集成后试听和补丁存档冒烟验证仍待首次人工验收。

**P4 α W1 预览材质已修复，旧截图像素提取发现 X 尺基准位置需要纠正。** 主工程 UE 5.8.3 `Nai_LongEditor Win64 Development` 构建通过（63/63 actions）；CSV 经 `CSVImportFactory` 导入原生 DataTable，3 行 / 11 列读回一致，Importer 报 0 Problems。旧 Standalone 截图的 X 尺位于前沿 `Y=7`，当地跨度1549.95px，与当地目标1548.713px误差+0.08%；这不符合计划要求的中心深度基准。Y尺垂直跨度234.24px（目标234.459px，误差-0.09%），两姿势下沿同为第539行。预览脚本现将 X 尺移至中心 `Y=92.5`，新截图待重建；旧画面软影反差不足以确认接地效果。当前 Unreal 窗口正在跑 P3 3-2，未切图。详见[修复后截图](UE5/Records/p4_alpha/w1_runtime_fixed_1920x1080.png)、[G0/W1 启动记录](UE5/Records/p4_alpha/G0_W1_启动记录.md)和 `w1_runtime_visual_review.json`。W1 仍待中心线复采、帧时间、软影/动态排序和正背侧动作样片验收。

**P4 α W1 校准场准备推进。** 新增无 Pawn 的 P4 W1 预览 GameMode，并完成隔离 scratch 工程的 UE 5.8.3 增量构建（5/5 actions）。新增编辑器脚本，准备生成单独校准图、固定相机、1000 uu / 185 uu 投影尺、图集切格材质、脚底对齐立绘与径向透明阴影。P4 的当前范围和证据见 [G0/W1 启动记录](UE5/Records/p4_alpha/G0_W1_启动记录.md)。

**早期检查记录（后续已推进）。** 最初几次 Editor-Cmd 启动确实没有生成报告、日志或 UAsset；这是确认主工程 Editor Python 可用之前的历史失败。最新导入和预览构建结果见本节前一条；真实画面、实际像素标定、材质排序观感与帧时间仍待验证，W1 出口门未通过。

**确定性基线补充。** 新增 P4 版本化状态哈希（当前状态字段子集）并接入 ArenaSubsystem 查询接口，隔离 UE 5.8.3 增量构建通过 5/5 actions。哈希字段尚未覆盖后续战斗状态，不能作为 α 金标；仍处 W1。

**α 强化卡数据行推进。** `FNaiwaP4UpgradeRow` 已扩成 DataTable 生产字段形状；三行 CSV 使用设计 JSON 的正式卡 ID 和数值，导入探针核对 RowStruct、完整列集合及列值。隔离 UE 5.8.3 构建通过 4/4 actions；主工程 DataTable 导入及读回结果已在本日较新的补记中记录。

## 2026-10-27

**P3 开工，M1 手感原型通过、M2 系统落到运行时。** 迷宫对抗关走 M1–M5 五个里程碑，本轮到 M2。源码 `NaiwaP3*` 共 22 组、8,105 行，独立于 P1/P2，未接进主菜单。

**M1：网格与炸弹手感。** `UNaiwaP3GridSubsystem` 用格子坐标做唯一真源，0.1 s 一格 tick，不做物理。炸弹 2.5 s 引信、射程中心格加四向各一格、连锁延迟 0.2 s、火焰 0.3 s 消失，参数全部进 `DefaultGame.ini` 的 `NaiwaP3M1Settings`，四个手感数字（4 格/秒移动、0.4 s 放弹间隔、容量 1、射程 1）按试玩反馈定。自测 7/7 PASS，布局哈希 `6DCBCA85`。

**M2：种子地图 + 机关 + 敌人 + 迷雾。** 23×17 布局由 `FNaiwaP3M2SeededMapGenerator` 按主种子派生三条独立随机流（地图 / 掉落 / 脚本 AI），生成后 BFS 连通校验、最多重试 64 个种子，仍不通就确定性修路。四种弹型（标准十字 / 豆奶大十字 / 奶茶圆爆 / 奶鼠分裂）、五类机关（传送门、踩板金库门、传送带、尖刺、断裂地板）、四名敌人各有独立决策器：奶察走危险表 + 代价寻路 + 逃跑，奶杀无视炸弹莽冲，奶豪按剧本投弹，奶雨远程且开火前 0.8 s 亮五格预警。战争迷雾分未探索 / 已探索 / 可见三档，未探索格整体盖深色遮罩。

**美术换夜巡风格。** 9 张角色立绘 + 28 张场景 sprite + 8 张 tile 纹理，从 7 张 AI 图集里裁切加工，源图集留在 `Records/p3_m2/art_preview/`，是这些资产生成的唯一输入。地面、硬石、木箱三张按夜巡色板重做了 v2。奶茶炸弹去掉底部珍珠，只留纯色吸管和编织引线。

**四张地图。** `L_NaiwaP3_M1Prototype`（手感测试）、`L_NaiwaP3_M2ScenarioTest`（23×17 评审用，seed 320000）、`L_NaiwaP3_31`（11×9 教学，无迷雾）、`L_NaiwaP3_32`（23×17，有迷雾），四张都在图里写死 GameMode。

**没验的部分。** 正式关 3-1 / 3-2 按计划书应排在场景测试关通过评审之后，实际提前建了图。M2 的 GameMode 没有在引擎里跑过一次，8,105 行核心逻辑（占格冲突、连锁结算顺序、AI 真实 tick 表现）全部未暴露。M1 的 `6DCBCA85` 自测记录停在 10-26 23:21，之后源码改到 10-27 07:53，新 DLL 编于 08:07 但没有对应自测，记录不再代表当前代码。

## 2026-10-22

**主菜单落地，游戏改成从菜单进。** 新增 `Content/Naiwa/UI/L_NaiwaMainMenu` 与 `NaiwaMenuWidget` / `NaiwaMenuGameMode` / `NaiwaMenuPlayerController`：界面全部由 C++ 在 `UCanvasPanel` 里搭，没有 UMG 资产，所以改版式就是改代码。两页——标题页（开始 / 退出）与关卡选择页（四张卡片 + 通关计数）。进哪个图由 `DefaultEngine.ini` 的 `GameDefaultMap` 指向菜单，P1 与 P2 的暂停菜单都能退回主菜单。

**进度存档。** `UNaiwaProgressSubsystem` 挂在 `GameInstance` 上，通过 `UNaiwaMenuSaveGame` 存本机，记每个关卡的最优时间、最少发数、最少死亡、收集数。解锁顺序：1-1 与 2-1 直接进，2-2 需通关 2-1，2-3 需通关 2-2。这套进度同时承担连闯时的跨关卡状态延续。

**关卡缩略图与输入映射。** 四张卡片配图 `T_UI_Thumbnail_11/21/22/23` 由 `UE5/Scripts/create_level_thumbnails.py` 生成，卡片读不到图时回退成纯色块而不是崩。走跑切换从代码里的裸键位改成增强输入资产：`Content/Naiwa/Input/IMC_Naiwa` 与 `IA_NaiwaGait`，由 `configure_gait_input.py` 写入、`apply_menu_and_input_assets.py` 落到蓝图，`Records/p0_completion_20261013/gait_input_mapping.json` 存键位对照。抽了 `NaiwaHUD` 一份共用基类，P1 的控制器另拆成 `NaiwaP1PlayerController`。

**P2 音频接入，此前的「零音频」补齐。** 四个音效 `SFX_P2_Collection` / `GoalHit` / `Miss` / `FailureTrumpet`，三首关卡 BGM 分别对应 2-1 Title Theme、2-2 雨爱、2-3 熊出没，菜单曲为金风玉露。打包清单确认这五个资源（三首 BGM + 「嗯？」未命中音 + 「冰冰冰」命中音）都进了 pak。

**1-1 → 2-1 连闯接通。** `ANaiwaP1GameMode::AdvanceToP2Campaign` 在终点结算后 1.5 s 淡出 `OpenLevel` 到 `L_NaiwaP2_21`；`ANaiwaP2GameMode` 用 `-NaiwaLinkRun` 命令行标记判断是否走连闯，逐关接力 2-1 → 2-2 → 2-3。命令行加 `-NaiwaP1Standalone` 可只跑 1-1 不进 2-1。

## 2026-10-21

**P2 全物理改造收口，M1–M5 完成。** 结构块、弹丸、目标蛋全部换成 Chaos 真刚体（`ANaiwaP2StructureBlock` / `ANaiwaP2PhysicsEgg` / `ANaiwaP2PhysicsGoal`），初速只给一次，不设角速度、不做反弹预测，摩擦与恢复走 `UPhysicalMaterial`。三关单发 `pass=1 / penetration_events=0`，四种特殊蛋能力探针全通，奶鼠分裂出的三枚真实参与碰撞。静置 5 s 漂移 0.772 / 0.471 / 0.609 cm（限 1.0），1080p 可见窗口 p95 5.750 / 5.751 / 5.709 ms（阈值 16.7）。

**子步配置写进 `DefaultEngine.ini`。** `bSubstepping=True`、`MaxSubstepDeltaTime=0.008333`、`MaxSubsteps=8`、`bTickPhysicsAsync=False`。最细的柱是 0.30 × 1.90 m（高宽比 6.3），1/60 步长下单步位移会让接触点在两个子步之间跳过支撑面。这几个数和 `NaiwaP2PhysicsProfile.h` 里的常量必须一致，`VerifyProjectPhysicsSettings()` 运行时比对，不一致直接打 Error。

**放弃确定性仿真。** 原计划里的 9 条标准链、1209 格扫描、窗宽查表全部作废——它们预设了「旋转由参数给定」，而实际要的是碰撞自己转。代价是同一发重复打无法复现，只能靠 `ProbeTraj` 空场景做唯一硬验。

**打包与回归。** Development 包 839 MB、pak 395 MB，三关地图均加载成功并各自选中对应 BGM。多发回归序列里原默认 34° 在当前参数下第一发飞出场外，已把 `run_p2_physics_level.ps1` 的默认序列改成 19.85°。Cook 报告 `EggCandidates/LowPoly1798` 的候选网格引用了不存在的候选 Skeleton；该路径不在运行时使用的 `Art/Egg/` 下，打包与启动均通过，按落选方案处理，不入库。

## 2026-10-19

**P1 全部成果入库，分三个语义化提交推送。** 共 666 文件 / 138.79 MB，`origin/main` 推至 `09b2af1`：`chore(repo)` 补纳管规则与打包脚本 2 文件、`feat(p1)` 玩法与机关 75 文件 / 1.04 MB（19915 行）、`feat(p1)` 关卡资产与证据 589 文件 / 137.75 MB（其中走 Git LFS 226 个对象 / 100 MB）。LFS 累计 534 文件 / 463.86 MB，占免费月额度 10 GiB 的 4.5%。

**纳管规则补全。** 排除 `/tmp/` 与根目录 `已处理音频/`（后者 15 个 wav 与 `SourceAssets` 两处逐文件同哈希，无唯一内容）；排除 42 份 `*_build_*.log` / `*_compile_*.log`（37.19 MB，重编即再生）。放行 96 帧死亡动画成品与 3 张参照图，原始解码帧仍作中间输入不入库。`SFML_Level1/wav` 经逐文件哈希比对确认其中 `Background_Music_3/4`、`Hurdle2_1/2` 在仓库内无第二副本，改为整体入库。

**验收证据链修复。** P1 的 205 份验收报告 json 与 6 张 CP08 截图原先只存在于被忽略的 `Saved/NaiwaP1/`，而 `P1_TASK_STATE.json` 引用的正是这批文件——仓库里只有终端日志，证据链是断的。现已复制到 [p1_acceptance_20261018](UE5/Records/p1_acceptance_20261018)，哈希抽查 14/14 一致。同时把 `.workbuddy/` 下的 v7 关卡结构图移入 `UE5/Records/关卡结构图_v7_20261018.html`，避免随工作区丢失。

**新增可重建的打包入口。** 此前 828 MB / 630 MB 两个包只留下 12 份日志，没有脚本，按「删掉能不能一条命令重新得到」的判据不成立。新增 `UE5/package_demo.ps1`，参数按 10-18 实际执行的 BuildCookRun 命令还原，含 `-NoUBA` 回退与 `bUseIoStore=False` 的 classic pak 分支。

**第二关（奶蛋闯关）工程计划书出 v3。** 三关结构 2-1 斩首 / 2-2 选蛋 / 2-3 综合，延续 1-1 终点淡出淡入接过去的连闯形式（`OpenLevel` + 0.3–0.5 s 转场，不做同图无缝），死亡次数与已解锁蛋种搬进 `UGameInstance` 跨段延续。难度由 Python 仿真实算（1209 组合逐点模拟）而非手填。工作量 **M0–M2 34–38 人天（中位 36）+ M3 交付 3–5**；停止条件是 **M1 切片 6–8 天**，引擎复现不出仿真数字就地停。2-2 定为纯体验关，不卡难度，失败用 R 重开兜底。评估见 [P2 工程计划书评估](UE5/Records/P2_工程计划书评估_20261019.md)。

## 2026-10-18

**P1 收官，判定完成。** 任务状态登记 `p1_complete=true`、`remaining_candidate_acceptance=[]`：正式关卡 `L_NaiwaP1_v7` 上混合步态整关 PASS（自动路线墙钟 116.52 s），8/8 检查点落稳、11/11 断口落在预期静态平台、4/4 拾取、两组终点复合机关完成、实体终点 `Won`，死亡 0、角色探针瞬移与直接触发注入 0。CP01–CP07 生产地图逐点自然死亡恢复 7/7 PASS，每轮均安全岸原地复活、控制恢复、重取必需拾取后到达终点。状态矩阵 8 项 PASS。用户已实操通关并确认音效听感可接受，「真人通关时长」与「空白时间占比」两项由用户主动豁免。两处留尾：生成器字节幂等未解、最终镜头无独立复核报告。详见 [P1 工程报告](UE5/Records/P1_工程报告_20261018.md)。

**P2 打包出包。** Development 828 MB、Shipping 630 MB（pak 9639 项），Dev 包运行时烟测通过：96 帧动画全预载、11 个音效全部命中、零 `Failed to find object`。Shipping 带双击启动批处理。手柄未测、跨设备性能未采。需注意这两个包早于最后的 HUD 亮度修正与 CP01–CP07 验收代码，最终验证用的是编辑器目标，不能直接当作「含全部最新源码的最终可玩包」签收。

**P1-6 反馈与音效实现并完成运行事件核验。** 背景音乐 1/2 按时长循环，背景音乐 1 为原速 1.3 倍；死亡先播 Die，再播 Die_1 和 Bilibili 捧腹片段；本次闯关第 3 次死亡后段改播完整 Die_3，彩蛋期间暂停背景音乐，结束后恢复。拾取物为无贴图网格；第 2/3 个分别播放 Collection_1/2，期间背景音乐降至原来的 30%，拾取音渐隐完成后恢复。CP01–04 播早段语音，CP05–08 播后段语音并生成 Niagara 粒子，普通跳跃和弹跳均播放跳跃音效。捧腹动画使用用户选定的 Bilibili 视频 2–6 秒片段，制成 96 帧、4 秒循环，只处理代表片段；1×1 UV 灰块错误已修复。后续检查发现旧 GIF 源文件虽已替换，但旧导入器跳过同名已有纹理，前 16 帧仍残留旧素材；现已强制覆盖全部 96 帧、删除旧 GIF 源文件、停止旧角色笑动画和静态兜底图显示，并降低死亡界面暗色遮罩。C++ 编译、资源导入、音量事件与死亡复活回归均通过。普通窗口视觉和真实扬声器听感仍待复验。详见 [P1 v7 执行记录](UE5/Records/P1_V7_执行记录_20261017.md) 与 [P1任务状态](UE5/Records/P1_TASK_STATE.json)。

## 2026-10-17

**P1-5b 整合关卡首段物理路线及检查点失败恢复通过。** 新地图 `/Game/Naiwa/P1/Maps/L_NaiwaP1_Integrated` 目前有危险提示与隐藏尖刺、双侧开关平台、弹跳板、静态检查点等 12 个受管 Actor；全新游戏进程可加载。Walk150、Run300 均用真实自动运动完成尖刺跳越、承载乘台、弹跳落高台及检查点触发；两种步态还分别从检查点后的高台自然坠入 KillZ，恰好一次死亡后落稳在检查点，机关状态复位。初版 Run300 测出提示条意外形成 6cm 碰撞台阶，已将其碰撞配置持久关闭并在运行时严格核验。首建和重复构建保持 P0、旧 P1 地图及蓝图哈希不变、受管 Actor 集合稳定，目标地图二进制哈希随重复保存变化；变换归一后全部受管属性与配置一致。视觉构图及后续限时、组合、拾取、出口仍待完成，本段不得作为四机关整关通过证据。

**P1-5a 限时平台独立原型完成核心与失败重试验证。** `/Game/Naiwa/P1/Maps/L_NaiwaP1_Timed` 的 Walk150、Run300 实际过桥、首次登台倒计时、站立不续时、暂停冻结、失效下落、占用时延迟恢复、R/死亡复位均通过；Walk150 真实触发到右岸约 3.367 秒，有效期 3.8 秒的实际余量为 0.433 秒。初版坑底救援阶梯及替代斜坡经静态碰撞复核发现入口阻塞，已从隔离图移除，改为自然坠入 KillZ=-1000 后自动复活；两种步态均实测一次死亡、静态左岸复活、机关 Ready 与真实移动再次过桥。失效重试探针全程没有直接传送角色。最终重建只含两岸与限时平台等 7 个归属 Actor，P0、Slice、旧 Full、Bounce、Switch 哈希不变。普通窗口完整路线与整图联调仍待验证；详见 [P1任务状态](UE5/Records/P1_TASK_STATE.json) 和 `Saved/NaiwaP1/timed_*probe_*.json`。

**移动平台原型核心状态与失败重试通过。** Walk150、Run300 在隔离图完成双侧召回、载人往返、运动中不反向、离开重进才重触发、停靠与发车提示、暂停及死亡/R 复位；最终 Run300 截图可辨认停靠方向与发车倒计时。移除未经验证的坑底楼梯后，两个步态均实测从空缺口自然落入 KillZ=-1000、恰好一次死亡、左岸复活及真实移动再次乘台到右岸，失败探针没有直接传送角色。重复保存时目标地图二进制哈希仍变化，但归属 Actor 集合稳定、受保护资产哈希不变，留待 P1-5c 核查。弹跳板末次 HighResShot 与墙钟限时探针同进程运行，85 秒内游戏仅前进 7 帧而发生伪超时；拆开截图后的 Walk150 独立重跑 PASS，先前 Run300 PASS 保留。

## 2026-10-16

**P1 开工门槛清空。** 状态记录中阻塞项与 P1-5a 前置门槛均为空：P1-0 基线归档 548 项、SFML 正式源素材 29 项逐个 SHA256 校验、全新进程打开 P0 通过；P1 独立地图幂等重建未改变 P0 哈希；短路线 Run300 与 Walk150 物理路线通过；死亡/复活/拾取/终点状态流与扩展状态矩阵（14 事件）通过，原生编译通过；P0 共用角色与动画代码的 60fps 离屏回归 24/24。机关原型按用户授权启动，顺序为弹跳板 → 移动平台 → 限时平台，**一次只推进一个机关**。

**P1-5a 第一项：弹跳板 Walk/Run 物理与重置验证通过。** 独立地图 `/Game/Naiwa/P1/Maps/L_NaiwaP1_Bounce` 在 Walk150、Run300 下均完成安全落台、离开后回踩、R 重置、第三次触发、危险区死亡及死亡复位；首个安全落点分别约为 X=186cm 与 X=214cm，发射速度为 1000cm/s。根据实测把安全台调到起跳台上方 60cm、将弹跳板半宽增至 160cm，并将危险区对齐安全台；最终两种步态各触发三次且只死亡一次。P0 共用代码回归 24/24（截图齐全），Slice 九事件状态烟测通过；P0 受保护资源及既有 Slice/Full 地图哈希复核未变。DX11 游戏截图确认角色和天空大气可见、场景可渲染；出生点截图未覆盖完整路线，完整机关构图留待整合地图最终复核。此前编辑器 Quick Asset 搜索曾触发 RHI/D3D12 访问冲突；本次 DX11 运行及探针无崩溃，崩溃报告未提交。详见 [P1任务状态](UE5/Records/P1_TASK_STATE.json)、`bounce_probe_*.json` 与 Saved 下截图。

**P1-5a 第二项已开始：双侧触碰开关与移动平台原型。** 按已定规则先在隔离地图实现平台召回、停靠、登台提示后发车、反向开关召回、载人移动、暂停与死亡/R 复位；运动中不反向、触发器需离开后重进才可再次触发。本原型通过后再进入限时平台，不并行实施。

**两项验收由用户主动延期复验**：基础镜头候选（`1000cm / Z100cm / FOV65`，横向跟随速度 4→8，用户反馈角色偏右）；真实键盘 Walk 的死亡 → 复活 → 再拾取 → 终点闭环。

**P1 计划书评审出裁决。** 10-15 评审判 CONCERNS，次日逐条裁定 D1–D7、M1–M30、N1–N3：驳回「限时平台降级为可砍项」；保留轻松夸张的风格目标，但不设「观察者必须发笑」一类主观门槛；采纳阻断定义 B1–B7、P0 哈希外置重算、机关状态规格、视频不作计时依据、P1-5T 独立工时等。**多机关版尚未实施**，`L_NaiwaP1_Full` 仍为旧结构灰盒基线（11 地形 / 9 尖刺），不作为新关验收证据。

**本批 P1 改动尚未提交**（约 91 文件 / 37 MB：新增 P1 运行时 C++ 模块、关卡与材质资产、脚本与记录）。

## 2026-10-15

**P1 计划书评审。** 对 P1 计划书完成工程与验收两个维度的独立评审，判定 CONCERNS：验收设计完整度高（P0 复盘 11 条、测试分层、检查点×拾取物可达性、P0/P1 共用代码边界、同帧事件幂等），但全文缺少面向「好玩」的目标判据，音效仅在「复用已有 Laugh 作死亡反馈」出现一处。修订建议汇总为 7 条待拍板项 + 30 条逐条建议。

**旧结构全关自动跑图恢复通过。** `L_NaiwaP1_Full` 的 Run300 与 Walk150 自动路线均通过（此前失败卡在 x≈1582 的跳跃阈值与陷阱位置不匹配）；此前真实按键驱动 43 秒通关，反证卡点是路线脚本而非关卡设计。该地图仍只作旧结构灰盒基线。

## 2026-10-14

**仓库首次提交。** 建立版本纳管规则：`.gitignore` 重写为 8 节「全局排除 + 定点放回」，新增 `.gitattributes` 与 Git LFS 规则；随后分三次提交纳入 UE5 P0 工程与验收记录、奶蛙 Blender 建模流水线与动画成片，共 1105 文件 / 398 MB。引擎缓存、可一条命令重建的过程数据（> 1 MB）、历史版本 blend 与全部 FBX 均不入库；根目录 SFML 原型源文件保持原样不动。

**文档分工确立。** `README.md` 只写项目介绍与当前进展，迭代流水与阶段性结论移入本文件。

## 2026-10-13

**UE5 P0 已完成。** 资产接入、灰盒控制、五动作连续播放、有限动态视觉、真实键盘/焦点及最终 24/24 回归通过。按用户明确需求保留 A/D 或左右键移动，Shift 切换行走 150 / 奔跑 300 并选用原 Walk/Run，普通窗口实际采样通过。见 [P0最终验收](UE5/Records/p0_completion_20261013/completion_report.md)。正式角色基线仍为 E2；下一阶段为 P1 闯关流程，打包与进一步性能适配属于 P2。

## 2026-10-09

**C3 联合求解修订**已生成 A/B，B 的轮廓及中央剖面有改善、资产/动作和独立复建通过，但局部侧颈转向集中增强，最终不采用。按指令停止，不追加第三候选，正式推荐保留 E2。见 [联合求解本轮结果](NaiwaCharacter/versions/20261009_E2_C3_solver_revision/README_本轮结果.md) / [图文总览](NaiwaCharacter/versions/20261009_E2_C3_solver_revision/index.html)；本次启动及审批响应流中断、恢复与反思已记入该目录 `执行记录.md`。

**E2→C3 第一候选 A** 已生成并完成关键对照，但形体改善不明确、固定参考前颈误差略增，按指令停止且不采用。正式推荐仍为 E2。见 [C3 本轮结果](NaiwaCharacter/versions/20261009_E2_C3_reference_refinement/README_本轮结果.md)。

## 2026-10-08

最新推荐为 **E2**：`NaiwaCharacter/versions/20261007_H2_E2_overall_refinement/candidate_E2/Naiwa_Character.blend`，FBX 同目录。先完成 H2 头颈肩整体连续形体，再独立修正眼孔与瞳向；实际曲面、完整多视图/动作、FBX 重导入和独立复建均通过。H2 保留为形体阶段，R1 及历史版本保留。固定参考前颈残差、远眼露出差距及强掠射眼窝/浅纹已明确记录。见 [H2/E2交付说明](NaiwaCharacter/versions/20261007_H2_E2_overall_refinement/README_交付说明.md) / [三版本图文总览](NaiwaCharacter/versions/20261007_H2_E2_overall_refinement/index.html)。

**上一轮圆润精修 R1** 位于 `NaiwaCharacter/versions/20261007_N2_round_refinement`，原生工程、FBX 及完整验收继续保留。见 [R1交付说明](NaiwaCharacter/versions/20261007_N2_round_refinement/README_交付说明.md)。

**下脸—颈部 N1/N2** 位于 `NaiwaCharacter/versions/20261006_head_neck_refinement`，原生工程与 FBX 继续保留。说明见 [第一阶段交付说明](NaiwaCharacter/versions/20261006_head_neck_refinement/README_交付说明.md)。

## 2026-10-06

**球面眼迭代**：`NaiwaCharacter/versions/20261006_spherical_eyes`，包含 A/B 两个独立球面眼体工程和 FBX。本轮只重建左右眼体并重新贴合高光；黄色主体、嘴、头部截面、骨架及站立双臂垂放姿态保持。基础候选均保留正面注视。

交付总览为该目录的 `index.html`、`README_交付说明.md`，包含三列彩色/灰模对比、实际参数及独立验收记录。建议以 A 继续打磨，B 保留为较高露出的构造对照；主体参考残差只记录，未追加头颈修改。固定输入为 `NaiwaCharacter/versions/20261005_head_side_continuity/Naiwa_Character.blend`，原始基准与各历史工程保留。

默认 `build_naiwa.py` 已指向 `revise_spherical_eyes.py`，每次从固定基准分别生成 A/B，避免累积位移。默认 `validate_head_revision.py` 调用 `validate_spherical_eyes.py`。进度检查点见 `NaiwaCharacter/TASK_CHECKPOINT.md`；此前连接故障记录见 `NaiwaCharacter/connection_error_report.json`。

缓存、重复草稿和重建临时副本的清理记录位于 `NaiwaCharacter/cleanup_report.json`；2026-10-06 的补充清理记录见 `NaiwaCharacter/cleanup_report_20261006.json`。原生历史工程、当前交付及头颈修整资料保留；部分历史 FBX 可用 `NaiwaCharacter/export_saved_fbx.py` 从对应 `.blend` 重新导出，说明见角色目录 README。

---

## SFML 原型（已定型，不再迭代）

根目录为 C++20 / SFML 横版游戏原型，第一关定型，含音效与陷阱系统。该版本为项目早期的练手原型，代码与内容保持原样冻结，后续开发重心在 UE5 主线。
