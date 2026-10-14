# Run_v1 FBX 导出诊断

日期：2026-10-11。只读检查原 E2、新 Run_v1 FBX、本机 Blender 5.2.2 / UE 5.8.3 源码与 UE 导入日志。此诊断未修改 FBX、模型资产或原材质。

## 1. 六条 BindPose 警告的来源

结论：新增腹部形态后，Blender 为 shape key 额外输出了一个只含身体 mesh 的 BindPose。UE 将这个局部 Pose 依次对六个角色 mesh 验证，产生六条 `Not all the deformers are stored in this BindPose`。六个完整 Skin BindPose 仍存在并被 UE 判为有效；这批日志不等同于六个 mesh 的真实骨骼 BindPose 都损坏。

直接二进制解析结果：

| FBX | Pose数量 | Pose结构 |
|---|---:|---|
| 原 E2 `Naiwa_Character.fbx` | 6 | 每个 Skin Pose 都包含本 mesh、`Naiwa_Rig`、原10骨，共12节点。 |
| 新 `Naiwa_Elastic_Run.fbx` | 7 | 多一个名称 `pear body mesh` 的 shape-only Pose，只包含 `Naiwa unified golden body` 一个节点；另外六个 Skin Pose 仍各有12节点。 |

六个完整 Skin Pose 的名称与节点结构和原 E2 一致：

1. `L concentric iris and pupil independent sphere.001`
2. `Sphere`
3. `pear body mesh`（该完整 Skin Pose 与额外的 shape-only Pose 同名，不能仅凭名称判别）
4. `R concentric iris and pupil independent sphere.001`
5. `Sphere.001`
6. `Recessed dark brown tapered oral cavity.001`

本机 Blender 导出实现：

- [export_fbx_bin.py:726](D:/STEAM/steamapps/common/Blender/5.2/scripts/addons_core/io_scene_fbx/export_fbx_bin.py:726)：`fbx_data_bindpose_element(... arm_obj=None, bones=[])` 同时供形态与骨骼使用；没有 armature 参数时将 mesh 自身当作 armature，输出1节点。
- [export_fbx_bin.py:822](D:/STEAM/steamapps/common/Blender/5.2/scripts/addons_core/io_scene_fbx/export_fbx_bin.py:822)：形态导出额外调用该函数；本机源码还注明同一 mesh 的多个 BindPose 可能存在兼容性问题。
- [export_fbx_bin.py:1851](D:/STEAM/steamapps/common/Blender/5.2/scripts/addons_core/io_scene_fbx/export_fbx_bin.py:1851)：随后针对每个有 Skin 的 mesh，带 armature 和全部骨骼另写完整 BindPose。

本机 UE 验证实现：

- [FbxSkeletalMeshImport.cpp:952](<C:/Program Files/Epic Games/UE_5.8/Engine/Source/Editor/UnrealEd/Private/Fbx/FbxSkeletalMeshImport.cpp:952>)：`RetrievePoseFromBindPose` 遍历场景中的每个 BindPose。
- [FbxSkeletalMeshImport.cpp:973](<C:/Program Files/Epic Games/UE_5.8/Engine/Source/Editor/UnrealEd/Private/Fbx/FbxSkeletalMeshImport.cpp:973>)：对每个 Pose 再遍历当前导入的全部 mesh 节点，调用 FBX SDK `IsValidBindPoseVerbose`。
- [FbxSkeletalMeshImport.cpp:978](<C:/Program Files/Epic Games/UE_5.8/Engine/Source/Editor/UnrealEd/Private/Fbx/FbxSkeletalMeshImport.cpp:978>)：有效 Pose 加入候选数组；额外的 shape-only Pose 缺骨骼变形器节点，导致所见警告。

实际证据：[runimport.log:1624](C:/Users/31314/Desktop/Nai-Long-Adventure/UE5/Records/runimport.log:1624) 到1629是额外 shape-only Pose 对六个 mesh 的六条警告；[runimport.log:1630](C:/Users/31314/Desktop/Nai-Long-Adventure/UE5/Records/runimport.log:1630) 到1635随后六个完整 Skin Pose 均输出 `Valid bind pose`。日志末尾重复摘录警告，不应重复计为新增十二个问题。

结合本地已验证的原10骨静止矩阵/父子关系一致，以及 UE 导入审计的旧/新 Skeletal Mesh 每骨参考变换最大差值0，目前证据支持「附加形态局部 Pose 的导出兼容警告」，未发现骨架参考姿态漂移。Run 与 Morph 的实际变形仍按实机验证记录。无需为了消除这批文字警告而修改原 E2 或使用 `Use T0 as ref pose`；后者会把跑步首帧带入参考姿态。

## 2. UE 颜色变浅的数值与实现链

结论：原、新 FBX 的顶点颜色相同。Blender 默认将线性色属性转成 sRGB 数值导出，当前 UE 传统骨骼 FBX 导入和 GPU 顶点色通路不做对应的 sRGB gamma 解码。直接把该 VertexColor 接到线性 BaseColor，会使黄皮、腹色和眼睛偏浅。具体实机明暗还会叠加光照、曝光与色调映射。

从保存的 E2 读取 `FLOAT_COLOR` 的 `color`，并直接解析两份 FBX 的 `LayerElementColor/Colors`：

| 区域 | Blender 线性值 | 原/新FBX实值（相同） |
|---|---|---|
| 主要黄皮 | `(0.94, 0.66, 0.13)` | `(0.973115027, 0.832250714, 0.395871580)` |
| 腹色 | `(0.70, 0.54, 0.33)` | `(0.854272068, 0.761085510, 0.609734356)` |
| 指尖/脚趾 | `(0.19, 0.115, 0.065)` | `(0.473131120, 0.373412341, 0.282769263)` |
| 虹膜绿色 | `(0.105, 0.265, 0.04)` | `(0.357476175, 0.551685333, 0.220915586)` |
| 瞳孔 | `(0.008, 0.010, 0.006)` | `(0.086100228, 0.099847101, 0.070175208)` |

证据链：

- [export_fbx_bin.py:3527](D:/STEAM/steamapps/common/Blender/5.2/scripts/addons_core/io_scene_fbx/export_fbx_bin.py:3527)：本机 FBX 导出器默认参数 `colors_type='SRGB'`。
- [export_fbx_bin.py:1304](D:/STEAM/steamapps/common/Blender/5.2/scripts/addons_core/io_scene_fbx/export_fbx_bin.py:1304)：`SRGB` 模式通过 Blender 属性 `color_srgb` 读取颜色；`LINEAR` 模式才读取 `color`。本轮未显式覆盖该默认。
- [FbxSkeletalMeshImport.cpp:3850](<C:/Program Files/Epic Games/UE_5.8/Engine/Source/Editor/UnrealEd/Private/Fbx/FbxSkeletalMeshImport.cpp:3850>)：`ConvertFbxColorToFColor` 将 FBX 通道 clamp 后直接乘255截成 byte，无 gamma 解码。
- [SkeletalMeshLODImporterData.cpp:2226](<C:/Program Files/Epic Games/UE_5.8/Engine/Source/Runtime/Engine/Private/Rendering/SkeletalMeshLODImporterData.cpp:2226>)：导入数据转 MeshDescription 明确不做 sRGB conversion，调用 `Wedge.Color.ReinterpretAsLinear()`。
- [SkeletalMeshLODImporterData.cpp:2784](<C:/Program Files/Epic Games/UE_5.8/Engine/Source/Runtime/Engine/Private/Rendering/SkeletalMeshLODImporterData.cpp:2784>)：逆转换同样 `bSRGB=false`，避免任何线性转 sRGB。
- [SkeletalMeshLODRenderData.cpp:509](<C:/Program Files/Epic Games/UE_5.8/Engine/Source/Runtime/Engine/Private/SkeletalMeshLODRenderData.cpp:509>)：顶点颜色缓冲直接从 `FSoftSkinVertex.Color` 初始化。
- [D3D12VertexDeclaration.cpp:45](<C:/Program Files/Epic Games/UE_5.8/Engine/Source/Runtime/D3D12RHI/Private/D3D12VertexDeclaration.cpp:45>)：`VET_Color` 使用 `DXGI_FORMAT_B8G8R8A8_UNORM`，属于归一化数值读取，不是 sRGB纹理采样。
- [GpuSkinVertexFactory.ush:824](<C:/Program Files/Epic Games/UE_5.8/Engine/Shaders/Private/GpuSkinVertexFactory.ush:824>)：输入颜色仅作通道 swizzle；[同文件:227](<C:/Program Files/Epic Games/UE_5.8/Engine/Shaders/Private/GpuSkinVertexFactory.ush:227>)直接传给材质 VertexColor。
- [HLSLMaterialTranslator.cpp:9118](<C:/Program Files/Epic Games/UE_5.8/Engine/Source/Runtime/Engine/Private/Materials/HLSLMaterialTranslator.cpp:9118>)：材质 VertexColor 对应 `Parameters.VertexColor`，无 gamma 解码。
- [build_graybox.py:20](C:/Users/31314/Desktop/Nai-Long-Adventure/UE5/Scripts/build_graybox.py:20)：初始 Body/Eye 材质 VertexColor 直接连 BaseColor。

例如主要黄皮会先变为约 byte `(248,212,100)`，GPU归一化值约 `(0.97255,0.83137,0.39216)`，其中蓝通道明显高于源线性0.13。

## 3. 建议处理范围

当前可在专供新游戏 Mesh 的 Body/Eye 材质副本中，加准确的 RGB 分段 sRGB→linear 解码。Alpha保持原值；原本就是线性常量的眼部高光/嘴部材质不套用解码。

```hlsl
float3 c = saturate(VertexRGB);
float3 low = c / 12.92;
float3 high = pow((c + 0.055) / 1.055, 2.4);
return float3(c.r <= 0.04045 ? low.r : high.r,
              c.g <= 0.04045 ? low.g : high.g,
              c.b <= 0.04045 ? low.b : high.b);
```

不要用名称相似的 `MaterialExpressionSRGBColorToWorkingColorSpace` 代替 gamma 解码：本机 [MaterialExpressions.cpp:10502](<C:/Program Files/Epic Games/UE_5.8/Engine/Source/Runtime/Engine/Private/Materials/MaterialExpressions.cpp:10502>) 到10519只处理 RGB 原色/色域矩阵；工作空间是 sRGB 时直接返回输入。

后续若重新建立统一导出规范，可对游戏 FBX 显式选择 `colors_type='LINEAR'`，并移除对应材质解码，避免二次解码。该建议不表示本轮已改写 FBX。当前正式 E2、已导出的 Run_v1 FBX、原材质保持不变。

完整新FBX SHA256：`f5688581ae103f27901bab67a98ba9deef983156ec985308435bb453e2621d2e`。
