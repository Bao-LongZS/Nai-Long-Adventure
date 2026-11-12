# P6 A1 院落精细化灰盒交接 v03

日期：2026-11-11  
状态：**精细化源块评审候选；尚未导入 UE，未通过固定镜头、可达性或运行时验收。**

## 本轮细化

v03 从 v02 源块继续制作，保留 v01 原始对象组、模型轴向、当前 M2 点位表和全部 14 个 XY 锚点，不改正式关卡坐标。新增内容包括：

- 以约 1.8 米尺度的错缝排布、轻微尺寸/角度差、八边倒角和少量细裂痕重做院内石板；共 825 片，让地面不再读成粗大的规则方格。
- 将屋面圆弧瓦按单片作低对比冷暖变化，并补檐角翘起端饰、脊端帽和脊线陶饰。
- 补房门浮雕分格、环形门扣、门边石件、窗台滴水线；给房屋东侧加凹入通风窗、木格栅、百叶和连续石窗台。
- 补树枝/分层树冠、地面小型阔叶簇、井桶木板与铁箍、井口压边和水纹、陶罐刻线、戏台帷幕与金色帘头、11 道地板拼缝/钉头、踏步鼻口和栏杆帽，以及检测仪铆钉/刻度。
- 在房屋立面和戏台两侧各补一盏带挂环、支架、框条与暖色灯面的壁灯；一共四盏，与风格参考板呼应。
- 相机桌补镜头面板螺钉、内圈、顶部快门、侧转盘/背带；桌沿加米色布衬、红色下摆和细缝线。
- 石基与墙砖使用相邻低对比色阶；材质仍是评审用平涂，不是最终 PBR 或贴图。

## 源模型与预览

- `Models/P6_A1_CourtyardKit_Blockout_v03.obj / .mtl`：1,476 个对象组、52,904 顶点、47,938 面、38 个材质；厘米单位，包围范围 X −3550…3550、Y −2898.518…2750、Z −110…1875。
- `P6_A1_CourtyardKit_Blockout_Preview_v03.png / .svg`：完整院落总览。
- `P6_A1_CourtyardKit_CutawayPreview_v03.png / .svg`：仅供观察院内结构的临时剖切视图，不代表关卡屋顶变更。
- `P6_A1_HouseDetailPreview_v03.png / .svg` 与 `P6_A1_StageDetailPreview_v03.png / .svg`：为看清立面/帷幕细节，临时隐藏屋面组的轴测近景；完整屋面仍见全景图。
- `P6_A1_PhotoTableDetailPreview_v03.png`、`P6_A1_NoticeboardDetailPreview_v03.png`、`P6_A1_DetectorDetailPreview_v03.png`、`P6_A1_WellDetailPreview_v03.png`、`P6_A1_TrophyDetailPreview_v03.png`：相机桌和四个交互物近景；相机桌图只保留相机桌/地面以避免检测仪重叠，各自 SVG 版本可编辑。
- `P6_A1_M2CameraPreview_v03.png / .svg` 和 `P6_A1_M2CameraCutawayPreview_v03.png / .svg`：按现有 M2 相机公式叠加 14 点锚点；同名 depth proxy JSON 记录逐点离线深度比较。
- `P6_A1_ReviewBoard_v03.svg`：概念方向、完整屋顶、M2 投影、临时剖切、房屋/戏台及相机桌/四个交互物近景和 MTL 色样自包含总览。
- `Manifests/P6_A1_CourtyardKit_Index_v03.json`：对象/顶点/面/材质指标、锚点与原始组保留情况、资产路径和哈希。

## 验收边界

v03 是比 v02 更精细的可编辑 OBJ/MTL 源块，仍没有 UV、纹理、法线贴图、碰撞或 LOD，也未在 UE 内核对材质实例、法线方向、遮挡、角色脚底、点击面、步行路线、性能和 1920×1080 / 1280×720 画面。当前离线 pivot 深度代理仍为完整屋顶 4/14、临时剖切 5/14；它不能代替真实网格轮廓或 UE 可见性验收。

源 OBJ 将 825 片铺地保留为独立对象，便于本轮调整和色样预览；正式导入时建议按材质合并铺地静态网格，仅保留交互道具/建筑模块需要的独立对象，避免把源组数直接转成大量 Actor。

## 下一步

1. 在 UE 导入 v03，并按固定 3/4 镜头检查 1080p 与 720p 的主要交互物轮廓。
2. 先复核公告栏与树冠、检测仪/相机桌/奶拍点与房屋、井桶与井口等离线命中的位置；由 Level Design 决定是否调整最终构图/摆位。
3. 确认门窗、石板、屋面细节的 UV/材质拆分，再补碰撞、LOD 和引擎内性能/可达性检查。

### 生成脚本

- `UE5/Scripts/build_p6_a1_courtyard_kit_v03.mjs`
- `UE5/Scripts/render_p6_a1_courtyard_preview_v03.mjs`
- `UE5/Scripts/build_p6_a1_review_board_v03.mjs`
- `UE5/Scripts/build_p6_a1_courtyard_kit_index_v03.mjs`
