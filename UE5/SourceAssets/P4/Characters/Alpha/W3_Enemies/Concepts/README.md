# P4 W3 Enemy Concepts v01

Status: **concept direction approved by the user on 2026-10-29.** These turnaround images are not runtime sprites. Six-state pose-board exploration is in progress; W3 graybox assets and gameplay data remain authoritative until production sprites are cleaned, reviewed, and integrated.

## Scope

This first review covers only the two Chapter 1 W3 enemies requested in `P4_敌人美术交接说明_2026-10-29.md`:

- **Naiding (`naiding`)** — small, quick pursuer; pale mint/cyan body, coral accents, swept-back milk-swirl crest, and no frog belly or green eye rings. Three views: front, back, right-facing side.
- **Naidun (`naidun`)** — broad, slow guard; deep blue-teal body, slate shell, pale-rimmed terracotta shield. Four views: front, back, right-facing side, and left-facing side. Both side views are included because the shield must remain on the forward side when facing either direction.

## Files

- `P4_W3_naiding_turnaround_v01_concept.png` — 2172×724 RGBA, generated on transparent background.
- `P4_W3_naidun_turnaround_v01_concept.png` — 2172×724 RGBA, generated on transparent background.
- `P4_W3_EnemyConceptReview_v01.html` — comparison/review page with the approved player pose, proposed relative scale, palette, and silhouette notes.
- `P4_W3_EnemyConcept_manifest_v01.json` — IDs, view order, SHA-256, source references, generation prompts, and current approval state.

## Proposed relative scale (for review)

Use the approved P4 front idle pose as the 1.00 height reference. The initial proposal is Naiding at **0.78×** player height and Naidun at **1.12×** player height with a noticeably broader silhouette. These are art targets only; engineering should keep collision radii and gameplay values in the existing data tables.

## Review gates

The user approved both silhouettes and palette direction and asked art production to continue. Review the current six-state pose boards at `P4_W3_EnemyAnimationReview_v01.html`. Production art still needs `idle`, `move`, `telegraph`, `attack`, `hit`, and `death` source frames at 512×512 per frame, aligned foot anchors, clean alpha edges, and front/back/side coverage. Keep separate left/right side frames for Naidun because the shield direction must remain correct. Current boards need cleanup and frame export before UE import.

## Final prompts used

### Naiding

> Design an original enemy called Naiding, a small quick pursuer for a cute arena action game. Show exactly three full-body views of the same character in one horizontal row: front, back, and right-facing profile. Use neutral readable standing poses with a slight forward-ready lean. Make a compact, light, rounded milk-drop creature with a distinct silhouette from the player: small pear-drop body, two tiny springy feet, short fin-like side flaps, a swept-back milk-swirl crest, small dark eyes without green rings, and no large cream belly patch. Use cool pale teal/blue with a restrained coral accent; do not reproduce the player's yellow body, green eyes, cream belly, or brown hands and feet. Match the approved P4 soft 3D-shaded 2D sprite finish, crisp edges, warm soft key light, orthographic views, equal scale, aligned feet, and transparent background. No shadow, floor, text, scenery, weapon, shield, effects, watermark, crop, or extra character.

### Naidun

> Design an original enemy called Naidun, a slow armored guard for a cute arena action game. Show exactly four full-body views of the same character in one horizontal row: front facing the viewer, back, right-facing profile, and left-facing profile. Use a planted heavy stance. Make the body short, broad, deep blue-teal, and visibly more massive than the player, with a dark slate shell/back plates, small amber eyes near the top, and short sturdy feet. Do not reproduce the player's yellow body, green eyes, cream belly, or brown hands and feet. Give it one large separate rounded-trapezoid shield with a pale stone rim and restrained terracotta center. Keep the face visible above and beside the shield; show shell and straps in back view; in both side profiles the shield must face forward in the direction the character faces. Match the approved P4 soft 3D-shaded 2D sprite finish, crisp edges, orthographic views, equal scale, aligned feet, and transparent background. No shadow, floor, text, scenery, other weapon, effects, watermark, crop, or extra character.

Both concept sheets were generated with the built-in ImageGen tool using the approved P4 A1 front idle sprite for rendering style and the Common player reference for identity separation and relative proportion.
