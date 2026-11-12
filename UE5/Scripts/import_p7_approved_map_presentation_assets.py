"""Import the approved P7 field board and guardian egg presentation textures."""
from __future__ import annotations

import csv
import hashlib
import json
import struct
from datetime import datetime
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = ROOT / "UE5/SourceAssets/P7"
EVIDENCE_ROOT = ROOT / "UE5/Records/p7_td/evidence"
REPORT_PATH = EVIDENCE_ROOT / (
    "P7_ApprovedMapPresentationAssetImport_"
    + datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    + ".json"
)
ASSETS = (
    {
        "asset_id": "P7-ENV-A0-GROUNDBOARD",
        "manifest": SOURCE_ROOT / "Manifests/A0/P7_A0_Assets_v04.csv",
        "destination": "/Game/Naiwa/P7/Environment/A0/T_P7_A0_GroundBoard_Field_v01",
        "expected_size": (1672, 941),
        "approval_prefix": "User approved as A0 environment sample",
    },
    {
        "asset_id": "P7-ENV-A1-GUARDIAN-MILK-EGG",
        "manifest": SOURCE_ROOT / "Manifests/A1/P7_A1_GuardianMilkEggAssets_v04.csv",
        "destination": "/Game/Naiwa/P7/Environment/A1/T_P7_A1_GuardianMilkEgg_v04",
        "expected_size": (1254, 1254),
        "approval_prefix": "User-approved visual",
    },
)
report = {
    "status": "running",
    "engine": None,
    "scope": "two user-approved P7 map presentation textures only; no map asset or gameplay data changed",
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


def png_metadata(path: Path) -> tuple[int, int, int, int]:
    with path.open("rb") as stream:
        header = stream.read(26)
    if len(header) != 26 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"Invalid PNG header: {path}")
    width, height = struct.unpack(">II", header[16:24])
    bit_depth, color_type = header[24], header[25]
    return width, height, bit_depth, color_type


def same_path(first: str, second: Path) -> bool:
    if not first:
        return False
    try:
        return Path(first).resolve().as_posix().casefold() == second.resolve().as_posix().casefold()
    except OSError:
        return False


def read_manifest_row(config: dict) -> tuple[dict, Path, str]:
    if not config["manifest"].is_file():
        raise FileNotFoundError(config["manifest"])
    with config["manifest"].open("r", encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    matches = [row for row in rows if row.get("AssetId") == config["asset_id"]]
    if len(matches) != 1:
        raise ValueError(f"Expected one manifest row for {config['asset_id']}; found {len(matches)}")
    row = matches[0]
    review_status = row.get("ReviewStatus", "")
    if not review_status.startswith(config["approval_prefix"]):
        raise ValueError(f"Refusing to import an unapproved asset: {config['asset_id']} ({review_status})")
    source_path = (SOURCE_ROOT / row["ExportPath"]).resolve()
    if not source_path.is_relative_to(SOURCE_ROOT.resolve()) or not source_path.is_file():
        raise FileNotFoundError(f"Invalid or missing approved export: {source_path}")
    actual_hash = sha256_file(source_path)
    expected_hash = row.get("ExportHash") or row.get("SourceHash")
    if not expected_hash or actual_hash != expected_hash.lower():
        raise ValueError(f"Approved export hash mismatch for {config['asset_id']}")
    width, height, bit_depth, color_type = png_metadata(source_path)
    if (width, height) != config["expected_size"] or bit_depth != 8 or color_type != 6:
        raise ValueError(f"Unexpected RGBA PNG metadata for {config['asset_id']}: {(width, height, bit_depth, color_type)}")
    if int(row.get("Width", 0)) != width or int(row.get("Height", 0)) != height:
        raise ValueError(f"Manifest dimensions disagree for {config['asset_id']}")
    if row.get("UnrealPath") != config["destination"]:
        raise ValueError(f"Unexpected Unreal destination for {config['asset_id']}: {row.get('UnrealPath')}")
    return row, source_path, actual_hash


def import_texture(config: dict, source_path: Path) -> dict:
    destination = config["destination"]
    texture = unreal.load_asset(destination)
    action = "reused_same_source"
    if texture:
        if not isinstance(texture, unreal.Texture2D):
            raise TypeError(f"Destination exists but is not a Texture2D: {destination}")
        import_data = texture.get_editor_property("asset_import_data")
        imported_source = import_data.get_first_filename() if import_data else ""
        if not same_path(imported_source, source_path):
            raise RuntimeError(f"Refusing to replace an unrelated texture: {destination}")
    else:
        task = unreal.AssetImportTask()
        task.set_editor_properties({
            "filename": str(source_path),
            "destination_path": destination.rsplit("/", 1)[0],
            "destination_name": destination.rsplit("/", 1)[-1],
            "replace_existing": False,
            "automated": True,
            "save": True,
        })
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(destination)
        if not texture:
            raise RuntimeError(f"Texture import did not create {destination}")
        action = "imported"

    if not isinstance(texture, unreal.Texture2D):
        raise TypeError(f"Imported object is not a Texture2D: {destination}")
    texture.set_editor_properties({
        "srgb": True,
        "compression_settings": unreal.TextureCompressionSettings.TC_DEFAULT,
        "compression_no_alpha": False,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
        "never_stream": True,
    })
    for name, expected in {
        "srgb": True,
        "compression_settings": unreal.TextureCompressionSettings.TC_DEFAULT,
        "compression_no_alpha": False,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
        "never_stream": True,
    }.items():
        if texture.get_editor_property(name) != expected:
            raise RuntimeError(f"Texture setting readback failed: {destination} ({name})")
    if not unreal.EditorAssetLibrary.save_asset(destination, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save imported texture: {destination}")
    import_data = texture.get_editor_property("asset_import_data")
    imported_source = import_data.get_first_filename() if import_data else ""
    if not same_path(imported_source, source_path):
        raise RuntimeError(f"Imported source path readback failed: {destination}")
    return {
        "asset_path": destination,
        "source_path": str(source_path),
        "source_sha256": sha256_file(source_path),
        "width": config["expected_size"][0],
        "height": config["expected_size"][1],
        "action": action,
        "texture_group": "UI",
        "mipmaps": False,
        "srgb": True,
        "alpha": True,
    }


try:
    unreal.log("P7 approved map presentation import started")
    report["engine"] = unreal.SystemLibrary.get_engine_version()
    for config in ASSETS:
        row, source_path, actual_hash = read_manifest_row(config)
        item = import_texture(config, source_path)
        item.update({
            "asset_id": config["asset_id"],
            "version": row["Version"],
            "review_status": row["ReviewStatus"],
            "manifest_hash": actual_hash,
        })
        report["assets"].append(item)
    report["status"] = "imported_approved_map_presentation_textures"
    unreal.log("P7 approved map presentation import completed")
except Exception as exc:
    report["status"] = "failed"
    report["error"] = str(exc)
    unreal.log_error(f"P7 approved map presentation import failed: {exc}")

REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
if report["status"] != "imported_approved_map_presentation_textures":
    raise RuntimeError(report.get("error", "P7 texture import failed"))
