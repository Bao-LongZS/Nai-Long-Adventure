# P6 A1 奶家大院精细化灰盒交接 v04

日期：2026-11-12  
状态：**离线美术评审候选；UE v04 单网格已导入，M2 固定机位审阅图 v06 与真实 720p/1080p 截图 v11 已保存；首轮检查发现屋顶遮挡与夜景光色差距，14 点可见性和运行时摆位仍待复核。**

## 本轮改进

- 保留 v03 模型与当前 M2 的 14 个点位，不移动关卡结构或交互锚点。
- 戏台前沿增加 5 组带金属圆钉的木作浮雕；台板补 44 道低对比顺纹细线。
- 房门补金属铰链带和铆钉，房屋两扇窗补小型搭扣。
- 离线预览由单向面光照调整为暖色主光、冷色天光补光；PNG 另加地面方向投影与柔化近似，并给四盏壁灯加轻微暖色泛光。
- 预览输出再提高了显示曝光/伽马中间调，并减轻投影对地面的压暗；这只是展示曲线，不改 OBJ/MTL 几何或 UE 内运行时曝光。
- v04 的 MTL 抬亮蓝灰屋瓦和主木色半档，让屋面与檐下木构的中间调更容易辨认。
- 预览颜色、地面投影与壁灯泛光只服务于本轮可读性评审，不表示 UE 灯光/材质已经完成，也不替代引擎里的动态投影。

## 源模型

- `Models/P6_A1_CourtyardKit_Blockout_v04.obj / .mtl`
- 1,523 个对象组、54,944 个顶点、49,534 个面、41 个基础色材质。
- 厘米单位；模型边界与 v03 一致：X −3550…3550、Y −2898.518…2750、Z −110…1875。
- 新增对象仅为细节层，现有 v01 对象组和 M2 点位坐标保持不变。

## UE 集成状态

- UE 专用合并源按 40 种实际使用材质分批，预补偿 UE OBJ 导入的 Y 轴翻转；原始 v04 OBJ/MTL 没有改动，导入后 bounds 与此处源坐标一致。
- UE 5.8.3 已导入一个 Static Mesh，40 材质槽 / section，Nanite 91,460 tris，fallback 27,520 tris；新增 UV 仅供平色预览，不是正式展开。
- 隔离审阅地图 `/Game/Naiwa/P6/Art/A1/Maps/L_NaiwaP6_A1M2LightingReview_v06` 使用当前 M2 点位表的真实固定机位，M2 主地图未修改。当前冷夜主光/补光和暖灯仍是待调整的试配方案。
- UE 5.8.3 实际 D3D12 截图：`UE5/Records/p6_art/evidence/P6_A1_M2LightingReview_1920x1080_v11.png` 与 `P6_A1_M2LightingReview_1280x720_v11.png`。视角与 M2 相机匹配，但仍是灰盒，画面偏平，前景屋顶遮挡院内点位；点位轮廓、点击范围和运行时验收尚未通过。
- 离线深度代理为完整屋顶 4/14、隐藏屋顶/檐口 5/14 pivot 可见；这是遮挡风险提示，不是 UE 几何轮廓或交互验收。M2 关卡点位与逻辑未修改。
- `UE5/Records/p6_art/evidence/P6_A1_M2VisibilityReview_v01.svg` 将 v11 截图叠加 14 个 pivot 标记、遮挡候选和 A0 夜景缩略图，便于逐点审阅；标记不是运行时交互面或 Actor bounds。
- 导入统计、相机、灯光与未完成项见 `P6_A1_UE_Integration_v04.md` 及 `UE5/Records/p6_art/evidence/P6_A1_M2LightingReviewMap_v06.json`、`P6_A1_M2LightingReviewCapture_v11.json`。v05/v10 是较平视的历史机位试拍。

## 评审图

- `P6_A1_CourtyardKit_Blockout_Preview_v04.png / .svg`：完整屋顶总览。
- `P6_A1_CourtyardKit_CutawayPreview_v04.png / .svg`：临时隐藏屋顶的院内关系图，不代表改关卡。
- `P6_A1_HouseDetailPreview_v04.png / .svg`、`P6_A1_StageDetailPreview_v04.png / .svg`：立面及戏台近景。
- `P6_A1_PhotoTableDetailPreview_v04.png / .svg`、公告栏/检测仪/井/奖杯近景：道具检查。
- `P6_A1_M2CameraPreview_v04.png / .svg` 与剖切对照：当前镜头的离线投影；完整屋顶仍有 4/14 个点位中心通过深度代理，剖切图为 5/14。它们不是 UE 网格轮廓或点击区域验收。
- `P6_A1_ReviewBoard_v04.svg`：包含同版本总览、局部和色样的评审板。

## 验收边界

当前仍是可编辑 OBJ/MTL 灰盒。正式 UV 展开、贴图、手工法线/碰撞/LOD、符合 A0 夜景概念的 UE 材质与灯光、14 点真实轮廓、角色脚底、交互可点击范围及可达性仍待引擎内核对。M2 点位表仍以 `Manifests/P6_A1_CurrentPointArtMap_v01.json` 为准。

### 生成脚本

- `UE5/Scripts/build_p6_a1_courtyard_kit_v04.mjs`
- `UE5/Scripts/render_p6_a1_courtyard_preview_v04.mjs`
- `UE5/Scripts/build_p6_a1_review_board_v04.mjs`
- `UE5/Scripts/build_p6_a1_courtyard_kit_index_v04.mjs`
