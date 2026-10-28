from __future__ import annotations

import json
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
ASSET_ROOT = ROOT / "UE5" / "SourceAssets" / "P5"
SOURCE_DIR = ASSET_ROOT / "Backgrounds" / "Source"
EXPORT_DIR = ASSET_ROOT / "Backgrounds" / "Exports"
MANIFEST_PATH = ASSET_ROOT / "Manifests" / "P5_A1_Backgrounds_manifest_v01.json"
EVIDENCE_DIR = ROOT / "UE5" / "Records" / "p5_p6_plan" / "evidence" / "P5_A1_Backgrounds"
PREVIEW_PATH = EVIDENCE_DIR / "P5_A1_Backgrounds_tiling_preview_v01.png"


ASSETS = [
    {
        "assetId": "BG_Mountain_Far",
        "source": "BG_Mountain_Far_Source_v01.png",
        "export": "BG_Mountain_Far_v01_WIP.png",
        "target": (2048, 512),
        "cropTop": 75,
        "sourceImageGenId": "exec-7d0ae228-bd4c-43b3-8ea5-9dffdeae5e3a",
        "manualSeamAssessment": "provisional_low_edge_difference_review_in_editor_pending",
    },
    {
        "assetId": "BG_Forest_Mid",
        "source": "BG_Forest_Mid_Source_v01.png",
        "export": "BG_Forest_Mid_v01_WIP.png",
        "target": (2048, 512),
        "cropTop": 80,
        "sourceImageGenId": "exec-1321daf3-0013-475a-8b2e-c90efbeabd49",
        "manualSeamAssessment": "provisional_low_edge_difference_review_in_editor_pending",
    },
    {
        "assetId": "BG_Ridge_Near",
        "source": "BG_Ridge_Near_Source_v01.png",
        "export": "BG_Ridge_Near_v01_WIP.png",
        "target": (2048, 768),
        "cropLeft": 24,
        "sourceImageGenId": "exec-8118d428-fcb5-4d04-a1ce-a0df3ba76e4f",
        "manualSeamAssessment": "fail_visible_ridge_height_mismatch",
    },
]


def edge_metrics(image: Image.Image) -> dict[str, object]:
    rgba = image.convert("RGBA")
    width, height = rgba.size
    left = rgba.crop((0, 0, 1, height))
    right = rgba.crop((width - 1, 0, width, height))
    # Mean absolute RGBA difference between the first and last columns.
    left_pixels = [left.getpixel((0, y)) for y in range(height)]
    right_pixels = [right.getpixel((0, y)) for y in range(height)]
    per_channel = [
        round(sum(abs(a[channel] - b[channel]) for a, b in zip(left_pixels, right_pixels)) / height, 3)
        for channel in range(4)
    ]
    alpha = rgba.getchannel("A")
    return {
        "alphaBounds": list(alpha.getbbox()) if alpha.getbbox() else None,
        "leftRightMeanAbsDiffRGBA": per_channel,
        "leftEdgeOpaquePixels": sum(1 for px in left_pixels if px[3] > 250),
        "rightEdgeOpaquePixels": sum(1 for px in right_pixels if px[3] > 250),
    }


def export_asset(spec: dict[str, object]) -> dict[str, object]:
    src_path = SOURCE_DIR / str(spec["source"])
    out_path = EXPORT_DIR / str(spec["export"])
    with Image.open(src_path) as opened:
        source = opened.convert("RGBA")
    source_width, source_height = source.size
    target_width, target_height = spec["target"]  # type: ignore[misc]

    if "cropTop" in spec:
        crop_height = round(source_width * target_height / target_width)
        top = int(spec["cropTop"])
        if top < 0 or top + crop_height > source_height:
            raise ValueError(f"Vertical crop is outside {src_path.name}")
        source = source.crop((0, top, source_width, top + crop_height))
    elif "cropLeft" in spec:
        crop_width = round(source_height * target_width / target_height)
        left = int(spec["cropLeft"])
        if left < 0 or left + crop_width > source_width:
            raise ValueError(f"Horizontal crop is outside {src_path.name}")
        source = source.crop((left, 0, left + crop_width, source_height))

    exported = source.resize((target_width, target_height), Image.Resampling.LANCZOS)
    exported.save(out_path, format="PNG", optimize=True)
    return {
        "assetId": spec["assetId"],
        "sourceFile": str(src_path.relative_to(ROOT)).replace("\\", "/"),
        "sourceImageGenId": spec["sourceImageGenId"],
        "sourceDimensions": [source_width, source_height],
        "exportFile": str(out_path.relative_to(ROOT)).replace("\\", "/"),
        "exportDimensions": [target_width, target_height],
        "format": "PNG RGBA, sRGB assumed",
        "alpha": "preserved",
        "repeatEdgeMetrics": edge_metrics(exported),
        "manualSeamAssessment": spec["manualSeamAssessment"],
    }


def make_tiling_preview(export_records: list[dict[str, object]]) -> None:
    background = (115, 137, 161, 255)
    row_heights = [300, 300, 430]
    preview = Image.new("RGBA", (2048, sum(row_heights)), background)
    draw = ImageDraw.Draw(preview)
    y = 0
    for record, row_height in zip(export_records, row_heights):
        tile_path = ROOT / str(record["exportFile"])
        with Image.open(tile_path) as opened:
            tile = opened.convert("RGBA")
        tile.thumbnail((1024, row_height - 28), Image.Resampling.LANCZOS)
        label_y = y + 6
        tile_y = y + row_height - tile.height
        for repeat in range(2):
            preview.alpha_composite(tile, (repeat * 1024, tile_y))
        draw.text((12, label_y), str(record["assetId"]), fill=(250, 250, 248, 255))
        y += row_height
    preview.convert("RGB").save(PREVIEW_PATH, format="PNG", optimize=True)


def main() -> None:
    EXPORT_DIR.mkdir(parents=True, exist_ok=True)
    EVIDENCE_DIR.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    records = [export_asset(spec) for spec in ASSETS]
    make_tiling_preview(records)
    manifest = {
        "manifestId": "P5_A1_Backgrounds",
        "version": "v01",
        "created": "2026-10-28",
        "status": "WIP; candidates only; not accepted for runtime import",
        "approvedDirection": "P5 A0 natural-tone v02 scene; original character first draft remains frozen",
        "exportTool": "UE5/Scripts/p5_export_a1_background_candidates.py",
        "assets": records,
        "preview": str(PREVIEW_PATH.relative_to(ROOT)).replace("\\", "/"),
        "gates": {
            "dimensions": "exported to specification",
            "transparency": "preserved from generated sources; visual edge review required in the actual material",
            "horizontalRepeat": "near ridge fails; far and mid remain provisional pending Editor loop review",
            "editorIntegration": "pending",
            "A0A1SceneGate": "pending real UE Editor fixture, route geometry and HUD review",
            "performance": "not measured",
        },
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Exported {len(records)} WIP assets.")
    for record in records:
        print(f"{record['assetId']}: {record['exportDimensions']} {record['repeatEdgeMetrics']['leftRightMeanAbsDiffRGBA']}")
    print(f"Preview: {PREVIEW_PATH}")
    print(f"Manifest: {MANIFEST_PATH}")


if __name__ == "__main__":
    main()
