from __future__ import annotations

import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[2]
P5_ROOT = ROOT / "UE5" / "SourceAssets" / "P5"
SOURCE_PATH = P5_ROOT / "Characters" / "Source" / "C_Naiwa_Ski_ApprovedSixPose_Source_v01.png"
EXPORT_DIR = P5_ROOT / "Characters" / "Exports"
MANIFEST_PATH = P5_ROOT / "Manifests" / "P5_A4_NaiwaSki_manifest_v01.json"
EVIDENCE_DIR = ROOT / "UE5" / "Records" / "p5_p6_plan" / "evidence" / "P5_A4_Character"
ATLAS_PATH = EXPORT_DIR / "C_Naiwa_Ski_v01_WIP.png"
PREVIEW_PATH = EVIDENCE_DIR / "C_Naiwa_Ski_AtlasPreview_v01.png"


POSES = [
    "Glide",
    "Preload",
    "AirTuck",
    "AirTurn",
    "Land",
    "Fall",
]

def pose_components(image: Image.Image, threshold: int = 32) -> list[tuple[int, tuple[int, int, int, int], bytearray]]:
    """Return sizeable alpha-connected figures in reading order, with binary masks."""
    width, height = image.size
    alpha_bytes = image.getchannel("A").tobytes()
    active = bytearray(1 if value >= threshold else 0 for value in alpha_bytes)
    seen = bytearray(width * height)
    components = []

    for start in range(width * height):
        if not active[start] or seen[start]:
            continue
        stack = [start]
        seen[start] = 1
        component = bytearray(width * height)
        min_x = max_x = start % width
        min_y = max_y = start // width
        count = 0
        while stack:
            pos = stack.pop()
            x, y = pos % width, pos // width
            component[pos] = 255
            count += 1
            min_x, max_x = min(min_x, x), max(max_x, x)
            min_y, max_y = min(min_y, y), max(max_y, y)
            for ny in range(max(0, y - 1), min(height, y + 2)):
                row = ny * width
                for nx in range(max(0, x - 1), min(width, x + 2)):
                    neighbor = row + nx
                    if active[neighbor] and not seen[neighbor]:
                        seen[neighbor] = 1
                        stack.append(neighbor)
        if count >= 1000:
            components.append((count, (min_x, min_y, max_x + 1, max_y + 1), component))

    top = sorted((item for item in components if item[1][1] < height // 2), key=lambda item: item[1][0])
    bottom = sorted((item for item in components if item[1][1] >= height // 2), key=lambda item: item[1][0])
    ordered = top + bottom
    if len(ordered) != len(POSES):
        raise ValueError(f"Expected {len(POSES)} isolated alpha components, found {len(ordered)}")
    return ordered


def main() -> None:
    EXPORT_DIR.mkdir(parents=True, exist_ok=True)
    EVIDENCE_DIR.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    with Image.open(SOURCE_PATH) as opened:
        source = opened.convert("RGBA")
    if source.size != (1536, 1024):
        raise ValueError(f"Expected approved 3x2 source sheet at 1536x1024; got {source.size}")

    atlas = Image.new("RGBA", (2048, 1024), (0, 0, 0, 0))
    frames = []
    preview = Image.new("RGBA", (2048, 1024), (115, 137, 161, 255))
    draw = ImageDraw.Draw(preview)
    x_edges = [round(i * 2048 / 3) for i in range(4)]
    anchor_y = 476
    components = pose_components(source)

    for index, (pose, component_data) in enumerate(zip(POSES, components)):
        column, row = index % 3, index // 3
        _pixel_count, bbox, component_bytes = component_data
        core = Image.frombytes("L", source.size, bytes(component_bytes))
        # Retain the soft antialiased edge just outside the thresholded core,
        # while excluding pixels belonging to adjacent poses.
        allowed = core.filter(ImageFilter.MaxFilter(7))
        crop_box = (
            max(0, bbox[0] - 3),
            max(0, bbox[1] - 3),
            min(source.width, bbox[2] + 3),
            min(source.height, bbox[3] + 3),
        )
        sprite = source.crop(crop_box)
        source_alpha = sprite.getchannel("A")
        allowed_crop = allowed.crop(crop_box)
        sprite.putalpha(Image.frombytes(
            "L", sprite.size,
            bytes((a if m else 0) for a, m in zip(source_alpha.tobytes(), allowed_crop.tobytes())),
        ))
        sprite.thumbnail((650, 460), Image.Resampling.LANCZOS)
        frame_x0, frame_x1 = x_edges[column], x_edges[column + 1]
        cell_width = frame_x1 - frame_x0
        paste_x = frame_x0 + (cell_width - sprite.width) // 2
        paste_y = row * 512 + anchor_y - sprite.height
        atlas.alpha_composite(sprite, (paste_x, paste_y))
        preview.alpha_composite(sprite, (paste_x, paste_y))
        draw.rectangle((frame_x0, row * 512, frame_x1 - 1, (row + 1) * 512 - 1), outline=(213, 223, 232, 220), width=2)
        draw.text((frame_x0 + 8, row * 512 + 8), f"{index + 1} {pose}", fill=(250, 250, 248, 255))
        anchor = [paste_x - frame_x0 + sprite.width // 2, anchor_y]
        frames.append({
            "index": index,
            "pose": pose,
            "sourceComponentBoundsThreshold32": list(bbox),
            "sourceCropWithEdgePadding3": list(crop_box),
            "atlasRect": [frame_x0, row * 512, frame_x1, (row + 1) * 512],
            "spriteRectInAtlas": [paste_x, paste_y, paste_x + sprite.width, paste_y + sprite.height],
            "anchor": anchor,
            "anchorMethod": "provisional bottom-center of cropped pose; align sprite board/feet to y=476",
        })

    atlas.save(ATLAS_PATH, format="PNG", optimize=True)
    preview.convert("RGB").save(PREVIEW_PATH, format="PNG", optimize=True)
    alpha = atlas.getchannel("A")
    manifest = {
        "manifestId": "P5_A4_NaiwaSki",
        "version": "v01",
        "created": "2026-10-28",
        "status": "WIP; source image explicitly approved by project owner; atlas and anchors are candidates",
        "sourceFile": str(SOURCE_PATH.relative_to(ROOT)).replace("\\", "/"),
        "sourceImageGenId": "exec-b30a4f3e-29ff-4f95-ab88-70e4e20a3174",
        "exportTool": "UE5/Scripts/p5_export_a4_character_candidate.py",
        "exportFile": str(ATLAS_PATH.relative_to(ROOT)).replace("\\", "/"),
        "exportDimensions": [2048, 1024],
        "format": "PNG RGBA atlas, sRGB assumed",
        "alpha": "preserved",
        "atlasGrid": {"columns": 3, "rows": 2, "frameCellWidths": [683, 682, 683], "frameCellHeight": 512},
        "frameCount": len(frames),
        "loopMode": "pose-select; only Glide is a loop candidate; other poses are one-shot/hold",
        "frames": frames,
        "atlasAlphaBounds": list(alpha.getbbox()) if alpha.getbbox() else None,
        "preview": str(PREVIEW_PATH.relative_to(ROOT)).replace("\\", "/"),
        "pending": [
            "editor review of per-pose foot/board anchors",
            "1080p and 720p readability and alpha filtering review in the real P5 scene",
            "match each pose to actual gameplay transitions and frame timing",
        ],
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Exported {len(frames)} WIP frames to {ATLAS_PATH}")
    print(f"Preview: {PREVIEW_PATH}")
    print(f"Manifest: {MANIFEST_PATH}")


if __name__ == "__main__":
    main()
