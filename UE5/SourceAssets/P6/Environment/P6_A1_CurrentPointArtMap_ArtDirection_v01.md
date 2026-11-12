# P6 A1 当前 M2 点位美术交接图 v01

状态：基于 M2 工程定义制作的 14 点交接样片；不是 A1 最终场景摆位或 UE 美术验收。

## 交付物

- `P6_A1_CurrentPointArtMap_v01.png`：可直接查看的坐标投影图与资产需求表。
- `P6_A1_CurrentPointArtMap_v01.svg`：可编辑矢量版，右侧清单编号对应图中的点位编号。
- `Manifests/P6_A1_CurrentPointArtMap_v01.json`：14 个 Actor ID、世界坐标、Actor bounds scale、图上投影坐标、资产方向和来源 hash。

## 数据与相机

点位与 Bounds 原始值逐项取自 `FNaiwaP6CaseDefinition::GetM2Interactables()`。镜头位置和方向、正交宽度取自 `ANaiwaP6InvestigationGameMode::EnsureCamera()`；图中坐标投影以 1920×1080 参考视口计算。Actor Bounds 是交互/碰撞范围输入，不是美术 Mesh 的长宽高或碰撞验收结果。

当前 M2 结构为 6 个通用证据点、3 个人物口供点、5 个可跳过笑点。口供点要求 2D billboard，但具体人物造型与中性姿态属于后续 P6-A2 资产任务。A1 仍需逐点复核实际相机可见性、遮挡与步行可达性。

## 风格边界

已通过的院落概念图只作为图版中的独立风格缩略图；它不是 M2 的坐标底图。点位散布图呈现现有工程锚点，不要求最终美术场景复制当前灰盒位置。9 个必看点与 5 个可跳过点依当前工程定义分组；笑点不承载唯一答案证据。

内容依据：`UE5/Records/p5_p6_plan/P6_工程计划书_v2.md`、`UE5/Records/p5_p6_plan/P6_美术执行计划_v1.md` 与当前 M2 相机/交互点源码。
