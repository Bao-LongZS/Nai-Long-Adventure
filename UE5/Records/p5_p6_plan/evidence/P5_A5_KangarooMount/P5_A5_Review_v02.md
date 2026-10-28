# P5 A5 袋鼠坐骑批准基底清理与图集导出 v02

日期：2026-10-28  
状态：**项目所有者通过此版作为制作基底；1024² 清理图集已导出，玩法锚点与 UE 集成仍待验收**

## 基底与导出

- 项目所有者在 2026-10-28 明确通过所附六姿态版本作为后续制作基底。
- 原图逐字节保存在 `UE5/SourceAssets/P5/Environment/Source/C_Kangaroo_Ride_ApprovedBase_Source_20261028.png`（1254×1254 RGBA，SHA256 `2B5D81345E0D4D668E3E93BEC6CEFBA858E4D397CC0FC2275F99487C5D56EFB89`）。
- 规格图集导出为 `UE5/SourceAssets/P5/Environment/Exports/C_Kangaroo_Ride_v04_WIP.png`（1024×1024 RGBA，SHA256 `BD63321DF62D3E4F3AD1E3F119476C9161211A5AF0653251CDF26C48A7F0CD66`）。
- 可由 `UE5/Scripts/p5_export_a5_kangaroo_candidate.ps1` 从批准基底重建；版本化帧矩形和脚底锚点候选见 `UE5/SourceAssets/P5/Manifests/P5_A5_KangarooMount_manifest_v02.json`。

## 清理与帧序

- 仅对导出副本按单格执行 alpha≥24 的 3×3 形态学开运算，去除游离彩点并保留最大袋鼠主体；再保留两像素软 alpha 边。不改动批准源图。
- 第五帧三个蓝色受击反馈笔触保留；其它游离彩点已从导出副本移除。透明画布抽样点 alpha 为 0。
- 六帧顺序为 AvailableIdle、ReadyToCarry、DownhillGlide、Bounce、ImpactReact、RecoveredIdle。
- 输出网格是 3 列×2 行。因为 1024 不能被 3 整除，列宽依次为 341、342、341；每帧矩形按此记录。

## 待验收项

- 当前脚底锚点取各帧主体 alpha 外接框的底部中心，属于对齐候选；雪坡法线、真实角色比例与动画插值尚未验证。
- `rideAnchor` 和帧停留时间暂空，等奶蛙骑乘呈现和碰撞/育儿袋状态的实际叠放关系确定后再定。
- 透明边在 UE 纹理压缩/过滤下、1080p/720p 清晰度与同屏 HUD 可读性尚未检查。
- 导出继续保留 `_WIP`，不代表运行时最终资产或 A5 交付门已通过。
