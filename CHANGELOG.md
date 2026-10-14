# 开发记录

按时间倒序记录各产线阶段性结论。仓库主页说明见 [README.md](README.md)。

---

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
