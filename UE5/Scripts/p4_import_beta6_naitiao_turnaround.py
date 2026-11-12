"""Import the Beta6 Naitiao turnaround as a review reference texture only."""
from __future__ import annotations

import hashlib
import json
import struct
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = ROOT / "UE5/SourceAssets/P4/Beta6/Characters/Enemies"
MANIFEST_PATH = SOURCE_ROOT / "P4_Beta6_NaitiaoTurnaround_manifest_v01.json"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_NaitiaoTurnaround_import_report_v01.json"
CONTENT_ROOT = "/Game/Naiwa/P4/Art/Beta6/Characters/Reference"

IMPORT_SETTINGS = {
    "srgb": True,
    "compression_settings": unreal.TextureCompressionSettings.TC_DEFAULT,
    "compression_no_alpha": False,
    "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
    "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
    "never_stream": True,
}

report = {
    "schema": "naiwa.p4.beta6.enemy_turnaround_import/1",
    "status": "running",
    "engine": unreal.SystemLibrary.get_engine_version(),
    "runtime_reference": "none; static Naitiao turnaround reference only",
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def inspect_png(path: Path) -> tuple[int, int, int]:
    with path.open("rb") as stream:
        header = stream.read(26)
    if len(header) != 26 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"Expected a valid PNG file: {path}")
    width, height = struct.unpack(">II", header[16:24])
    color_type = header[25]
    if width < 2000 or height < 700 or abs(width / height - 3.0) > 0.1:
        raise ValueError(f"Naitiao three-view sheet must be a wide 3-view plate: {width}x{height}")
    if color_type != 6:
        raise ValueError(f"Expected RGBA PNG with alpha, received color type {color_type}: {path}")
    return width, height, color_type


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    if manifest.get("schema") != "naiwa.p4.beta6.enemy_turnaround_manifest/1":
        raise ValueError("Unexpected Naitiao turnaround manifest schema")
    source = (ROOT / manifest["source_path"]).resolve()
    if not source.is_file():
        raise FileNotFoundError(source)
    source_hash = sha256_file(source)
    if source_hash.lower() != manifest.get("sha256", "").lower():
        raise ValueError(f"Naitiao turnaround hash mismatch: {source}")
    width, height, _ = inspect_png(source)
    if [width, height] != manifest.get("dimensions_px"):
        raise ValueError("Naitiao turnaround dimensions changed since manifest creation")

    asset_path = manifest["ue_texture_asset"]
    asset_name = asset_path.rsplit("/", 1)[1]
    texture = unreal.load_asset(asset_path)
    imported_now = False
    if not texture:
        unreal.EditorAssetLibrary.make_directory(CONTENT_ROOT)
        task = unreal.AssetImportTask()
        task.set_editor_properties({
            "filename": str(source),
            "destination_path": CONTENT_ROOT,
            "destination_name": asset_name,
            "replace_existing": False,
            "automated": True,
            "save": False,
        })
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(asset_path)
        imported_now = True
    if not texture or not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Could not import Naitiao reference texture: {asset_path}")
    texture.set_editor_properties(IMPORT_SETTINGS)
    for key, expected in IMPORT_SETTINGS.items():
        if texture.get_editor_property(key) != expected:
            raise RuntimeError(f"Texture setting readback failed for {asset_path}: {key}")
    if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save Naitiao reference texture: {asset_path}")

    manifest["status"] = "turnaround_imported_pending_owner_review"
    write_json(MANIFEST_PATH, manifest)
    report.update({
        "status": "reference_texture_imported_pending_owner_review",
        "asset_id": manifest["asset_id"],
        "source_path": manifest["source_path"],
        "source_sha256": source_hash,
        "dimensions_px": [width, height],
        "alpha_mode": manifest["alpha_mode"],
        "texture_asset": asset_path,
        "imported_now": imported_now,
        "texture_settings": "sRGB, TC_Default, alpha preserved, no mips, UI group, non-streaming",
        "review_gate": manifest["review_gate"],
    })
    write_json(REPORT_PATH, report)


try:
    main()
except Exception as exc:
    report["status"] = "failed"
    report["error"] = str(exc)
    report["traceback"] = traceback.format_exc()
    write_json(REPORT_PATH, report)
    raise
