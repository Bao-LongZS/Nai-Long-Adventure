"""Import only user-approved P7 A1 tower textures into the UE project.

This script does not create or modify levels, gameplay data, P4/P5 assets, or
runtime code. Imported textures remain pending scale and in-game visual review.
"""
from __future__ import annotations

import csv
import hashlib
import json
import struct
import traceback
from datetime import datetime
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = ROOT / "UE5/SourceAssets/P7"
MANIFEST_PATH = SOURCE_ROOT / "Manifests/A1/P7_A1_Assets_v04.csv"
DEST_ROOT = "/Game/Naiwa/P7/Towers/A1"
EVIDENCE_ROOT = ROOT / "UE5/Records/p7_td/evidence"
REPORT_PATH = EVIDENCE_ROOT / (
    "P7_A1_ApprovedTowerImport_"
    + datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    + ".json"
)

report = {
    "status": "running",
    "engine": None,
    "manifest": str(MANIFEST_PATH),
    "destination": DEST_ROOT,
    "scope": "nine user-approved P7 A1 tower textures only",
    "level_opened_or_changed": False,
    "runtime_played": False,
    "assets": [],
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
    if (len(header) != 26 or header[:8] != b"\x89PNG\r\n\x1a\n"
            or header[12:16] != b"IHDR" or header[25] != 6):
        raise ValueError(f"Expected an RGBA PNG source: {path}")
    return struct.unpack(">II", header[16:24])


def same_path(first: str, second: Path) -> bool:
    if not first:
        return False
    try:
        return Path(first).resolve().as_posix().casefold() == second.resolve().as_posix().casefold()
    except OSError:
        return False


def preflight() -> list[dict]:
    if not MANIFEST_PATH.is_file():
        raise FileNotFoundError(MANIFEST_PATH)

    prepared = []
    with MANIFEST_PATH.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        fieldnames = reader.fieldnames or []
        required = {
            "AssetId", "SourcePath", "SourceHash", "Width", "Height",
            "UnrealPath", "ReviewStatus", "EvidencePath", "Notes",
        }
        if not required.issubset(fieldnames):
            raise ValueError(f"P7 A1 manifest is missing fields: {sorted(required - set(fieldnames))}")

        rows = list(reader)
    if len(rows) != 9:
        raise ValueError(f"Expected exactly nine P7 A1 tower rows; found {len(rows)}")

    seen_paths = set()
    for row in rows:
        if not row["AssetId"].startswith("P7-TWR-A1-"):
            raise ValueError(f"Unexpected P7 AssetId: {row['AssetId']!r}")
        if not row["ReviewStatus"].startswith("Approved by user"):
            raise ValueError(f"Refusing to import unapproved source: {row['AssetId']}")

        source = (SOURCE_ROOT / row["SourcePath"]).resolve()
        if not source.is_relative_to(SOURCE_ROOT.resolve()) or not source.is_file():
            raise FileNotFoundError(f"Invalid P7 source path: {source}")
        actual_hash = sha256_file(source)
        if actual_hash != row["SourceHash"].lower():
            raise ValueError(f"Source hash mismatch for {row['AssetId']}: {actual_hash}")
        dimensions = png_dimensions(source)
        expected_dimensions = (int(row["Width"]), int(row["Height"]))
        if dimensions != expected_dimensions or dimensions != (1254, 1254):
            raise ValueError(f"Unexpected dimensions for {row['AssetId']}: {dimensions}")

        asset_path = row["UnrealPath"]
        if not asset_path.startswith(DEST_ROOT + "/T_P7_A1_"):
            raise ValueError(f"Unexpected P7 destination for {row['AssetId']}: {asset_path}")
        if asset_path in seen_paths:
            raise ValueError(f"Duplicate Unreal destination: {asset_path}")
        seen_paths.add(asset_path)

        existing = unreal.load_asset(asset_path)
        if existing:
            if not isinstance(existing, unreal.Texture2D):
                raise TypeError(f"Destination exists but is not a Texture2D: {asset_path}")
            import_data = existing.get_editor_property("asset_import_data")
            imported_source = import_data.get_first_filename() if import_data else ""
            if not same_path(imported_source, source):
                raise RuntimeError(f"Refusing to replace an unrelated asset: {asset_path}")

        prepared.append({
            "asset_id": row["AssetId"],
            "source": source,
            "source_hash": actual_hash,
            "dimensions": dimensions,
            "asset_path": asset_path,
            "asset_name": asset_path.rsplit("/", 1)[-1],
            "already_imported": bool(existing),
        })

    return prepared


def import_one(entry: dict) -> dict:
    texture = unreal.load_asset(entry["asset_path"])
    result = "reused_same_source"
    if not texture:
        task = unreal.AssetImportTask()
        task.set_editor_properties({
            "filename": str(entry["source"]),
            "destination_path": DEST_ROOT,
            "destination_name": entry["asset_name"],
            "replace_existing": False,
            "automated": True,
            "save": True,
        })
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(entry["asset_path"])
        if not texture:
            raise RuntimeError(f"Texture2D import did not create {entry['asset_path']}")
        result = "imported"

    if not isinstance(texture, unreal.Texture2D):
        raise TypeError(f"Imported asset is not a Texture2D: {entry['asset_path']}")

    texture.set_editor_properties({
        "srgb": True,
        "compression_settings": unreal.TextureCompressionSettings.TC_DEFAULT,
        "compression_no_alpha": False,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
        "never_stream": True,
    })
    expected = {
        "srgb": True,
        "compression_settings": unreal.TextureCompressionSettings.TC_DEFAULT,
        "compression_no_alpha": False,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
        "never_stream": True,
    }
    for name, value in expected.items():
        if texture.get_editor_property(name) != value:
            raise RuntimeError(f"Texture setting readback failed: {entry['asset_path']} ({name})")

    if not unreal.EditorAssetLibrary.save_asset(entry["asset_path"], only_if_is_dirty=False):
        raise RuntimeError(f"Could not save P7 texture: {entry['asset_path']}")

    import_data = texture.get_editor_property("asset_import_data")
    imported_source = import_data.get_first_filename() if import_data else ""
    if not same_path(imported_source, entry["source"]):
        raise RuntimeError(f"Imported source path readback failed: {entry['asset_path']}")

    return {
        "asset_id": entry["asset_id"],
        "asset_path": entry["asset_path"],
        "source": str(entry["source"]),
        "source_sha256": entry["source_hash"],
        "dimensions_px": list(entry["dimensions"]),
        "result": result,
        "texture_settings_readback": "passed",
    }


def update_manifest() -> None:
    with MANIFEST_PATH.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        fieldnames = reader.fieldnames
        rows = list(reader)
    if not fieldnames:
        raise ValueError("P7 A1 manifest has no CSV header")

    evidence_path = "UE5/Records/p7_td/evidence/" + REPORT_PATH.name
    for row in rows:
        if row["AssetId"].startswith("P7-TWR-A1-"):
            row["ReviewStatus"] = (
                "Approved by user; UE Texture2D import complete; "
                "scale/visual/PIE checks pending"
            )
            row["EvidencePath"] = evidence_path
            note = "Imported to /Game/Naiwa/P7/Towers/A1 as an sRGB RGBA UI texture; in-game scale and visual review pending."
            if note not in row["Notes"]:
                row["Notes"] = (row["Notes"] + " " + note).strip()

    with MANIFEST_PATH.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


try:
    report["engine"] = unreal.SystemLibrary.get_engine_version()
    entries = preflight()
    report["preflight"] = {
        "status": "passed",
        "approved_source_count": len(entries),
        "already_imported_count": sum(item["already_imported"] for item in entries),
        "texture_dimensions_px": [1254, 1254],
    }
    unreal.EditorAssetLibrary.make_directory(DEST_ROOT)
    report["assets"] = [import_one(entry) for entry in entries]
    update_manifest()
    report["status"] = "imported_approved_tower_textures_pending_scale_review"
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    raise
finally:
    EVIDENCE_ROOT.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("NAIWA_P7_A1_TOWER_IMPORT " + json.dumps(report, ensure_ascii=False))
