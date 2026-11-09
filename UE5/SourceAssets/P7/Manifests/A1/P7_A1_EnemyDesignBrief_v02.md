# P7 A1 Enemy Animation Frames v02

Date: 2026-11-01 (Asia/Shanghai)
Status: all three Alpha enemy roles now have a three-frame source set. The original normal and fast silhouettes were approved by the user. Four normal/fast motion frames and both jammer continuation frames are new candidates awaiting motion-set review. No timing, UE import or in-game scale acceptance is claimed.

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
- 48px thumbnail and grayscale readability checks: pending.
- UE atlas/import, pivot/bounds calibration, 1080p/720p in-game scale, playback timing and runtime review: pending.

## Motion-pose prompt set

The built-in ImageGen workflow used each approved/base frame as an identity reference and requested a pose-only variation, preserving palette, face, outline, proportions, facing direction and distinguishing marks:

- **Normal frame 2:** compressed passing/contact pose; feet gather under the broad low body and arms counter-swing slightly.
- **Normal frame 3:** alternate stride; rear foot lifts and front foot reaches while the body stays low and stocky.
- **Fast frame 2:** compressed pass with long limbs close beneath the slim forward-leaning body; fins trail behind.
- **Fast frame 3:** alternate extended stride with long separated limbs and forward lean.
- **Interferer frame 2:** compact passing pose; preserve the lilac body, swept crest and orange lightning mark.
- **Interferer frame 3:** opposite extended stride; preserve the approved jammer identity and mark.

Every prompt required genuine transparent alpha and excluded checkerboard backgrounds, scene props, text, weapons, liquid, slime, cream drips and wet gloss.
