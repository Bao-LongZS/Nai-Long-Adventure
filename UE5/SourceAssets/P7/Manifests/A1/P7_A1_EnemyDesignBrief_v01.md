# P7 A1 Enemy Silhouette Candidates v01

Date: 2026-11-01 (Asia/Shanghai)
Status: two original first-pose enemy candidates prepared for visual review; neither is approved for final production or UE import.

## Design basis

P7 Alpha calls for three readable enemy roles: normal, fast/low-health, and interferer. The existing user-approved `Enemies/A0/P7_A0_Enemy_Jammer_v01.png` remains the interferer style/sample reference and is unchanged. These two new concepts extend its bold-outline, rounded, cel-shaded presentation while giving each role a different silhouette that should remain distinguishable in grayscale:

- **Normal**: stocky, broad body, low stance, short feet; warm coral with cream belly and a small cyan accent.
- **Fast**: slim body, forward-leaning running pose, long separated limbs and swept-back fins; cyan/blue-violet with a small warm accent.

Both use a clean dry finish. They contain no tower base, weapon, liquid, cream drips, slime, wet gloss, text, logo, or watermark.

## Files

| AssetId | Source | Size | Alpha | Current status |
|---|---|---:|---|---|
| `P7-ENM-A1-NORMAL` | `P7_A1_Enemy_Normal_v01.png` | 1254×1254 | RGBA; transparent pixels and antialiased partial alpha verified | Candidate; user review pending |
| `P7-ENM-A1-FAST` | `P7_A1_Enemy_Fast_v01.png` | 1254×1254 | RGBA; transparent pixels and antialiased partial alpha verified | Candidate; user review pending |

Hashes and intended Unreal destinations are recorded in `P7_A1_EnemyAssets_v01.csv`. Originals are generated with the built-in ImageGen workflow and stored as lossless PNGs. The ordinary concept needed a background-extraction pass because the first render had a checkerboard baked into RGB; that first render was not added to the project.

## Remaining Alpha art work

These are first-pose silhouettes only. The Alpha requirement remains three enemy classes × three animation frames, so each role needs two additional motion frames after the base designs are approved. Still outstanding: second destructible obstacle family and damage states; approximately 14 route/ground tile elements and two fork-gate states; milk jar and currency icon; the remaining HUD icons; and four 4-frame VFX groups. Before bulk expansion, review these enemy silhouettes at 48 px and in grayscale, then confirm palette/shape contrast against the jammer and the three tower families. UE import, in-game scale, frame bounds, and 1080p/720p readability remain pending engineering/TA integration.

## Prompt records

The built-in ImageGen workflow was used. The ordinary image was created with the first prompt, then the second prompt removed a checkerboard that had been baked into its RGB background. The fast image used the third prompt.

### Ordinary concept generation

> Use case: stylized-concept  
> Asset type: P7 tower-defense enemy character sprite, first Alpha production candidate (ordinary enemy)  
> Input images: Image 1 is the user-approved P7 jammer enemy, for visual style only; do not copy its silhouette, colors, lightning mark, or expression.  
> Subject: one original ordinary road enemy for a lighthearted family-friendly milk-themed game. Make it a short, broad, sturdy little creature with a low center of gravity, two clearly visible short feet, compact arms, and a simple readable face looking toward the right as if walking along a path. Its silhouette should communicate slow and steady through mass and posture. Use a warm coral body with cream belly and a small cyan accent, dark clean outline, appealing but slightly mischievous expression. Keep all limbs attached and unobscured.  
> Style/medium: polished 2D game illustration with the same rounded, bold-outlined, soft cel-shaded finish and level of detail as the reference; clean solid forms, matte dry surfaces, restrained highlights.  
> Composition/framing: single full-body character centered, three-quarter side view facing right, large enough for a small gameplay sprite, generous transparent padding all around. No floor, shadow, platform, pedestal, scene, or extra objects.  
> Constraints: genuinely transparent background; exactly one character; no text, numbers, logo, watermark, tower base, weapon, liquid, slime, cream drips, wet gloss, photo realism, or copied character design.

### Ordinary background extraction

> Use case: background-extraction  
> Asset type: transparent-background game sprite cutout  
> Input image: Image 1 is the edit target. Preserve the character exactly: same coral short broad body, cream belly, cyan accent, face, limbs, pose, outline, colors, shading, scale, and placement. Change only the background.  
> Primary request: remove the checkerboard pattern completely and make every pixel outside the character fully transparent. Keep the complete outer contour clean with no fringe or checker pixels. Keep the character pixels unchanged as closely as possible.  
> Constraints: export with a genuine alpha channel; transparent background, not a checkerboard illustration, not white or black; no crop, no redraw, no extra shadow, no new objects, no text, no watermark.

### Fast concept

> Use case: stylized-concept  
> Asset type: P7 tower-defense enemy character sprite, first Alpha production candidate (fast enemy)  
> Input images: Image 1 is the user-approved P7 jammer, a style reference only. Image 2 is the new ordinary enemy concept, a line-weight and shading reference only. Do not copy either silhouette, face, markings, or colors.  
> Subject: one original fast, low-health road enemy for a lighthearted family-friendly milk-themed game. Design a slim, low-mass, long-legged creature leaning forward into a run, with a narrow streamlined body, two long readable legs and small swept-back ear-like fins. Give it a simple alert face looking toward the right. Use a cool cyan/blue-violet body with a small warm yellow accent and cream muzzle. Its thin forward-leaning silhouette must remain distinct from the short, broad ordinary enemy even in grayscale. No lightning bolt mark.  
> Style/medium: polished 2D game illustration matching the rounded, bold dark outline, soft cel-shaded finish, clean solid forms and restrained highlights of the references; dry matte surfaces.  
> Composition/framing: single full-body character centered, three-quarter side view facing right, dynamic running pose but all limbs clearly separated, gameplay sprite scale, generous transparent padding. No ground, shadow, platform, pedestal, scene or extra objects.  
> Constraints: genuine transparent background; exactly one character; no text, numbers, logos, watermark, tower base, weapon, liquid, slime, cream drips, wet gloss, photo realism, or copied character design.
