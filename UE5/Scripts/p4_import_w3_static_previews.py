"""Import owner-approved W3 static preview frames as isolated Texture2D assets.

This imports two front-view concept crops for visual scale integration only. It
does not import the six-state pose boards or open/switch/run a level.
"""
from __future__ import annotations

import hashlib
import json
import struct
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
P4_SOURCE = ROOT / "UE5/SourceAssets/P4"
MANIFEST_PATH = P4_SOURCE / "Manifests/P4_W3_StaticPreview_manifest_v02.json"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/W3/P4_W3_StaticPreview_import_report_v02.json"
DEST_ROOT = "/Game/Naiwa/P4/Art/Alpha/W3_Enemies/StaticPreview"
EXPECTED_SCHEMA = "naiwa.p4.w3_enemy_static_preview/1"
APPROVED_STATUS = "approved_for_static_preview_only"

report = {
    "status": "running",
    "manifest": str(MANIFEST_PATH),
    "destination_root": DEST_ROOT,
    "engine": unreal.SystemLibrary.get_engine_version(),
    "level_opened_or_changed": False,
    "runtime_played": False,
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def png_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(26)
    if len(header) != 26 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"Invalid PNG header: {path}")
    width, height = struct.unpack(">II", header[16:24])
    if header[25] != 6:
        raise ValueError(f"Expected RGBA PNG color type 6: {path}")
    return width, height


def prepare_assets(manifest: dict) -> list[dict]:
    if manifest.get("schema") != EXPECTED_SCHEMA:
        raise ValueError(f"Unexpected manifest schema: {manifest.get('schema')!r}")
    if manifest.get("status") != APPROVED_STATUS:
        raise ValueError(f"Static-preview-only import is not authorized: {manifest.get('status')!r}")
    entries = manifest.get("assets")
    if not isinstance(entries, list) or len(entries) != 2:
        raise ValueError("Expected exactly two static preview entries")

    prepared = []
    seen_ids = set()
    for entry in entries:
        enemy_id = entry.get("enemy_id", "")
        if enemy_id not in {"naiding", "naidun"} or enemy_id in seen_ids:
            raise ValueError(f"Unexpected or duplicate enemy id: {enemy_id!r}")
        seen_ids.add(enemy_id)
        source = (ROOT / entry["file"]).resolve()
        if not source.is_file():
            raise FileNotFoundError(source)
        actual_hash = sha256_file(source)
        if actual_hash != entry.get("sha256", "").lower():
            raise ValueError(f"Source hash mismatch for {enemy_id}: {actual_hash}")
        dimensions = png_dimensions(source)
        if dimensions != tuple(entry.get("dimensions_px", [])) or dimensions != (512, 512):
            raise ValueError(f"Unexpected source dimensions for {enemy_id}: {dimensions}")

        asset_path = entry["texture_asset"]
        asset_name = asset_path.rsplit("/", 1)[-1]
        existing = unreal.load_asset(asset_path)
        existing_source = None
        if existing:
            if not isinstance(existing, unreal.Texture2D):
                raise RuntimeError(f"Destination is not a Texture2D: {asset_path}")
            import_data = existing.get_editor_property("asset_import_data")
            existing_source = import_data.get_first_filename() if import_data else ""
            if not existing_source or Path(existing_source).resolve().as_posix().casefold() != source.as_posix().casefold():
                raise RuntimeError(f"Refusing to overwrite a different or unknown source: {asset_path}")
        prepared.append({
            "enemy_id": enemy_id,
            "source": source,
            "sha256": actual_hash,
            "dimensions_px": list(dimensions),
            "asset_path": asset_path,
            "asset_name": asset_name,
            "existing": bool(existing),
        })
    if seen_ids != {"naiding", "naidun"}:
        raise ValueError(f"Unexpected static preview coverage: {seen_ids}")
    return prepared


def import_one(entry: dict) -> dict:
    texture = unreal.load_asset(entry["asset_path"])
    imported_now = False
    if not texture:
        unreal.EditorAssetLibrary.make_directory(DEST_ROOT)
        task = unreal.AssetImportTask()
        task.set_editor_properties({
            "filename": str(entry["source"]),
            "destination_path": DEST_ROOT,
            "destination_name": entry["asset_name"],
            "replace_existing": False,
            "automated": True,
            "save": False,
        })
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(entry["asset_path"])
        if not texture or not isinstance(texture, unreal.Texture2D):
            raise RuntimeError(f"Texture2D import did not create {entry['asset_path']}")
        imported_now = True

    texture.set_editor_properties({
        "srgb": True,
        "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        "compression_no_alpha": False,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_SHARPEN0,
        "lod_group": unreal.TextureGroup.TEXTUREGROUP_CHARACTER,
        "never_stream": True,
    })
    expected = {
        "srgb": True,
        "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        "compression_no_alpha": False,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_SHARPEN0,
        "lod_group": unreal.TextureGroup.TEXTUREGROUP_CHARACTER,
        "never_stream": True,
    }
    for name, value in expected.items():
        if texture.get_editor_property(name) != value:
            raise RuntimeError(f"Texture setting readback failed: {entry['asset_path']} property {name}")
    if not unreal.EditorAssetLibrary.save_asset(entry["asset_path"], only_if_is_dirty=False):
        raise RuntimeError(f"Could not save Texture2D: {entry['asset_path']}")
    return {
        "enemy_id": entry["enemy_id"],
        "asset_path": entry["asset_path"],
        "source": str(entry["source"]),
        "source_sha256": entry["sha256"],
        "dimensions_px": entry["dimensions_px"],
        "result": "imported" if imported_now else "reused_same_source",
        "texture_readback": "passed",
        "mip_generation": "Sharpen0",
        "compression": "TC_EditorIcon (uncompressed RGBA8)",
    }


def main() -> None:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    report["manifest_status"] = manifest.get("status")
    prepared = prepare_assets(manifest)
    report["preflight"] = {
        "status": "passed",
        "asset_count": len(prepared),
        "dimensions_px": [512, 512],
        "existing_same_source_assets": sum(entry["existing"] for entry in prepared),
    }
    report["assets"] = [import_one(entry) for entry in prepared]
    report["status"] = "imported_static_preview_pending_runtime_review"
    report["limitations"] = [
        "These are static front-view preview textures only; no six-state animation frames were imported.",
        "The script did not open or switch a level and did not run PIE/Standalone.",
    ]


try:
    if not MANIFEST_PATH.is_file():
        raise FileNotFoundError(MANIFEST_PATH)
    main()
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    raise
finally:
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("NAIWA_P4_W3_STATIC_PREVIEW_IMPORT " + json.dumps(report, ensure_ascii=False))
