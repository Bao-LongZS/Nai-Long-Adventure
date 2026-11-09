# P4 W3 Enemy Animation Pose Boards v01

Status: **six-state pose-board WIP; not production sprites or UE-ready atlases.** The owner approved the Naiding and Naidun concept direction on 2026-10-29 and asked art production to continue.

## Selected sheets

The PNGs below are the current review candidates. Most are generated pose contact sheets with views arranged by row and animation frames arranged by column. Hit sheets contain one frame per view, with views arranged left to right.

| Enemy | State | Selected file | Grid | Review note |
|---|---|---|---|---|
| Naiding | idle | `naiding_idle_grid_v03.png` | 3 columns × 3 views | Rear view is faceless; stray alpha pixels remain to clean. |
| Naiding | move | `naiding_move_grid_v01.png` | 4 columns × 3 views | Even 362×362 cells. |
| Naiding | telegraph | `naiding_telegraph_grid_v01.png` | 2 columns × 3 views | Clear crouch/attack intent; grid needs relayout. |
| Naiding | attack | `naiding_attack_grid_v01.png` | 3 columns × 3 views | Candidate pose sequence; grid needs relayout. |
| Naiding | hit | `naiding_hit_grid_v01.png` | 3 views × 1 frame | One pose per view; even 724×724 cells. |
| Naiding | death | `naiding_death_grid_v01.png` | 3 columns × 3 views | Rear-view facial marks need correction; grid needs relayout. |
| Naidun | idle | `naidun_idle_grid_v01.png` | 3 columns × 4 views | Shield stays on the forward side; grid needs relayout. |
| Naidun | move | `naidun_move_grid_v01.png` | 4 columns × 4 views | Shield orientation retained; grid needs relayout. |
| Naidun | telegraph | `naidun_telegraph_grid_v01.png` | 2 columns × 4 views | Even 512×384 cells. |
| Naidun | attack | `naidun_attack_grid_v01.png` | 3 columns × 4 views | Even 512×256 cells; shield bash read is clear. |
| Naidun | hit | `naidun_hit_grid_v01.png` | 4 views × 1 frame | One pose per view; even 543×724 cells. |
| Naidun | death | `naidun_death_grid_v01.png` | 3 columns × 4 views | Even 362×362 cells; shield remains held. |

Naiding idle alternates `v01` and `v02`, plus Naiding attack alternate `v02`, remain in this folder for history; they are not selected because their view cleanup or pose read is weaker. See the manifest for dimensions, hashes, cell math, view order, and timing proposals.

## Frame and view order

- Naiding rows: `front`, `back`, `side_right`.
- Naidun rows: `front`, `back`, `side_right`, `side_left`. Keep the two side directions separate because the shield cannot be mirrored safely.
- Cells progress left to right within each view. The suggested counts are 3 idle, 4 move, 2 telegraph, 3 attack, 1 hit, and 3 death frames per view.
- Initial visual timing proposals only: idle 0.25 s/frame; move 0.10 s/frame; telegraph 0.175 s/frame; attack 0.10 s/frame; hit 0.14 s total; death 0.12 s/frame. Engineering should tune these against the existing W3 attack timings and runtime playback.

## Production blockers

- These are pose boards, not exported 512×512 source frames. Several sheets do not divide evenly into the requested row/column counts, so do not infer import slicing coordinates from the preview.
- Alpha edges, stray transparent-RGB/alpha pixels, foot anchors, loop continuity, and 720p/1080p gameplay readability still need art QA.
- Naiding's death rear view contains face-like marks; the idle sheet also needs speck cleanup.
- Naidun's shield is not yet supplied as an editable independent layer. No layered Aseprite/PSD/Blender source is included.
- No production texture import, sprite registry update, or W3 runtime replacement has been performed.

## Generation notes

The six-state pose boards were created with the built-in ImageGen tool using the approved concept turnarounds as design references. The prompt set requested transparent-background contact sheets, consistent character design and foot placement, and separate front/back/side rows; Naidun included both side directions. State-specific prompt clauses described breathing, locomotion, telegraph crouch, melee action, hit recoil, or a compact defeat slump. Some outputs need relayout and edge cleanup, so these prompts and images are treated as exploration sources rather than final production exports.

For exact dimensions, hashes, generation status, and review warnings, see [`P4_W3_EnemyAnimation_manifest_v01.json`](../../../../Manifests/P4_W3_EnemyAnimation_manifest_v01.json) and [`P4_W3_EnemyAnimationReview_v01.html`](../../../../../../Records/p4_alpha/evidence/W3/P4_W3_EnemyAnimationReview_v01.html).

For the approved concept-derived **static preview only**, see [`RuntimePreview/`](../RuntimePreview/) and [`P4_W3_StaticPreviewReview_v01.html`](../../../../../../Records/p4_alpha/evidence/W3/P4_W3_StaticPreviewReview_v01.html). That preview does not make these pose boards production-ready.
