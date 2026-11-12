# P6 A1 院落模块灰盒交接

日期：2026-11-11  
状态：**模块概念与 OBJ/MTL 源块候选；不是正式场景、碰撞体或 UE 验收资产。**

## 文件

- `P6_A1_EnvironmentKit_Concept_v02.png`：12 个模块族的干净不透明评审板，仅供造型方向参考；v01 透明接触表保留为历史稿。
- `Models/P6_A1_CourtyardKit_Blockout_v01.obj`：程序化搭建的低多边形院落灰盒源模型。
- `Models/P6_A1_CourtyardKit_Blockout_v01.mtl`：基础材质表，供 DCC / OBJ 预览。
- `P6_A1_CourtyardKit_Blockout_Preview_v01.svg / .png`：完整屋顶版本的可编辑矢量投影和静态深度预览。
- `P6_A1_CourtyardKit_CutawayPreview_v01.svg / .png`：临时隐藏屋顶/檐口、保留墙体与院内道具的剖切查看版本；不是关卡修改。
- `P6_A1_M2CameraPreview_v01.png / .svg`：用当前点位图中的精确正交投影公式生成 1920×1080 模型预览并标出 14 个 Actor 锚点序号；PNG 的环色只反映软件深度缓冲是否遮住 Actor pivot。
- `P6_A1_M2CameraCutawayPreview_v01.png`：同一投影下临时隐藏屋顶/檐口的预览，用于对照大型屋顶遮挡风险。
- `P6_A1_M2CameraDepthProxy_v01.json` / `P6_A1_M2CameraCutawayDepthProxy_v01.json`：按点记录最近的投影表面组和深度差。若最近表面是该点自己的道具部件，属于 pivot 自遮挡代理，不代表整个模型不可见。
- `P6_A1_M2OcclusionMatrix_v01.svg`：14 点最近表面组单页矩阵，区分持续空间遮挡、结构变化和自家几何；仍为离线诊断。
- `P6_A1_ReviewBoard_v01.svg`：自包含 A1 评审总览，内嵌方向板、完整屋顶、剖切和 M2 公式投影图。
- `P6_A1_MaterialPalette_v01.svg`：从 OBJ/MTL 自动读取的 16 色平涂样表，用于颜色方向评审，不代表 UE PBR 或灯光结果。
- `UE5/Scripts/render_p6_a1_courtyard_preview.mjs`：从 OBJ/MTL 重新导出离线投影；`node ... --cutaway` 隐藏屋顶/檐口，`node ... --m2` 使用当前 M2 相机公式并叠加 Actor pivot，二者可组合。
- `../../../Manifests/P6_A1_CourtyardKit_Index_v01.json`：资产 ID、来源、几何指标和状态索引。

## 模块内容

场景骨架包括地面、四周院墙、门洞与门帽；重点地标包括戏台、奖杯、房屋立面、井、树冠和两盆植栽。线索承载小件提供公告栏、静音检测仪、相机桌、巡夜簿桌、垃圾桶、晾衣绳与瓦片堆等可单独选择的模型组。所有对象按部件拆组，允许环境美术在 DCC 内单独移动、删减或替换。当前灰盒有压缩形体和屋顶遮挡，因此输出完整屋顶与临时剖切两张深度正确的静态软件投影查看；它们都不是 UE 镜头截图。

灰盒模型共 112 个 OBJ 对象组、1,640 个顶点和 1,587 个面；包围范围为 X −3550…3550cm、Y −2875.106…2750cm、Z −110…1859.072cm。源坐标以厘米记录，地面中心为原点，Z 为高度，院落长边沿 X。OBJ/MTL 是可修改的源块，不附 UE 碰撞设置、不代表 14 个 M2 交互点最终空间位置，也未通过固定镜头或步行视线验收。v01 透明接触表边缘有色边和杂点；v02 改为统一暖灰不透明底，保留 12 个模块设计且边缘干净。两张接触表只作外观评审参考，没有自动转成模型或纹理，也不用于正式拆件/贴图。
- `Manifests/P6_A1_PointPropAlignment_v01.json` 将 14 个 M2 Actor ID 对到候选模型组；11 个场景道具的 XY 锚点匹配当前 M2 坐标，三名 NPC 对到 A2 图录原图候选。此表只是灰盒映射参考，不是锁定后的关卡摆位。

## 交接要点

1. 导入 DCC / UE 前先核对目标轴向和单位；保持 1:1 厘米比例、Z-up 语义及对象组名称，必要时仅在导入设置里做轴变换。
2. 将 OBJ 当成可编辑、无 UV 贴图的几何源块。MTL 色值是平涂基准；正式资产应在 UE 内建立符合项目材质规范的材质，不依赖 OBJ 材质作为最终着色。
3. 根据真实场景拆分可遮挡建筑、背景轮廓、可碰撞体和纯装饰物；当前对象组边界不是最终网格拓扑，也没有经过 UV、法线、碰撞或 LOD 制作。
4. 将模型与 `P6_A1_CurrentPointArtMap_v01` 对照后再处理奖杯、公告栏、检测仪、相机、假花/脚印与巡夜簿的摆放。点位图只反映 M2 坐标，不能直接覆盖正式关卡设计。
5. 先完成戏台、奖杯、一个 NPC 点位的三点相机样片，再扩展 14 点；需另存 1920×1080 与 1280×720 的 Editor 预览、可读性/遮挡记录和人工可达性结论。
6. 与 M2 点位图同公式的静态投影显示屋顶/墙体占屏较大。深度代理将主线点 Trophy、Noticeboard、Detector、PhotoTable、Flowers、PatrolLedger 标为 pivot 未通过，但细看最近表面组后，部分标记落在自家底座/桌面上；不能据此说物件本身不可见。应优先在 UE 检查 Noticeboard 对应树冠、Detector/PhotoTable 及 NaiPai 对应房屋主体/屋顶的空间遮挡，并用真实 actor bounds、billboard 轮廓与点击面复核。完整屋顶与临时剖切结果分开记录，剖切仅用于讨论，不代表批准的场景修改。

最近表面组明细（离线单像素诊断，不作 UE 结论）：

| 点位 | 完整屋顶最近表面组 | 剖切最近表面组 | 复核方向 |
|---|---|---|---|
| Trophy | `P6_Stage_Roof` | `P6_Trophy_Foot` | 看奖杯整体轮廓；剖切标记落在自家脚座 |
| Noticeboard | `P6_Tree_Canopy_A` | `P6_Tree_Canopy_A` | 树冠与公告栏的真实镜头空间冲突 |
| Detector | `P6_House_RoofSlope_Right` | `P6_House_MainVolume` | 房屋屋顶/主体均挡在投影前方 |
| PhotoTable | `P6_House_RoofSlope_Left` | `P6_House_MainVolume` | 房屋屋顶/主体均挡在投影前方 |
| Flowers | `P6_House_RoofSlope_Left` | `P6_G5_FakeFlowers_Pot` | 分清屋顶遮挡与自家花盆 pivot 重叠 |
| PatrolLedger | `P6_Ledger_Desk` | `P6_Ledger_Desk` | 检查 pivot 是否在桌体内部，勿判作遮挡失败 |
| NaiPai | `P6_House_RoofSlope_Right` | `P6_Pot_Right_Plant` | Billboard 与房屋/盆栽的 720p 轮廓检查 |
| TrashCan | `P6_TrashCan_Lid` | `P6_TrashCan_Lid` | pivot 位于自身桶盖的代理现象 |
| RoofTiles | `P6_RoofTile_Stack` | `P6_Ground_CourtyardPlaza` | 剖切后 pivot 通过；点位自身在低处 |
| WellBucket | `P6_Well_Water` | `P6_Well_Water` | pivot 与井水网格重合，需看实际 bucket silhouette |

## 来源与边界

造型参考来自已通过的 P6 院落概念和 P6 A1 点位/环境 kit 方向文件。奶蛙角色不在本模型包内；角色图仍直接引用 `References/MilkFrogStudy/` 的图录原图裁切，不复制到模型目录或重绘。此轮未改 UE Content、Actor 数据或 M2 坐标。
