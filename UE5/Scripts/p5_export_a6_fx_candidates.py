from __future__ import annotations

import json
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
P5_ROOT = ROOT / "UE5" / "SourceAssets" / "P5"
SOURCE_DIR = P5_ROOT / "FX" / "Source"
EXPORT_DIR = P5_ROOT / "FX" / "Exports"
MANIFEST_PATH = P5_ROOT / "Manifests" / "P5_A6_FX_manifest_v01.json"
EVIDENCE_DIR = ROOT / "UE5" / "Records" / "p5_p6_plan" / "evidence" / "P5_A6_FX"
PREVIEW_PATH = EVIDENCE_DIR / "P5_A6_FX_preview_v01.png"
LANDPUFF_PREVIEW_PATH = EVIDENCE_DIR / "P5_A6_LandPuffAtlas_preview_v01.png"


ASSETS = [
    ("FX_Snow_Mist", "FX_Snow_Mist_Source_v01.png", "exec-8e5f0e72-8773-4825-820a-639730492f67", (1024, 256), 181),
    ("FX_Avalanche_Cloud", "FX_Avalanche_Cloud_Source_v01.png", "exec-26793815-7c8d-4076-be1e-29c10f833257", (512, 512), None),
    ("FX_Avalanche_Rim", "FX_Avalanche_Rim_Source_v01.png", "exec-27155010-7e87-45de-a7f0-f28bc7219dd1", (512, 256), None),
    ("FX_Shadow_Soft", "FX_Shadow_Soft_Source_v01.png", "exec-da9df0c2-8dd1-43df-9660-4052cc9803e0", (256, 128), None),
    ("FX_Snowflake", "FX_Snowflake_Source_v01.png", "exec-7c7f0130-56a1-4544-b65b-d598430d4bb7", (64, 64), None),
]


def alpha_metrics(image: Image.Image) -> dict[str, object]:
    rgba = image.convert("RGBA")
    alpha = rgba.getchannel("A")
    hist = alpha.histogram()
    total = rgba.width * rgba.height
    return {
        "alphaBounds": list(alpha.getbbox()) if alpha.getbbox() else None,
        "transparentPixelPercent": round(hist[0] * 100 / total, 2),
        "opaquePixelPercent": round(sum(hist[250:]) * 100 / total, 2),
    }


def export_assets() -> list[dict[str, object]]:
    records = []
    for asset_id, source_name, image_gen_id, target, crop_top in ASSETS:
        source_path = SOURCE_DIR / source_name
        with Image.open(source_path) as opened:
            source = opened.convert("RGBA")
        source_size = source.size
        if crop_top is not None:
            crop_height = round(source.width * target[1] / target[0])
            source = source.crop((0, crop_top, source.width, crop_top + crop_height))
        exported = source.resize(target, Image.Resampling.LANCZOS)
        output_name = f"{asset_id}_v01_WIP.png"
        output_path = EXPORT_DIR / output_name
        exported.save(output_path, format="PNG", optimize=True)
        records.append({
            "assetId": asset_id,
            "sourceFile": str(source_path.relative_to(ROOT)).replace("\\", "/"),
            "sourceImageGenId": image_gen_id,
            "sourceDimensions": list(source_size),
            "cropTop": crop_top,
            "exportFile": str(output_path.relative_to(ROOT)).replace("\\", "/"),
            "exportDimensions": list(target),
            "format": "PNG RGBA, sRGB assumed",
            "alpha": "preserved",
            **alpha_metrics(exported),
        })
    return records


def make_preview(records: list[dict[str, object]]) -> None:
    canvas = Image.new("RGBA", (1536, 940), (115, 137, 161, 255))
    draw = ImageDraw.Draw(canvas)
    layout = [
        (20, 40, 700, 175),
        (785, 35, 440, 440),
        (20, 275, 700, 350),
        (785, 535, 330, 165),
        (1180, 535, 180, 180),
    ]
    for record, (x, y, max_width, max_height) in zip(records, layout):
        with Image.open(ROOT / str(record["exportFile"])) as opened:
            sprite = opened.convert("RGBA")
        sprite.thumbnail((max_width, max_height), Image.Resampling.LANCZOS)
        canvas.alpha_composite(sprite, (x, y + max_height - sprite.height))
        draw.text((x, y - 20), str(record["assetId"]), fill=(250, 250, 248, 255))
    canvas.convert("RGB").save(PREVIEW_PATH, format="PNG", optimize=True)


def export_land_puff() -> dict[str, object]:
    asset_id = "FX_LandPuff"
    source_path = SOURCE_DIR / "FX_LandPuff_Sheet_Source_v01.png"
    output_path = EXPORT_DIR / "FX_LandPuff_v01_WIP.png"
    with Image.open(source_path) as opened:
        source = opened.convert("RGBA")
    columns, rows = 4, 3
    cell_width, cell_height = 128, 128
    atlas = Image.new("RGBA", (columns * cell_width, rows * cell_height), (0, 0, 0, 0))
    frames = []
    anchor = (64, 120)
    for row in range(rows):
        for column in range(columns):
            index = row * columns + column
            x0 = round(column * source.width / columns)
            x1 = round((column + 1) * source.width / columns)
            y0 = round(row * source.height / rows)
            y1 = round((row + 1) * source.height / rows)
            cell = source.crop((x0, y0, x1, y1))
            bbox = cell.getchannel("A").getbbox()
            if bbox is None:
                raise ValueError(f"FX_LandPuff source frame {index} is empty")
            sprite = cell.crop(bbox)
            sprite.thumbnail((112, 100), Image.Resampling.LANCZOS)
            dx = column * cell_width + (cell_width - sprite.width) // 2
            dy = row * cell_height + anchor[1] - sprite.height
            atlas.alpha_composite(sprite, (dx, dy))
            frames.append({
                "index": index,
                "sourceCell": [x0, y0, x1, y1],
                "sourceAlphaBounds": list(bbox),
                "atlasRect": [column * cell_width, row * cell_height, (column + 1) * cell_width, (row + 1) * cell_height],
                "anchor": list(anchor),
                "order": "row-major; one-shot candidate",
            })
    atlas.save(output_path, format="PNG", optimize=True)
    preview = Image.new("RGBA", (1024, 1080), (115, 137, 161, 255))
    preview.alpha_composite(atlas.resize((1024, 1024), Image.Resampling.NEAREST), (0, 0))
    ImageDraw.Draw(preview).text((8, 1040), "FX_LandPuff 4x3 atlas / 12 row-major frames", fill=(250, 250, 248, 255))
    preview.convert("RGB").save(LANDPUFF_PREVIEW_PATH, format="PNG", optimize=True)
    return {
        "assetId": asset_id,
        "sourceFile": str(source_path.relative_to(ROOT)).replace("\\", "/"),
        "sourceImageGenId": "exec-0b40633f-dcef-41d2-9161-87998ca60daa",
        "sourceDimensions": list(source.size),
        "sourceGrid": {"columns": columns, "rows": rows},
        "exportFile": str(output_path.relative_to(ROOT)).replace("\\", "/"),
        "exportDimensions": [512, 512],
        "format": "PNG RGBA atlas, sRGB assumed",
        "alpha": "preserved",
        "frameCount": len(frames),
        "frameSize": [cell_width, cell_height],
        "frames": frames,
        "preview": str(LANDPUFF_PREVIEW_PATH.relative_to(ROOT)).replace("\\", "/"),
        "reviewStatus": "WIP; per-frame timing and in-engine playback pending",
    }


def main() -> None:
    EXPORT_DIR.mkdir(parents=True, exist_ok=True)
    EVIDENCE_DIR.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    records = export_assets()
    land_puff = export_land_puff()
    make_preview(records)
    records.append(land_puff)
    manifest = {
        "manifestId": "P5_A6_FX",
        "version": "v01",
        "created": "2026-10-28",
        "status": "WIP; six visual candidates only; not accepted for runtime import",
        "approvedDirection": "P5 A0 natural-tone v02 scene",
        "exportTool": "UE5/Scripts/p5_export_a6_fx_candidates.py",
        "assets": records,
        "preview": str(PREVIEW_PATH.relative_to(ROOT)).replace("\\", "/"),
        "pending": [
            "UE material filtering, premultiplied alpha edge review, and intended layer-opacity tuning",
            "Avalanche/character/obstacle scene overlap and 720p readability",
            "FX_LandPuff per-frame timing and in-engine playback",
        ],
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Exported {len(records)} WIP assets, including {land_puff['frameCount']} land-puff frames.")
    for record in records:
        print(f"{record['assetId']}: {record['exportDimensions']}")
    print(f"Preview: {PREVIEW_PATH}")
    print(f"Manifest: {MANIFEST_PATH}")


if __name__ == "__main__":
    main()
