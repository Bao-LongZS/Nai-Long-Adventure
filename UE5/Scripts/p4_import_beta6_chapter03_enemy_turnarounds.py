"""Import the Chapter 3 Beta6 enemy turnarounds as static reference textures."""
from __future__ import annotations

import hashlib
import json
import struct
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = ROOT / "UE5/SourceAssets/P4/Beta6/Characters/Enemies"
MANIFEST_PATH = SOURCE_ROOT / "P4_Beta6_Chapter03EnemyTurnarounds_manifest_v01.json"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_Chapter03EnemyTurnarounds_import_report_v01.json"
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
    "schema": "naiwa.p4.beta6.chapter03_enemy_turnarounds_import/1",
    "status": "running",
    "engine": unreal.SystemLibrary.get_engine_version(),
    "runtime_reference": "none; static Chapter 3 enemy reference sheets only",
    "assets": [],
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def inspect_png(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(26)
    if len(header) != 26 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"Expected a valid PNG file: {path}")
    width, height = struct.unpack(">II", header[16:24])
    if width < 2000 or height < 700 or abs(width / height - 3.0) > 0.1 or header[25] != 6:
        raise ValueError(f"Expected a three-view RGBA turnaround sheet: {width}x{height}")
    return width, height


def import_texture(entry: dict) -> dict:
    source = (ROOT / entry["source_path"]).resolve()
    if not source.is_file():
        raise FileNotFoundError(source)
    source_hash = sha256_file(source)
    if source_hash.lower() != entry.get("sha256", "").lower():
        raise ValueError(f"Turnaround source hash mismatch: {source}")
    width, height = inspect_png(source)
    if [width, height] != entry.get("dimensions_px"):
        raise ValueError(f"Turnaround dimensions changed since manifest creation: {source}")

    asset_path = entry["ue_texture_asset"]
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
        raise RuntimeError(f"Could not import Chapter 3 turnaround texture: {asset_path}")
    texture.set_editor_properties(IMPORT_SETTINGS)
    for key, expected in IMPORT_SETTINGS.items():
        if texture.get_editor_property(key) != expected:
            raise RuntimeError(f"Texture setting readback failed for {asset_path}: {key}")
    if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save turnaround texture: {asset_path}")

    return {
        "asset_id": entry["asset_id"],
        "character_id": entry["character_id"],
        "source_path": entry["source_path"],
        "source_sha256": source_hash,
        "dimensions_px": [width, height],
        "alpha_mode": entry["alpha_mode"],
        "texture_asset": asset_path,
        "imported_now": imported_now,
        "texture_settings": "sRGB, TC_Default, alpha preserved, no mips, UI group, non-streaming",
        "status": "turnaround_imported_pending_owner_review",
    }


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    if manifest.get("schema") != "naiwa.p4.beta6.enemy_turnarounds_manifest/1":
        raise ValueError("Unexpected Chapter 3 enemy turnaround manifest schema")
    entries = manifest.get("assets", [])
    if [entry.get("character_id") for entry in entries] != ["nailing", "naichui"]:
        raise ValueError("Expected the Chapter 3 enemy pair: Nailing and Naichui")
    results = [import_texture(entry) for entry in entries]
    for entry in entries:
        entry["status"] = "turnaround_imported_pending_owner_review"
    manifest["status"] = "chapter03_turnarounds_imported_pending_owner_review"
    write_json(MANIFEST_PATH, manifest)
    report.update({
        "status": "chapter03_turnarounds_imported_pending_owner_review",
        "asset_count": len(results),
        "assets": results,
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
