# P6 A1 v04 UE 集成与审阅交接

日期：2026-11-12  
状态：**UE 5.8.3 单网格导入成功，Nanite 已构建；已按当前 M2 固定机位建立独立审阅图 v06 并采集 720p/1080p v11 画面；首轮检查发现明显屋顶遮挡与灰盒光色差距，14 点可见性、交互面与最终美术验收仍待完成。**

## 当前 UE 资产

- 原始源稿 `Models/P6_A1_CourtyardKit_Blockout_v04.obj / .mtl` 保持不变，仍含 1,523 个可编辑对象组、54,944 顶点和 49,534 面。
- UE 专用合并源 `Models/P6_A1_CourtyardKit_v04_UECombined.obj / .mtl` 将全部面按 40 个实际使用材质连续分批，并合并为一个对象。为抵消 UE OBJ 导入时的右手系 Y 翻转，生成器仅在此 UE 专用副本中预翻转 Y；其最终 UE 坐标与原始 v04/M2 坐标一致。它还新增每顶点一个 XY 平面预览 UV，供 MTL 平色和导入预览使用；UV 不代表最终展 UV。
- 导入脚本将单个 Static Mesh 保存至 `/Game/Naiwa/P6/Art/A1/CourtyardKit_v04/P6_A1_CourtyardKit_v04_UECombined`，40 材质槽、40 个 section。LOD0 Nanite fallback 为 18,119 顶点 / 27,520 三角形；Nanite 数据为 54,944 顶点 / 91,460 三角形。UE bounds 与源稿一致：X −3,550 至 3,550 cm、Y −2,898.518 至 2,750 cm、Z −110 至 1,875 cm。最终导入报告：`UE5/Records/p6_art/evidence/P6_A1_CourtyardKit_UEImport_v04.json`。
- UE 导入日志最终状态 0 errors / 1 warning。剩余警告来自项目既有 PIE 静态对象约束，不是 A1 导入；初次无 UV 导入日志保留为 `..._no_uv.*`，证明后续 UV 修复前的失败原因。

## 独立灯光审阅地图

- 当前审阅图 `/Game/Naiwa/P6/Art/A1/Maps/L_NaiwaP6_A1M2LightingReview_v06` 单独引用 A1 网格；M2 主地图保持未修改。它从 `P6_A1_CurrentPointArtMap_v01.json` 读取固定相机参数。
- v06 相机为 M2 当前值：位置 `[0, 2078, 3600] cm`，`Pitch -60° / Yaw -90° / Roll 0°`，正交宽度 `7800 cm`。地图报告确认相机与点位表一致。冷色主光 7.2 klux、冷补光 1.4 klux、天空强度 0.45、四盏暖色点光（每盏 16,000 cd、半径 950 cm），手动曝光偏置 −0.15 EV。它是光色候选，不代表已通过风格验收。
- 地图报告：`UE5/Records/p6_art/evidence/P6_A1_M2LightingReviewMap_v06.json`；构建脚本：`UE5/Scripts/build_p6_a1_m2_lighting_review_map_v06.py`。
- UE 5.8.3 D3D12 实际画面：`P6_A1_M2LightingReview_1920x1080_v11.png` 与 `P6_A1_M2LightingReview_1280x720_v11.png`；采图记录：`P6_A1_M2LightingReviewCapture_v11.json`；采图脚本：`UE5/Scripts/capture_p6_a1_m2_lighting_review_v06.py`。两张图按 M2 相机采集，仍是灰盒；冷夜环境和暖灯层次未接近已通过的 A0 夜景概念。
- 画面中前景屋顶占比很大。v04 离线深度代理只计 4/14 个 pivot 可见；临时隐藏屋顶/檐口后为 5/14，说明单独拆顶不能解决全部遮挡。代理指出的 7 个受遮挡主线/必看点为 Trophy（戏台瓦片）、Noticeboard（树冠）、Detector（房屋右屋面）、PhotoTable / Flowers（房屋左屋瓦）、PatrolLedger（桌体自遮挡）、NaiPai（房屋右屋瓦）。这是几何投影代理，不是 UE 轮廓、碰撞或点击验收。
- 带 14 个位置标记、两种深度代理状态和 A0 风格缩略对照的独立审阅板：`UE5/Records/p6_art/evidence/P6_A1_M2VisibilityReview_v01.svg`。板上标记只用于编辑器审阅，不是运行时 HUD 或实际 Actor bounds。
- 旧 v05 灯图与 v10 截图保留为较平视的机位试拍（`Pitch -25°`、宽度 `10500 cm`），不能代表 M2 固定构图；v06/v11 是当前复核入口。

## 尚未验收的事项

审阅图使用 UE 命令行和 `-NullRHI` 构建；截图来自完整 Editor 的 D3D12 渲染。首轮 M2 机位检查确认相机参数一致，但画面仍是灰盒，屋面压住多个点位，光色也没有形成 A0 概念中的暖灯/冷夜对比。

后续在 M2 固定相机下按 14 点逐项核对真实可见轮廓、可点区域、billboard 脚底、行走路线和遮挡；隔离审阅图只证明当前独立场景可渲染与取景，不代表 M2 运行时位置和点击验收。

当前没有最终 UV 展开、项目碰撞体或人工 LOD 链；当前引擎数据为一个 Nanite mesh 与一个 Nanite fallback LOD。正式 Level Design 摆位仍以 `Manifests/P6_A1_CurrentPointArtMap_v01.json` 为准，不用此隔离关卡覆盖 M2。

## 可重跑入口

1. `UE5/Scripts/build_p6_a1_ue_source_v04.mjs` 从原始 v04 OBJ 重建按材质批次排序的单对象 UE 导入源。
2. `UE5/Scripts/import_p6_a1_courtyard_v04.py` 执行严格源几何/UV/材质预检，同源复导并记录 UE LOD 与 Nanite 统计。
3. `UE5/Scripts/build_p6_a1_m2_lighting_review_map_v06.py` 创建当前 M2 相机隔离照明图；相机旋转按 UE Python `Rotator(roll, pitch, yaw)` 传参。
4. `UE5/Scripts/capture_p6_a1_m2_lighting_review_v06.py` 在完整 Editor 中采集固定分辨率画面。
5. `UE5/Scripts/inspect_p6_a1_static_mesh.py` 可只读复核网格 bounds、LOD、section 与 Nanite 数据。
