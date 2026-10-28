from __future__ import annotations

import json
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
P5_ROOT = ROOT / "UE5" / "SourceAssets" / "P5"
SOURCE_DIR = P5_ROOT / "Materials" / "Source"
EXPORT_DIR = P5_ROOT / "Materials" / "Exports"
MANIFEST_PATH = P5_ROOT / "Manifests" / "P5_A2_SnowSurface_manifest_v01.json"
EVIDENCE_DIR = ROOT / "UE5" / "Records" / "p5_p6_plan" / "evidence" / "P5_A2_SnowSurface"
PREVIEW_PATH = EVIDENCE_DIR / "P5_A2_SnowSurface_preview_v01.png"


def edge_differences(image: Image.Image, axis: str) -> list[float]:
    rgba = image.convert("RGBA")
    width, height = rgba.size
    pairs = []
    if axis == "horizontal":
        pairs = [(rgba.getpixel((0, y)), rgba.getpixel((width - 1, y))) for y in range(height)]
    else:
        pairs = [(rgba.getpixel((x, 0)), rgba.getpixel((x, height - 1))) for x in range(width)]
    return [
        round(sum(abs(a[c] - b[c]) for a, b in pairs) / len(pairs), 3)
        for c in range(4)
    ]


def texture_record(
    asset_id: str,
    source_file: str,
    image_gen_id: str,
    output_file: str,
    target: tuple[int, int],
    edge_axes: tuple[str, ...],
    crop_box: tuple[int, int, int, int] | None = None,
) -> dict[str, object]:
    src = SOURCE_DIR / source_file
    dst = EXPORT_DIR / output_file
    with Image.open(src) as opened:
        source = opened.convert("RGBA")
    source_dimensions = list(source.size)
    prepared = source.crop(crop_box) if crop_box is not None else source
    exported = prepared.resize(target, Image.Resampling.LANCZOS)
    exported.save(dst, format="PNG", optimize=True)
    alpha = exported.getchannel("A")
    return {
        "assetId": asset_id,
        "sourceFile": str(src.relative_to(ROOT)).replace("\\", "/"),
        "sourceImageGenId": image_gen_id,
        "sourceDimensions": source_dimensions,
        "cropBox": list(crop_box) if crop_box is not None else None,
        "exportFile": str(dst.relative_to(ROOT)).replace("\\", "/"),
        "exportDimensions": list(target),
        "format": "PNG RGBA, sRGB assumed",
        "alpha": "preserved" if "A" in exported.getbands() else "opaque",
        "alphaBounds": list(alpha.getbbox()) if alpha.getbbox() else None,
        "repeatEdgeMeanAbsDiffRGBA": {axis: edge_differences(exported, axis) for axis in edge_axes},
    }


def make_preview(ground_path: Path, track_path: Path) -> None:
    ground = Image.open(ground_path).convert("RGBA")
    track = Image.open(track_path).convert("RGBA")
    preview = Image.new("RGBA", (1024, 576), (115, 137, 161, 255))
    for tile_index in range(2):
        tile = ground.copy()
        tile.alpha_composite(track, (0, 220))
        preview.alpha_composite(tile, (tile_index * 512, 24))
    ImageDraw.Draw(preview).text(
        (10, 4),
        "T_Snow_Ground + T_Snow_Track on two adjacent 512px segments",
        fill=(250, 250, 248, 255),
    )
    preview.convert("RGB").save(PREVIEW_PATH, format="PNG", optimize=True)


def main() -> None:
    EXPORT_DIR.mkdir(parents=True, exist_ok=True)
    EVIDENCE_DIR.mkdir(parents=True, exist_ok=True)
    ground = texture_record(
        "T_Snow_Ground",
        "T_Snow_Ground_Source_v01.png",
        "exec-8e7e092d-a340-4a7d-8d46-62370088966c",
        "T_Snow_Ground_v01_WIP.png",
        (512, 512),
        ("horizontal", "vertical"),
    )
    track = texture_record(
        "T_Snow_Track",
        "T_Snow_Track_Source_v01.png",
        "exec-e7dc7cec-fad7-45de-964d-a323b498f078",
        "T_Snow_Track_v01_WIP.png",
        (512, 64),
        ("horizontal",),
        (2, 296, 1982, 544),
    )
    ground_path = ROOT / str(ground["exportFile"])
    track_path = ROOT / str(track["exportFile"])
    make_preview(ground_path, track_path)
    manifest = {
        "manifestId": "P5_A2_SnowSurface",
        "version": "v01",
        "created": "2026-10-28",
        "status": "WIP; candidates only; not accepted for runtime import",
        "approvedDirection": "P5 A0 natural-tone v02 scene",
        "exportTool": "UE5/Scripts/p5_export_a2_snow_candidates.py",
        "assets": [ground, track],
        "preview": str(PREVIEW_PATH.relative_to(ROOT)).replace("\\", "/"),
        "gates": {
            "dimensions": "exported to specification",
            "textureRepeat": "edge metrics and static repeat preview recorded; route UV scale and material sampling pending",
            "transparency": "track alpha preserved; actual UE filtering/compression review pending",
            "routeIntegration": "pending actual snow-slope fixture",
            "lightingAndColor": "static natural-tone direction only; final in-scene balance pending",
        },
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Exported {len(manifest['assets'])} WIP assets.")
    for asset in manifest["assets"]:
        print(f"{asset['assetId']}: {asset['exportDimensions']} {asset['repeatEdgeMeanAbsDiffRGBA']}")
    print(f"Preview: {PREVIEW_PATH}")
    print(f"Manifest: {MANIFEST_PATH}")


if __name__ == "__main__":
    main()
