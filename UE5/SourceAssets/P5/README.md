# P5 —— 第 4 关滑雪大冒险源美术

2026-10-28 项目所有者通过 A0 v02 自然色方向和首稿奶蛙造型，授权继续推进。A0 仍是概念合成，并未通过真实 UE Editor 的路线、碰撞、720p 与性能验收；详见 `Manifests/P5_A0_ArtSample_manifest_v02.json` 和 `Records/p5_p6_plan/evidence/P5_A0/P5_A0_CompositionReview_v02.md`。

当前已有 A1 背景、A2 雪面、A3 障碍、A4 奶蛙六姿态、A6 特效和 A7 色板的规格候选导出。除项目所有者确认的 A4 **造型原稿**外，这些导出均为 `_WIP`；A4 atlas 分格和像素隔离已复核，但锚点、真实关卡可读性、材质过滤及 Editor 集成仍待验收。A1 近景雪脊 `BG_Ridge_Near_v03_WIP.png` 的首尾 RGBA 边缘列已精确匹配，双拼接缝切线/纹理与 UE 过滤仍待验。项目所有者已于 2026-10-28 通过 A5 袋鼠六姿态稿作为制作基底；其原稿已锁存，清理后的 1024×1024 图集位于 `Environment/Exports/C_Kangaroo_Ride_v04_WIP.png`。袋鼠是关卡内坐骑道具，不是玩家控制角色；图集仍需锚点、UE 过滤和分辨率检查。

原交接范围见 `UE5/Records/p5_p6_plan/P5_美术交接记录_2026-10-28.md` 与 `Manifests/P5_ApprovedAssetIndex_v01.json`；当前续作状态和复核见 `UE5/Records/p5_p6_plan/P5_美术资产执行记录_2026-10-28.md`、`UE5/Records/p5_p6_plan/evidence/P5_A1_Backgrounds/P5_A1_Backgrounds_Review_v05.md` 与 `UE5/Records/p5_p6_plan/evidence/P5_A5_KangarooMount/P5_A5_Review_v02.md`。当前选定 19 张 PNG/40 个计数单位及 SHA256 见 `UE5/Records/p5_p6_plan/P5_美术交付清单_v01.md`；工程接入步骤见 `UE5/Records/p5_p6_plan/P5_美术接入工程交接_v01.md`。A1 manifest 为 `Manifests/P5_A1_Backgrounds_manifest_v05.json`；A5 当前帧表与导出指纹见 `Manifests/P5_A5_KangarooMount_manifest_v02.json`。导出实现位于 `UE5/Scripts/p5_export_a1_background_candidates.py`、`p5_export_a1_ridge_seamless_candidate.ps1`、`p5_export_a2_snow_candidates.py`、`p5_export_a3_obstacle_candidates.py`、`p5_export_a4_character_candidate.py`、`p5_export_a5_kangaroo_candidate.ps1`、`p5_export_a6_fx_candidates.py` 与 `p5_build_a7_palette.py`。

P5 专属。源稿按 `Backgrounds/`、`Characters/`、`Environment/`、`FX/`、`Materials/`、`Manifests/` 和 `References/` 组织。全量 B 的 19 PNG/40 帧基线及验收见 `UE5/Records/p5_p6_plan/P5_美术执行计划_v1.md` 与 `P5_质感方案与资产规格_v2.md`。`References/` 内图片是方向图，不计入生产资产/运行时包；任何 `_WIP` 导出均未标为运行时验收通过。
