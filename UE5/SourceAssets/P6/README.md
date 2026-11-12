# P6 —— 第 5 关剧情破案源美术

美术侧 A0–A5 交接入口和优先集成顺序见 `UE5/Records/p6_art/P6_美术侧交接总览_2026-11-11.md`。
- `P6_ArtReviewPortfolio_v04.svg`：当前自包含 A0–A5 评审总览，纳入 A1 v04 灯光与木作细节更新，并标注 A0 场景概念和 A2 图像选型的通过边界；索引见 `Manifests/P6_ArtReviewPortfolio_Index_v04.json`。v01–v03 保留为历史版，各分项源文件/交接仍以本目录和对应 manifest 为准。

P6 案件专属。源稿按 `Characters/NPC/`、`Environment/`、`Evidence/`、`InfoLayers/`、`UI/`、`Materials/`、`Manifests/` 和 `References/` 组织。P6 14 个交互点、12 张证据卡、4 类信息层的任务与验收见 `UE5/Records/p5_p6_plan/P6_美术执行计划_v1.md`。通用卡面底板和合格皮肤中性立绘单独记入 `Common/`；案件内容不得混入 Common。

## A0 首轮样片

- Environment/P6_A0_Courtyard_Concept_v01.png：固定 3/4 斜视的院落氛围概念图，非 UE 地图。
- Environment/P6_A0_ReferenceBoard_v01.svg：首版场景与奶蛙姿态研究图版；SVG 外部引用预览不稳定，保留作历史样稿。
- Environment/P6_A0_ReferenceBoard_v02.png / .svg：可直接查看的 PNG 评审板和可编辑 SVG。场景图原稿保持不变，并加入图谱人物选型、场景地标和评审关注点。
- Environment/P6_A0_VisualDirection_v01.md：造型、构图、色板和 UI 视觉基线。
- Environment/P6_A0_VisualDirection_v02.md：本轮修订后的评审基线。
- UI/P6_ClueCard_Template_v01.svg：案件卡片底板候选。
- UI/P6_ClueCard_Template_v02.svg：本轮更新的可编辑卡面底板，固定 ID / 类别、插画、标题、摘要、时间、来源和记录类型层级。
- UI/P6_ClueBoard_Mockup_v01.svg：首版线索板布局样片。
- UI/P6_ClueBoard_Mockup_v02.png / .svg：整合 12 张真实卡面缩略图与三段空结论槽的可预览 / 可编辑样片；未显示已收集或答案正误状态。
- InfoLayers/P6_InfoLayer_Kit_v01.svg：线框首稿，已由 v02 替代。
- InfoLayers/P6_InfoLayer_Kit_v02.png / .svg：四类信息层叠加在院落实景上的可直接查看 PNG 和可编辑 SVG，含柔化视域、破损断面、地面擦痕与冲淡脚印。
- Evidence/P6_A3_ClueCard_ContactSheet_v01.png：G1–G6、S1–S4、X1–X2 共 12 张证据卡预览。
- Evidence/Cards_v01/：12 张单卡 PNG，尺寸 960×680；内容按 P6 工程计划整理。
- Evidence/P6_A3_ClueCards_ArtDirection_v01.md 与 Manifests/P6_A3_ClueCard_Index_v01.json：卡面规则、时间/来源对照和文件哈希。
- References/MilkFrogStudy/：从用户提供的《奶蛙学 必修第一册》原图版中原样提取的角色参考 PNG，文件名注明 PDF 页码。
- References/MilkFrogStudy/P6_REF_MF_PDF01_p110_AtlasSheet.png：奶蛙形态图谱 4 整页；`037_Curious` 与 `046_Neutral` 是从该页直接裁切的参考图，分别用于观察反应和中性站姿。
- Manifests/P6_A0_AssetIndex_v02.json：本轮资产 ID、来源、状态、尺寸与校验信息；v01 manifest 保留为历史记录。

## A1 当前 M2 点位美术交接图

- `Environment/P6_A1_CurrentPointArtMap_v01.png / .svg`：当前 14 个 M2 Actor 点位的正交相机投影与视觉资产需求表；已通过的院落概念图只作独立风格缩略图。
- `Manifests/P6_A1_CurrentPointArtMap_v01.json`：点位 ID、工程坐标和 bounds scale、参考投影坐标、资产方向及源码/风格图 hash。
- `Environment/P6_A1_CurrentPointArtMap_ArtDirection_v01.md`：解释数据来源、投影口径与“灰盒参考不等于最终摆位”的边界。

该图用于 A1 环境 kit 与可读性评审；坐标来自当前 M2，不替代最终 14 点场景摆位、遮挡检查和步行可达性试玩。

### A1 院落模块灰盒候选（v04 当前精细化版）

- `Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_Blockout_v01.obj / .mtl`：112 个可独立选择的对象组，1,640 个顶点、1,587 个面；以厘米计量，可作为 DCC/UE 灰盒源块。
- `Environment/A1_CourtyardKit/P6_A1_EnvironmentKit_Concept_v02.png`：12 项环境模块干净的不透明评审板。它是外观方向图，不是模型贴图；有透明边缘杂点的 v01 留作历史。
- `Environment/A1_CourtyardKit/P6_A1_ReviewBoard_v01.svg`：自包含总览，内嵌方向板、完整屋顶、剖切和 M2 公式投影。
- `Environment/A1_CourtyardKit/P6_A1_MaterialPalette_v01.svg`：自动读取 MTL 的 16 色预览样表；仅作平涂方向参考。
- `Environment/A1_CourtyardKit/P6_A1_CourtyardKit_Blockout_Preview_v01.png / .svg`：完整屋顶视图；`P6_A1_CourtyardKit_CutawayPreview_v01.png / .svg` 临时隐藏屋顶以检查院内道具。两套是深度排序的离线投影，不是 UE 实时截图。
- `Environment/A1_CourtyardKit/P6_A1_M2CameraPreview_v01.png / .svg`：按现有 M2 相机投影公式生成的 1920×1080 模型审阅图；14 个 Actor 锚点序号辅助核对，PNG 的淡色环只表示 pivot 深度代理值，SVG 的环不做深度测试。不是运行时可读性结果。
- `Environment/A1_CourtyardKit/P6_A1_M2CameraDepthProxy_v01.json` 与 `P6_A1_M2CameraCutawayDepthProxy_v01.json`：逐点列出投影中最近表面组；部分 pivot 落在自身道具几何上，须与真实 bounds/轮廓一起判读。
- `Environment/A1_CourtyardKit/P6_A1_M2OcclusionMatrix_v01.svg`：把两份 JSON 报告整理成可审阅的 14 点遮挡矩阵图。
- `Environment/A1_CourtyardKit/P6_A1_CourtyardKit_Handoff_v01.md`：模型单位、坐标、组内容、导入边界和下一步检查。
- `Manifests/P6_A1_CourtyardKit_Index_v01.json`：模块清单、几何计数、文件哈希与当前验收状态。
- `Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_Blockout_v02.obj / .mtl`：在 v01 原组和 M2 锚点上补门窗木作、石基、瓦列、院内铺地及重点道具结构；450 组、18,338 顶点、19,643 面、26 色。仍是可编辑灰盒源块，不含最终 UV、碰撞、LOD 或 UE 材质。
- `Environment/A1_CourtyardKit/P6_A1_ReviewBoard_v02.svg`：v02 细化灰盒、M2 镜头、临时剖切和 MTL 色样总览。可直接打开 PNG 查看两张主投影：`P6_A1_CourtyardKit_Blockout_Preview_v02.png` 与 `P6_A1_CourtyardKit_CutawayPreview_v02.png`。
- `Environment/A1_CourtyardKit/P6_A1_CourtyardKit_Handoff_v02.md` 与 `Manifests/P6_A1_CourtyardKit_Index_v02.json`：细化内容、来源、哈希和验收边界。v01 文件保留作原始粗灰盒历史版。
- `Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_Blockout_v03.obj / .mtl`：继续保留 v01 原件和 14 点 M2 锚点；以 825 片错缝倒角石板铺满院地，并补屋檐端饰、门环、东侧格窗/百叶、植物、公告栏纸页、检测仪刻度、井桶刻线、戏台帷幕/拼缝/钉头、房屋及戏台共四盏壁灯、相机桌布衬与镜头细节、石材低对比色阶。1,476 组、52,904 顶点、47,938 面、38 色。仍为可编辑源块，无最终 UV、贴图、碰撞或 LOD。
- `Environment/A1_CourtyardKit/P6_A1_CourtyardKit_Blockout_Preview_v03.png`、`P6_A1_CourtyardKit_CutawayPreview_v03.png`、`P6_A1_HouseDetailPreview_v03.png`、`P6_A1_StageDetailPreview_v03.png`，以及相机桌/公告栏/检测仪/井/奖杯五张 DetailPreview：全景、临时剖切、建筑与重点道具近景离线预览。
- `Environment/A1_CourtyardKit/P6_A1_M2CameraPreview_v03.png` 和 `P6_A1_M2CameraCutawayPreview_v03.png`：当前 14 点投影与剖切对照；离线 pivot 深度代理仍为 4/14、5/14，不代表 UE 可见性结论。
- `Environment/A1_CourtyardKit/P6_A1_ReviewBoard_v03.svg`：概念方向、全景、M2、剖切、房屋/戏台及相机桌/四个交互物近景和材质色样的自包含评审板。
- `Environment/A1_CourtyardKit/P6_A1_CourtyardKit_Handoff_v03.md` 与 `Manifests/P6_A1_CourtyardKit_Index_v03.json`：源组保留情况、改动、哈希和验收边界；v01/v02 均保留作历史版。
- `Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_Blockout_v04.obj / .mtl`：在 v03 上增加 5 组戏台前沿木作浮雕、44 道台板木纹、房门铰链/铆钉和两扇窗搭扣，并抬亮蓝灰瓦色与主木色；保留原 112 组与全部 14 个 M2 点位。1,523 组、54,944 顶点、49,534 面、41 色；厘米单位，坐标边界未变。
- `Environment/A1_CourtyardKit/P6_A1_CourtyardKit_Blockout_Preview_v04.png` 与 `P6_A1_HouseDetailPreview_v04.png`、`P6_A1_StageDetailPreview_v04.png`：显示中间调提亮、暖主光/冷天光补光、柔化地面方向投影和壁灯泛光的离线 PNG 预览；这些效果不等于 UE 的真实动态光影。
- `Environment/A1_CourtyardKit/P6_A1_CourtyardKit_CutawayPreview_v04.png`、`P6_A1_PhotoTableDetailPreview_v04.png`、公告栏/检测仪/井/奖杯近景：v04 同版剖切与道具图。
- `Environment/A1_CourtyardKit/P6_A1_M2CameraPreview_v04.png` 和 `P6_A1_M2CameraCutawayPreview_v04.png`：锚点表和镜头公式保持不变；离线 pivot 深度代理仍为 4/14、5/14，不代表 UE 可见性结论。
- `Environment/A1_CourtyardKit/P6_A1_ReviewBoard_v04.svg`、`P6_A1_CourtyardKit_Handoff_v04.md` 与 `Manifests/P6_A1_CourtyardKit_Index_v04.json`：当前验收展示、交接说明和资产哈希清单。v03 及更早版本继续保留。
- `Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_v04_UECombined.obj / .mtl`：为 UE 导入准备的单对象副本，原 v04 源不变；保留 54,944 顶点、49,534 面与 40 个已使用材质槽。配套 Editor 导入脚本与独立亮光审阅关卡脚本见 `Scripts/import_p6_a1_courtyard_v04.py`、`Scripts/build_p6_a1_lighting_review_map.py`；集成边界见 `Environment/A1_CourtyardKit/P6_A1_UE_Integration_v04.md`。当前还未生成 UE `.uasset` 或引擎截图。
- `Manifests/P6_A1_PointPropAlignment_v01.json`：14 个 M2 Actor ID 与道具组/图录 billboards 的候选对应关系；当前只确认灰盒锚点坐标对照。
- M2 相机投影预览中房屋/戏台屋顶占屏明显；按同一程序深度代理，完整屋顶视图有 4/14 个 Actor pivot 点通过，剖切屋顶后为 5/14。逐点报告显示部分未通过标记落在自身底座/桌面；较明确的空间冲突候选是公告栏与树冠、检测仪/相机桌/奶拍点与房屋、奶拍点与附近盆栽。此计数不能代表物件轮廓可见或 UE 遮挡验收，必须回 Editor 用真实 Actor/碰撞和 3/4 固定相机复核。
- 仅将 A1 环境方向板 v02 用作视觉参考；模型纹理、UV 和资产拆分仍待美术制作。灰盒房屋和戏台屋顶遮挡内场的风险已记入交接；最终摆位仍以 Level Design 评审为准。

## A4 单层信息源图候选

- `InfoLayers/A4_LocalCandidates/P6_A4_S1_Sightline_v01.svg`：视线扇区被墙沿截断。
- `InfoLayers/A4_LocalCandidates/P6_A4_S2_BrokenEdge_v01.svg`：局部破损边缘。
- `InfoLayers/A4_LocalCandidates/P6_A4_S3_ShortGroundTrace_v01.svg`：短地面擦痕和停留点。
- `InfoLayers/A4_LocalCandidates/P6_A4_S4_WaterAndFadingPrints_v01.svg`：水膜与渐淡三趾印。
- `InfoLayers/A4_LocalCandidates/P6_A4_ReviewBoard_v01.svg`：自包含四层单页总览；SVG 卡片内嵌每个独立透明源图。
- `InfoLayers/A4_LocalCandidates/P6_A4_LocalLayer_Handoff_v01.md` 与 `Manifests/P6_A4_LocalLayer_Index_v01.json`：透明源图尺寸、分层含义、建议排序与导入验收门。
- 四张 SVG 是独立可关的矢量源图，暂未导出贴图或接入 UE 材质；720p/1080p 遮挡、排序与开关态仍待工程内复核。

## A5 调查 UI 状态样片

- `UI/P6_A5_InvestigationHUD_v01.png / .svg`：调查 HUD 与附近可检查物件提示样片；控件键位沿用当前 M2 实现。
- `UI/P6_A5_InfoLayerTutorial_v01.png / .svg`：S1–S4 四层的首次调查教程，使用材质化小图例和局部描述。
- `UI/P6_A5_SubmitFeedback_v01.png / .svg`：结论未成立与结案归档两种提交后状态；不点名错误答案。
- `UI/P6_A5_ScreenStateBoard_v01.png / .svg`：三类界面评审总览；PNG 直接预览，SVG 可编辑。
- `Evidence/P6_A5_InvestigationUI_ArtDirection_v01.md` 与 `Manifests/P6_A5_UIScreen_Index_v01.json`：状态规则、集成边界、尺寸和哈希索引。

## A2 图录人物图像选型已通过（UE 集成待验收）

- `Characters/NPC/P6_A2_SuspectSourceUseBoard_v01.svg`：三名嫌疑人口供点的原图使用方案预览。
- `Characters/NPC/P6_A2_SuspectArtUseMap_v01.md` 与 `Manifests/P6_A2_SuspectArt_Index_v01.json`：奶蛙学附录图录编号与 NPC 点位的直接引用关系。
- 源图继续只保留在 `References/MilkFrogStudy/`：046「闲立微笑」作共用中性态，037「好奇询问」作反应候选；源图 PNG 与原始整页都不修改，也不复制到新目录。
- 负责人于 2026-11-11 通过 046/037 两张图像及三名口供 NPC 共用形象的选型。图录只展示同一奶蛙形象，尚未给三名角色提供独立身份造型；点位标签与口供负责区分人物。UE billboards 集成与 720p 遮挡/可读性验收仍待完成。

本组为目标视觉评审样片，不是 UE 运行截图。M2 当前 UI 仍是工程占位状态；结案全流程及四层教程待后续集成。底图复用已通过的院落场景，不修改其原图。

场景概念稿已由用户确认通过；评审板 PNG 与信息层 PNG 是方便打开的展示稿。负责人于 2026-11-11 通过直接使用《奶蛙学 必修第一册》附录图录原图的角色图像选型：046「闲立微笑」与 037「好奇询问」供三名口供 NPC 共用，原始整页与既有裁切保持不变。该审批只覆盖图像和共用形象选择；图录没有给三个 NPC 不同的角色身份造型，其 billboards 集成和 720p 可读/遮挡检查仍待完成。A3 线索卡为视觉样片，卡面文字按工程计划校对，未登记为运行时成品或答案状态展示。当前 P6 样片未登记为通过验收的运行时成品。通用卡面底板在确认有第二处实际引用并通过复用验收前留在 P6，不迁入 Common。
