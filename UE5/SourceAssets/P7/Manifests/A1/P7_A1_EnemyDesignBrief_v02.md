# P7 A1 Enemy Animation Frames v02

Date: 2026-11-01 (Asia/Shanghai)
Status: all three Alpha enemy roles have a three-frame baseline set. The original normal and fast silhouettes are user-approved. The six baseline continuation frames remain candidates awaiting motion-set review. Normal and jammer F01 compressed-step v02 alternatives were approved on 2026-11-12; the fast-enemy F01 compressed-step v02 alternative was generated on 2026-11-12 and is awaiting user review. No timing, UE import or in-game scale acceptance is claimed.

## Frame map

| Role | Frame 0 | Frame 1 | Frame 2 | Visual rhythm |
|---|---|---|---|---|
| Normal | `Enemies/A1/P7_A1_Enemy_Normal_v01.png` | `Enemies/A1/P7_A1_Enemy_Normal_Frame02_v01.png` | `Enemies/A1/P7_A1_Enemy_Normal_Frame03_v01.png` | Broad body, low stance, compact passing pose, alternate foot reach |
| Fast | `Enemies/A1/P7_A1_Enemy_Fast_v01.png` | `Enemies/A1/P7_A1_Enemy_Fast_Frame02_v01.png` | `Enemies/A1/P7_A1_Enemy_Fast_Frame03_v01.png` | Forward lean, compressed pass, alternate extended stride |
| Interferer | `Enemies/A0/P7_A0_Enemy_Jammer_v01.png` | `Enemies/A1/P7_A1_Enemy_Jammer_Frame02_v01.png` | `Enemies/A1/P7_A1_Enemy_Jammer_Frame03_v01.png` | Approved lilac jammer identity; compressed pass then alternate stride |

All nine frames are 1254×1254 RGBA PNGs. The six new frames were checked for true transparency, non-empty opaque subject pixels and antialiased partial alpha. Each frame remains an individual source asset to keep later atlas packing and UE import settings reversible. Hashes, stable AssetIds, frame indices and proposed Unreal paths are in `P7_A1_EnemyAssets_v02.csv`.

## Review status

- Base silhouettes for normal and fast: user-approved; carried forward unchanged as frame 0.
- Interferer frame 0: the already approved A0 sample; reused in place without copying or editing.
- Six new motion frames: generated candidates; pending user review for identity consistency, readable pose changes and loop rhythm.
- Normal and jammer F01 compressed-step v02 alternatives: user-approved visual alternates; original v01 frames remain unchanged and the alternates are not imported.
- Fast F01 compressed-step v02 alternative: new candidate, pending user visual review; original v01 frame remains unchanged and the candidate is not imported. See the [three-role comparison board](../../Enemies/A1/Review/P7_A1_EnemyMotionAlternateReview_v02.png), [candidate manifest](P7_A1_EnemyMotionAlternateAssets_v02.csv) and [alternate brief](P7_A1_EnemyMotionAlternateDesignBrief_v02.md).
- 48px 彩色/灰度缩略检查已于 2026-11-11 完成；透明棋盘上的边缘预览和真实 48×48 下采样见[评审页](../../Enemies/A1/Review/P7_A1_EnemyMotionReadabilityReview_v01.png)。普通型、快速型、干扰型各三帧均可读；普通型和干扰型的部分轮廓差异较小，列入动作连续性复核重点。
- 首帧另叠加在 P7 1280×720 灰盒路线，比较 36/48px 彩色与灰度：[`P7_A1_EnemyTrackContrastReview_v01.png`](../../Enemies/A1/Review/P7_A1_EnemyTrackContrastReview_v01.png)。48px 更醒目，36px 更贴近单条路径带宽；实际战场尺寸、遮挡和 A/B 路径交叉仍待 UE 视口确认。
- UE atlas/import, pivot/bounds calibration, final 1080p/720p in-game scale, playback timing and runtime review: pending.
- Review board can be rebuilt with `UE5/Scripts/build_p7_enemy_motion_review_v01.ps1`; it does not modify the nine source frames.
- Track contrast board can be rebuilt with `UE5/Scripts/build_p7_enemy_track_contrast_review_v01.ps1`; it uses the current P7 graybox screenshot and does not modify enemy source frames.

## Motion-pose prompt set

The built-in ImageGen workflow used each approved/base frame as an identity reference and requested a pose-only variation, preserving palette, face, outline, proportions, facing direction and distinguishing marks:

- **Normal frame 2:** compressed passing/contact pose; feet gather under the broad low body and arms counter-swing slightly.
- **Normal frame 3:** alternate stride; rear foot lifts and front foot reaches while the body stays low and stocky.
- **Fast frame 2:** compressed pass with long limbs close beneath the slim forward-leaning body; fins trail behind.
- **Fast frame 3:** alternate extended stride with long separated limbs and forward lean.
- **Interferer frame 2:** compact passing pose; preserve the lilac body, swept crest and orange lightning mark.
- **Interferer frame 3:** opposite extended stride; preserve the approved jammer identity and mark.

Every prompt required genuine transparent alpha and excluded checkerboard backgrounds, scene props, text, weapons, liquid, slime, cream drips and wet gloss.
