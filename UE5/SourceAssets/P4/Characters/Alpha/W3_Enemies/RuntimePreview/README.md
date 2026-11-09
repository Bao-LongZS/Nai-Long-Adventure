# W3 Static Enemy Preview v01

Status: **two static front-view preview PNGs imported; texture settings read back; runtime review pending**. These crops were derived from the approved concept turnarounds to check visible enemy scale and contrast in the P4 art-integration map. They are not animation frames and are not production-approved.

## Files

- `naiding_idle_front_static_preview_v01.png` — 512×512 RGBA, Naiding front view, foot anchor `(256,466)`, static canvas height 215 uu in preview.
- `naidun_idle_front_static_preview_v01.png` — 512×512 RGBA, Naidun front view, foot anchor `(256,466)`, static canvas height 309 uu in preview.
- `P4_W3_StaticPreview_manifest_v01.json` — source crops, hashes, import paths, target scale, and approval scope.
- `P4_W3_StaticPreviewReview_v01.html` — side-by-side source review.

The preview renderer is wired to substitute art only for wave-spawned Naiding/Naidun entities. It keeps training enemies and projectiles as grayboxes and falls back to grayboxes if either imported texture or the runtime material is missing. The full editor target and in-map appearance still need verification. All enemies display the front pose; this does not validate facing or animation.

Naiding's extracted concept crop retains small edge artifacts. Both previews still need runtime composition review, and Naidun's shield is baked into the image rather than supplied as a separate layer. Continue using the six-state pose boards for animation planning; do not import those grids as production sprite sheets.

## UE texture correction (2026-10-30)

Both existing Texture2D assets were reused from the same PNG sources and now read back as `TC_EditorIcon` uncompressed RGBA8, sRGB, Character group, Sharpen0 mips, and Never Stream. The translucent sprite material preserves the straight-alpha antialiased edge. The corrected readback is in `UE5/Records/p4_alpha/evidence/W3/P4_W3_StaticPreview_import_report_v02.json`; this still does not approve animation or production use.
