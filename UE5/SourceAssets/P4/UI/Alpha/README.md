# P4 α 卡牌图标候选

状态：**三枚图标视觉均已通过，尚未导入 UE。** 三张图标对应 alpha 固定卡池，文件使用透明 SVG，512×512 viewBox；卡名和效果数据仍以 `UE5/Records/p4_alpha/design_reference/P4_章节数据_v1.json` 为准。

| CardId | 图标 | 状态 | 视觉语义 |
|---|---|---|---|
| `bounce_ball` | `P4_A3_CardIcon_bounce_ball_v01.svg` | **视觉通过** | 奶球沿弧线弹向第二个目标点。 |
| `rounder_belly` | `P4_A3_CardIcon_rounder_belly_v02.svg` | **视觉通过** | 三格充能的奶蛙鼓起大肚撞向靶体。 |
| `cant_hold` | `P4_A3_CardIcon_cant_hold_v02.svg` | **视觉通过** | 奶蛙受击后闭眼大笑，笑声带出能量亮点。 |

三个图标共用奶油白、奶黄、暖棕描边和少量叶绿点缀；图形不内嵌文字。v01 文件与清单保留为历史稿；当前状态、哈希和单项验收状态见 [`P4_A3_AlphaCardIcons_manifest_v02.json`](../../Manifests/P4_A3_AlphaCardIcons_manifest_v02.json)。预览卡片及 40 px 缩放检查见 [`P4_A3_AlphaCardIcons_v02.html`](../../../../Records/p4_alpha/evidence/W3/P4_A3_AlphaCardIcons_v02.html)。卡面互动 v03 已通过（v02 作为历史通过版保留）；类别标记已导出为独立 SVG。HUD 状态和游戏内可读性仍待 UE 样片复核。

## 全卡面与三选一流程样片

A3 卡面互动样片 v03 **视觉通过**，记录见 [v03 验收记录](../../../../Records/p4_alpha/evidence/W3/P4_A3_AlphaCardFaces_v03_approval.md)。v03 保留 v02 的三选一交互，增加子弹圆点、冲撞菱形、大笑星芒与灰度检查；样片不接游戏或存档。 [卡面预览](../../../../Records/p4_alpha/evidence/W3/P4_A3_AlphaCardFaces_v03.html)，[清单](../../Manifests/P4_A3_AlphaCardFaces_manifest_v03.json)。类别标记已拆为独立 SVG，见 [标记预览](../../../../Records/p4_alpha/evidence/W3/P4_A3_CategoryMarks_v01.html)、[标记清单](../../Manifests/P4_A3_CategoryMarks_manifest_v01.json)。UMG 参考合同：[P4_A3_UMG_视觉交接_v01.md](P4_A3_UMG_视觉交接_v01.md)。这些均为已通过视觉源稿或交接说明，**没有导入 UE**；v01/v02 历史版本保留。
## HUD 与战斗预警样片

HUD/预警视觉和 v02 占屏比例均已通过，尚未导入 UE。预览：[P4_A3_HUDAndTelegraph_v02.html](../../../../Records/p4_alpha/evidence/W3/P4_A3_HUDAndTelegraph_v02.html)；验收清单：[P4_A3_HUDAndTelegraph_manifest_v02.json](../../Manifests/P4_A3_HUDAndTelegraph_manifest_v02.json)。v01 保留为历史稿。




