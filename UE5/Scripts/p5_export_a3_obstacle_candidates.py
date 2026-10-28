from __future__ import annotations

import json
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
P5_ROOT = ROOT / "UE5" / "SourceAssets" / "P5"
SOURCE_DIR = P5_ROOT / "Environment" / "Source"
EXPORT_DIR = P5_ROOT / "Environment" / "Exports"
MANIFEST_PATH = P5_ROOT / "Manifests" / "P5_A3_Obstacles_manifest_v01.json"
EVIDENCE_DIR = ROOT / "UE5" / "Records" / "p5_p6_plan" / "evidence" / "P5_A3_Obstacles"
PREVIEW_PATH = EVIDENCE_DIR / "P5_A3_Obstacles_preview_v01.png"


SHEETS = [
    {
        "source": "O_Tree_Sheet_Source_v01.png",
        "sourceImageGenId": "exec-fa4b6800-c80c-4adc-a415-eb7a8dd6c62a",
        "cellSize": (512, 1024),
        "targetSize": (512, 1024),
        "assetIds": ["O_Tree_A", "O_Tree_B", "O_Tree_C"],
    },
    {
        "source": "O_Rock_Sheet_Source_v01.png",
        "sourceImageGenId": "exec-db16114d-a4b8-4670-b3b5-cdaac3add5e4",
        "cellSize": (887, 887),
        "targetSize": (256, 256),
        "assetIds": ["O_Rock_A", "O_Rock_B"],
    },
]


def alpha_record(image: Image.Image) -> dict[str, object]:
    rgba = image.convert("RGBA")
    alpha = rgba.getchannel("A")
    histogram = alpha.histogram()
    total = rgba.width * rgba.height
    return {
        "dimensions": list(rgba.size),
        "alphaBounds": list(alpha.getbbox()) if alpha.getbbox() else None,
        "transparentPixelPercent": round(histogram[0] * 100 / total, 2),
        "opaquePixelPercent": round(sum(histogram[250:]) * 100 / total, 2),
    }


def make_preview(files: list[Path]) -> None:
    canvas = Image.new("RGBA", (1536, 1120), (115, 137, 161, 255))
    draw = ImageDraw.Draw(canvas)
    y = 0
    for index, path in enumerate(files):
        with Image.open(path) as opened:
            sprite = opened.convert("RGBA")
        if index < 3:
            sprite.thumbnail((360, 850), Image.Resampling.LANCZOS)
            x = index * 512 + (512 - sprite.width) // 2
            canvas.alpha_composite(sprite, (x, y + 38 + 850 - sprite.height))
            draw.text((index * 512 + 10, y + 8), f"O_Tree_{chr(65 + index)}", fill=(250, 250, 248, 255))
        else:
            sprite.thumbnail((260, 260), Image.Resampling.LANCZOS)
            x = (index - 3) * 768 + (768 - sprite.width) // 2
            canvas.alpha_composite(sprite, (x, 930 + (190 - sprite.height) // 2))
            draw.text(((index - 3) * 768 + 10, 908), f"O_Rock_{chr(65 + index - 3)}", fill=(250, 250, 248, 255))
    canvas.convert("RGB").save(PREVIEW_PATH, format="PNG", optimize=True)


def main() -> None:
    EXPORT_DIR.mkdir(parents=True, exist_ok=True)
    EVIDENCE_DIR.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    records: list[dict[str, object]] = []
    preview_files: list[Path] = []

    for sheet in SHEETS:
        source_path = SOURCE_DIR / sheet["source"]
        with Image.open(source_path) as opened:
            source = opened.convert("RGBA")
        cell_width, cell_height = sheet["cellSize"]
        target = sheet["targetSize"]
        for index, asset_id in enumerate(sheet["assetIds"]):
            crop = source.crop((index * cell_width, 0, (index + 1) * cell_width, cell_height))
            exported = crop.resize(target, Image.Resampling.LANCZOS)
            output_file = f"{asset_id}_v01_WIP.png"
            output_path = EXPORT_DIR / output_file
            exported.save(output_path, format="PNG", optimize=True)
            preview_files.append(output_path)
            records.append({
                "assetId": asset_id,
                "sourceFile": str(source_path.relative_to(ROOT)).replace("\\", "/"),
                "sourceImageGenId": sheet["sourceImageGenId"],
                "sourceDimensions": list(source.size),
                "sourceCell": [index * cell_width, 0, (index + 1) * cell_width, cell_height],
                "exportFile": str(output_path.relative_to(ROOT)).replace("\\", "/"),
                "format": "PNG RGBA, sRGB assumed",
                **alpha_record(exported),
                "manualSilhouetteAssessment": "distinct_static_candidate_collision_match_pending",
            })

    make_preview(preview_files)
    manifest = {
        "manifestId": "P5_A3_Obstacles",
        "version": "v01",
        "created": "2026-10-28",
        "status": "WIP; candidates only; not accepted for runtime import",
        "approvedDirection": "P5 A0 natural-tone v02 scene",
        "exportTool": "UE5/Scripts/p5_export_a3_obstacle_candidates.py",
        "assets": records,
        "preview": str(PREVIEW_PATH.relative_to(ROOT)).replace("\\", "/"),
        "gates": {
            "dimensions": "exported to specification",
            "transparency": "alpha retained; UE material filtering and edge review pending",
            "silhouette": "three tree variants and two rock variants are visually distinct in a neutral composite preview",
            "collision": "pending actual P5 collision bounds and gameplay readability review",
            "editorIntegration": "pending",
        },
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Exported {len(records)} WIP assets.")
    for record in records:
        print(f"{record['assetId']}: {record['dimensions']} alpha={record['alphaBounds']}")
    print(f"Preview: {PREVIEW_PATH}")
    print(f"Manifest: {MANIFEST_PATH}")


if __name__ == "__main__":
    main()
