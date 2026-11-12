"""Import P4 Beta6 production environment textures only; engineering owns runtime binding."""
from __future__ import annotations

import hashlib
import json
import struct
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = ROOT / "UE5/SourceAssets/P4/Environment/Beta6"
MANIFEST_PATH = SOURCE_ROOT / "P4_Beta6_EnvironmentArt_manifest_v01.json"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_EnvironmentArt_import_report_v01.json"
CONTENT_ROOT = "/Game/Naiwa/P4/Art/Beta6/Environment"

IMPORT_SETTINGS = {
    "srgb": True,
    "compression_settings": unreal.TextureCompressionSettings.TC_DEFAULT,
    "compression_no_alpha": True,
    "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP,
    "lod_group": unreal.TextureGroup.TEXTUREGROUP_WORLD,
    "never_stream": False,
}

report = {
    "schema": "naiwa.p4.beta6.environment_art_import/1",
    "status": "running",
    "engine": unreal.SystemLibrary.get_engine_version(),
    "import_scope": "textures only; no materials, maps, theme binding, or runtime readability review",
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
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"Expected a valid PNG file: {path}")
    width, height = struct.unpack(">II", header[16:24])
    if width < 1600 or height < 900 or abs(width / height - 16.0 / 9.0) > 0.01:
        raise ValueError(f"Environment plate must be at least 1600x900 at 16:9: {width}x{height}")
    return width, height


def import_texture(entry: dict) -> dict:
    source = (ROOT / entry["source_path"]).resolve()
    if not source.is_file():
        raise FileNotFoundError(source)
    source_hash = sha256_file(source)
    if source_hash.lower() != entry.get("sha256", "").lower():
        raise ValueError(f"Environment art hash mismatch: {source}")
    width, height = inspect_png(source)
    if [width, height] != entry.get("dimensions_px"):
        raise ValueError(f"Environment art dimensions changed since manifest: {source}")

    asset_name = entry["ue_texture_asset"].rsplit("/", 1)[1]
    asset_path = f"{CONTENT_ROOT}/{asset_name}"
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
        raise RuntimeError(f"Could not import environment texture: {asset_path}")

    texture.set_editor_properties(IMPORT_SETTINGS)
    for key, expected in IMPORT_SETTINGS.items():
        if texture.get_editor_property(key) != expected:
            raise RuntimeError(f"Texture setting readback failed for {asset_path}: {key}")
    if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save environment texture: {asset_path}")

    return {
        "asset_id": entry["asset_id"],
        "chapter_id": entry["chapter_id"],
        "source_path": entry["source_path"],
        "source_sha256": source_hash,
        "dimensions_px": [width, height],
        "texture_asset": asset_path,
        "imported_now": imported_now,
        "texture_settings": "sRGB, TC_Default, no alpha, mip settings from World texture group, streamable",
        "status": "texture_imported_pending_engineering_theme_binding_and_readability_review",
    }


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    if manifest.get("schema") != "naiwa.p4.beta6.environment_art_manifest/1":
        raise ValueError("Unexpected Beta6 environment art manifest schema")
    entries = manifest.get("assets", [])
    if [entry.get("chapter_id") for entry in entries] != [f"chapter_{i}" for i in range(2, 7)]:
        raise ValueError("Expected five unique Beta6 environment assets for Chapters 2 through 6")

    results = []
    for entry in entries:
        result = import_texture(entry)
        entry["status"] = "production_art_imported_pending_engineering_review"
        entry["runtime_reference"] = "not bound; engineering owns chapter theme binding and fixed-camera readability review"
        results.append(result)

    manifest["status"] = "production_backgrounds_imported_pending_engineering_theme_binding_and_readability_review"
    write_json(MANIFEST_PATH, manifest)
    report.update({
        "status": "textures_imported_pending_engineering_theme_binding_and_readability_review",
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
