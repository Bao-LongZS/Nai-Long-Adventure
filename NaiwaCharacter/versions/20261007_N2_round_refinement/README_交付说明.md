# 奶蛙 N2 圆润精修 R1 · 2026-10-07

推荐工程：`candidate_R1/Naiwa_Character.blend`；推荐导出：`candidate_R1/Naiwa_Character.fbx`。
图文总览为 `index.html`；快速看 `comparisons/comparison_head.png` 和 `comparisons/comparison_lowerface_detail.png`。

本版从固定 N2 重新载入生成，保留原 N2 工程及 FBX。处理嘴下至喉部的圆顺、侧颈和后肩浅纹、后脑至后颈收腰。五官网格及眼体安装、冠顶、肩根、骨架、完整动画、拓扑、UV、颜色和有效权重独立核对保持；全部最终顶点 Z 与 N2 的 float32 位模式一致。没有实施 C1。

实际中线曲率峰 33.77219 → 26.78389，降低 20.69%；相邻窗口一起检查，左右颊及 Raw/Idle 的独立结果见 `diagnostics/surface_diagnostics.md`。主体最大总位移 0.00772086，最大 X 位移 0.00256816。低颈、后侧法向及切向测量、三层端点、五官接触和 Walk/Laugh/Jump 肩根检查见 `round_validation.json`。

方法是冻结真实 N2 三角面、源法线与保护区，对最终周期截面 XY 进行联合弯曲光顺，再对真实原生顶点的口周剖面曲率作联合约束。嘴缝距离≤0.012完全冻结，0.012～0.040使用非对称连续释放；保护权重在优化的基函数及曲率 Jacobian 内生效。所有投影保持源 Z。有限迭代的全局求解残差如实保存在 `build_report.json`，不以 C2 或收敛标签代替最终曲面验收。

曲率调整把原窄峰摊成较宽的平台，部分位置的曲率相对同Z源值升高；完整邻峰、保护边界及横向数据全部记录。低粗糙度反射仍可显出既有细小锯齿和浅纹，未把“无任何纹路”作为交付结论。

彩色/灰模、真实低粗糙度反射条纹、两方位掠射及全身图均固定相机、光照和曝光。四区域视觉结论与保留的自然口周细节见 `visual_acceptance.json`。固定参考沿用原50°/−2.5°机位及注册系数，逐行误差约束见 `reference_comparison/reference_review.json`。

FBX已独立重导入，逐网格真实顶点和三角面表面、权重及动作均核对；见 `candidate_R1/fbx_validation.json`。独立复建与原生交付的所有坐标/元数据/动画逐项相符，见 `candidate_R1/rebuild_validation.json`。

## 从固定 N2 复建

在项目根目录运行已交付脚本；`candidate_R1/parameters.json` 记录最终参数。命令见 `rebuild_command.ps1`，输出到独立临时目录，避免覆盖推荐工程。源码闭包封存在 `source_scripts.zip`，归档内沿用项目的 `NaiwaCharacter/` 结构；必要固定输入 N2 保留在原版本目录。

复现环境为 Blender 5.2.2、Python/NumPy/Pillow。图文脚本使用 `C:/Windows/Fonts/msyh.ttc`；渲染参考依赖保留的球面眼相机协议、旧N2参考误差表及 `references/20261005/pdf_page03_image03_IM60.jpg`。这些是共同基准，未重新注册参考图。

## 文件索引

- `section_raw.json` / `raw_mesh_measurements.npz`：独立保存工程的 Raw/Idle 真实三角剖面、坐标、源法线。
- `diagnostics/`：曲率 CSV/JSON、纵横剖面、侧颈频段辅助诊断、位移与保护热图。
- `comparisons/`：N2/R1 原图及固定条件拼版。
- `selection.json`：推荐路径、源及交付 SHA；`file_manifest.json`：逐文件校验清单。

固定 N2 SHA256：`9397186f3ae13e93e05cc0d492c12ceb68b743ab4d2a9a0d650426a47a66b0cb`。
推荐 R1 SHA256：`ea12e9af2c564527d0c473cd31b28974339fcceb15b666502a33f0ffa057bc24`。
