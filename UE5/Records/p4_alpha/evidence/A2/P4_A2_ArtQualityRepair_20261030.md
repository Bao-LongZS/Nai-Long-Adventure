# P4 美术资源 UE 画质修复记录（2026-10-30）

## 原因

1. A4 预览仍用纯色方块搭竞技场；已有 A2 精细背景 v02 没接入 A4。运行时角色只会在地图带 `NaiwaP4A2RefinedArena` 标记时显示场景板。
2. A1 角色材质原来是 Masked，并把 alpha 接到 Opacity Mask、阈值 0.08。PNG 半透明抗锯齿边缘被硬切，浅 alpha 杂点会变成清晰不自然的硬边。
3. P4 导入脚本引用了 UE 5.8 不存在的 `TC_USERINTERFACE2D`；旧脚本无法应用该质量配置，不能据旧报告认定属性已生效。A1/W3 脚本预设 Sharpen3，也会放大源图的低 alpha 边缘杂点。
4. A2 背景源稿为 1672×941，而非目标 2560×1440。即使无损导入，也无法增加源稿里不存在的细节；这会在高分辨率画面中留有轻微软化。

UE 5.8.3 随附头文件和 Epic Python API 将 `TC_EDITOR_ICON` 定义为 Uncompressed (RGBA8)；选用它代替不存在的 UI2D enum。该配置保留 alpha 通道，并为高频 2D 贴图关闭有损 block compression。

## 修复与应用

- A1 33 帧、W3 2 张敌人静帧：设为 sRGB、`TC_EditorIcon` RGBA8、Character 组、Sharpen0 mip、Never Stream。
- A1 运行时材质改为 Translucent；RGB 接 Emissive，straight alpha 接 Opacity，让半透明抗锯齿边缘平滑显示，不再硬切。
- A2 场景贴图使用 sRGB、`TC_EditorIcon` RGBA8、UI 组、Sharpen0 mip、Never Stream；创建背景材质及固定机位预览地图 `/Game/Naiwa/P4/Maps/L_NaiwaP4_A2_RefinedArena_v02`。
- A4 预览图接入 A2 背景板并移除旧方块布景。地图加上专用标记，运行时 Pawn 按相机参数把背景板放到战斗层后方；当前代码已完成最终 Editor 编译，81/81 actions 成功。
- A3 的 6 张卡图标/类别标记使用 sRGB、`TC_EditorIcon` RGBA8、UI 组、NoMipmaps、Never Stream。
- 全程复用原 A1/W3/A3 源文件；未重采样、重绘角色或 UI 图像，也没有改项目级/用户级画质配置。

## UE 应用结果

- UE 5.8.3 回读报告：42 张纹理属性全部通过，其中 A1/W3 角色 35 张、A3 UI 6 张、A2 场景 1 张；报告在 `P4_TextureQuality_report.json`。
- A2 场景构建报告状态为 `map_created_pending_runtime_review`；背景纹理、材质和预览地图已保存，见 `P4_A2_RefinedArena_build_report.json`。A4 综合预览地图为 `/Game/Naiwa/P4/Maps/L_NaiwaP4_ArtIntegrationPreview`，构建报告见 `../A4/P4_ArtIntegrationPreview_build_report_v02.json`。
- 最终 UE Editor 编译记录：`P4_A2_RefinedBackdrop_compile_Final_20261030.log`；结果 `Succeeded`，81 actions。
- A1 33 张同源帧均为 `reused_same_source`；报告在 `../A1/P4_A1_clean_frame_import_report_v02.json`。
- W3 两张同源静帧均为 `reused_same_source`；报告在 `../W3/P4_W3_StaticPreview_import_report_v02.json`。
- A2 背景纹理、材质及地图已保存，清单和构建报告分别在 `../../../../SourceAssets/P4/Manifests/P4_A2_ArenaScene_manifest_v02.json` 和 `P4_A2_RefinedArena_build_report.json`。
- A4 预览地图、角色材质、33 行 A1 动作表、4 行卡牌表和 6 张 UI 纹理已重建；报告在 `../A4/P4_ArtIntegrationPreview_build_report_v02.json`。

## 尚待负责人画面验收

- 没有切换负责人当前使用的编辑器窗口，也没有运行 PIE/Standalone。UE 资产属性已读回，A2/A4 预览地图资产已生成，但实际角色边缘、背景取景、角色落地、遮挡和 720p/1080p 可读性仍须在运行画面确认。
- A1 清理副本仍可能留下少量低 alpha 边缘像素；本次改材质和 mip 只能避免硬切与额外锐化，若实际仍可见，需回到源帧按已批准清理规则修边。
- A2 背景 v02 目前只有 1672×941；本次不声称原生 1080p 或 2K 细节。
- `TC_EditorIcon` 是无压缩 RGBA8，显存占用高于 BC 压缩格式。A1 33 帧仅 base mip 约 66 MiB；角色 mip 链还会增加占用。当前尺寸适合 α 固定机位验收，发布优化需在质量与设备内存间另行评估。

## 引擎依据

- Epic [UE 5.8 TextureCompressionSettings Python API](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/TextureCompressionSettings)列出真实枚举与压缩说明；引擎 `TextureDefines.h` 标明 `TC_EditorIcon` 为 Uncompressed (RGBA8)。
- Epic [UE 5.8 Texture Asset Editor](https://dev.epicgames.com/documentation/unreal-engine/texture-asset-editor-in-unreal-engine?lang=en-US)说明压缩设置、alpha、纹理格式及 mip 信息的查看方式。
