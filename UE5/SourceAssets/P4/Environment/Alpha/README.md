# P4 α 首章竞技场环境方向

状态：**场景可读性深化 v02 方向已通过；背景贴图、材质和 A2/A4 预览地图已在 UE 5.8.3 创建，运行画面验收待项目负责人完成。** 对应第一章「沙丘晒笑场」和 P4 固定机位的 α 竖切。

`P4_A2_AlphaArena_ArtDirection_v02.svg` 是更完整的场景分层/落位示意，不是运行时贴图、材质或最终地图。主视图明确沙岩后墙与入场拱门、灰绿战斗地面、两侧低障碍、前沿低边界、玩家/敌群占位和预警示例；中心保持净空。v01 色板构图草图保留为历史版本。深化稿复核说明见 `UE5/Records/p4_alpha/evidence/W3/P4_A2_AlphaArena_ArtDirection_v02.md`。

`P4_A2_ArenaScene_v01.png` 保留首轮构图。`P4_A2_ArenaScene_v02.png` 是精细细节版：沙岩拱门与雕刻墙面、边侧布幔/陶器/荒漠植物、灰绿石板地面；中央战斗区域保持开阔。生成服务输出为 1672×941、16:9、全幅不透明 PNG，尚低于原生 1080p；该限制已记录。UE 纹理使用 `TC_EditorIcon` 未压缩 RGBA8、sRGB、UI 组、Sharpen0 mip、不流送。场景板由固定镜头后方的运行时平面显示，角色、血条、预警和脚底影仍由运行时单独绘制。清单在 `UE5/SourceAssets/P4/Manifests/P4_A2_ArenaScene_manifest_v02.json`；A2 地图为 `/Game/Naiwa/P4/Maps/L_NaiwaP4_A2_RefinedArena_v02`，同一背景已接入 A4 主预览地图 `/Game/Naiwa/P4/Maps/L_NaiwaP4_ArtIntegrationPreview`。A2 构建报告为 `UE5/Records/p4_alpha/evidence/A2/P4_A2_RefinedArena_build_report.json`；纹理属性回读记录为 `UE5/Records/p4_alpha/evidence/A2/P4_TextureQuality_report.json`。两项均通过资产级检查，实际 PIE 构图和脚底/遮挡对齐仍待负责人验收。

P4 的 A1/W3 角色帧配置为无压缩 RGBA8、Character 纹理组与 Sharpen0 mip；A3 图标配置为无压缩 RGBA8、UI 纹理组并关闭 mip。纹理保留 sRGB/alpha，不缩放角色/UI 源图。42 张 A1/W3/A2/A3 贴图的 UE 属性均已回读通过；这不能替代 720p/1080p 实际画面对齐检查。

## 资产复用盘点

- P2 `/Game/Naiwa/P2/Art/Geo/Materials/MI_P2Geo_*` 有地面、石、木、岩、天空等低多边形材质候选；盘点到的 `Art/Geo` 内容为材质资产，没有可直接引用的独立场景网格。候选仅供 TA 对比材质分层和几何风格，未做 P4 运行引用。
- P3 `T_P3Floor_A_v2`、`T_P3HardStone_v2` 是偏冷蓝的高频夜巡石材纹理，`T_P3WoodCrate_v2` 是深木纹。它们服务俯视迷宫，镜头和章节色调都不匹配；当前仅作为纹理密度/边界细节参考，不直接复用或复制。
- P4 α 场地建议先用 P4 自有基础几何与自有平色/轻纹理材质；若未来在 P2/P3 材质候选中确认有合适引用，应保留原 Content 路径，只在 `/Game/Naiwa/P4/` 新建实例。

## 固定机位审查点

- 黄色角色与地面/边框在 1080p、720p 都能分开；脚底影与地面反差足够但不形成深色圆斑。
- 1100×185 uu 的浅纵深区域不被过密地砖纹、浮雕或装饰物切碎；背景不能压住 2D 角色轮廓。
- 红色攻击圆环、琥珀方向三角、白色不可挡菱形均保留形状编码；颜色作为第二编码，兼顾色觉差异。
- 角色、预警与障碍需要有自己的深度/遮挡顺序；本方向稿尚无 UE 图像证据。
