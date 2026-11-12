"""Import the user-approved P7 A1 VFX atlas as a transparent UI Texture2D."""
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
MANIFEST_PATH = SOURCE_ROOT / "Manifests/A1/P7_A1_VFXAssets_v07.csv"
DEST_ROOT = "/Game/Naiwa/P7/VFX/A1"
EXPECTED_ASSET_PATH = f"{DEST_ROOT}/T_P7_A1_VFXAtlas_v07"
EVIDENCE_ROOT = ROOT / "UE5/Records/p7_td/evidence"
REPORT_PATH = EVIDENCE_ROOT / (
    "P7_A1_ApprovedVFXAtlasImport_"
    + datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    + ".json"
)
EXPECTED_FRAME_CELLS = {
    (16 + column * 208, 16 + row * 208)
    for row in range(4)
    for column in range(4)
}

report = {
    "status": "running",
    "engine": None,
    "manifest": str(MANIFEST_PATH),
    "destination": EXPECTED_ASSET_PATH,
    "scope": "one user-approved P7 A1 4x4 VFX atlas texture; runtime playback is separate",
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


def png_metadata(path: Path) -> tuple[int, int, int]:
    with path.open("rb") as stream:
        header = stream.read(26)
    if (len(header) != 26 or header[:8] != b"\x89PNG\r\n\x1a\n"
            or header[12:16] != b"IHDR"):
        raise ValueError(f"Invalid PNG header: {path}")
    width, height = struct.unpack(">II", header[16:24])
    bit_depth, color_type = header[24], header[25]
    return width, height, (bit_depth << 8) | color_type


def same_path(first: str, second: Path) -> bool:
    if not first:
        return False
    try:
        return Path(first).resolve().as_posix().casefold() == second.resolve().as_posix().casefold()
    except OSError:
        return False


def preflight() -> dict:
    if not MANIFEST_PATH.is_file():
        raise FileNotFoundError(MANIFEST_PATH)

    with MANIFEST_PATH.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        fieldnames = reader.fieldnames or []
        required = {
            "AssetId", "SourcePath", "SourceHash", "ExportPath", "ExportHash",
            "Version", "FrameCount", "FrameIndex", "AtlasX", "AtlasY",
            "AtlasW", "AtlasH", "UnrealPath", "ReviewStatus",
        }
        if not required.issubset(fieldnames):
            raise ValueError(f"Manifest is missing fields: {sorted(required - set(fieldnames))}")
        rows = list(reader)
    if len(rows) != 16:
        raise ValueError(f"Expected 16 approved VFX frame rows; found {len(rows)}")

    approved_paths = set()
    seen_ids = set()
    seen_cells = set()
    svg_hash = None
    png_hash = None
    png_path = None
    for row in rows:
        asset_id = row["AssetId"]
        if asset_id in seen_ids:
            raise ValueError(f"Duplicate AssetId: {asset_id}")
        seen_ids.add(asset_id)
        if not asset_id.startswith("P7-VFX-A1-") or row["Version"] != "v07":
            raise ValueError(f"Unexpected VFX manifest row: {asset_id}")
        if not row["ReviewStatus"].startswith("Approved by user"):
            raise ValueError(f"Refusing to import unapproved frame: {asset_id}")

        source = (SOURCE_ROOT / row["SourcePath"]).resolve()
        exported = (SOURCE_ROOT / row["ExportPath"]).resolve()
        if not source.is_relative_to(SOURCE_ROOT.resolve()) or not source.is_file():
            raise FileNotFoundError(f"Invalid editable source path: {source}")
        if not exported.is_relative_to(SOURCE_ROOT.resolve()) or not exported.is_file():
            raise FileNotFoundError(f"Invalid texture export path: {exported}")
        actual_svg_hash = sha256_file(source)
        actual_png_hash = sha256_file(exported)
        if actual_svg_hash != row["SourceHash"].lower():
            raise ValueError(f"SVG source hash mismatch for {asset_id}: {actual_svg_hash}")
        if actual_png_hash != row["ExportHash"].lower():
            raise ValueError(f"PNG export hash mismatch for {asset_id}: {actual_png_hash}")
        if svg_hash is not None and actual_svg_hash != svg_hash:
            raise ValueError("Manifest rows do not point to one shared SVG atlas")
        if png_hash is not None and actual_png_hash != png_hash:
            raise ValueError("Manifest rows do not point to one shared PNG atlas")
        svg_hash, png_hash, png_path = actual_svg_hash, actual_png_hash, exported

        dimensions = png_metadata(exported)
        if dimensions != (848, 848, (8 << 8) | 6):
            raise ValueError(f"Expected 848x848 8-bit RGBA PNG; found {dimensions}")
        if (int(row["FrameCount"]) != 4 or int(row["AtlasW"]) != 192
                or int(row["AtlasH"]) != 192):
            raise ValueError(f"Unexpected frame geometry for {asset_id}")
        cell = (int(row["AtlasX"]), int(row["AtlasY"]))
        if cell not in EXPECTED_FRAME_CELLS or cell in seen_cells:
            raise ValueError(f"Unexpected or duplicate atlas cell for {asset_id}: {cell}")
        seen_cells.add(cell)
        if int(row["FrameIndex"]) not in range(16):
            raise ValueError(f"Invalid frame index for {asset_id}")
        approved_paths.add(row["UnrealPath"])

    if seen_cells != EXPECTED_FRAME_CELLS:
        raise ValueError("Manifest does not cover all 16 cells in the 4x4 atlas")
    if approved_paths != {EXPECTED_ASSET_PATH}:
        raise ValueError(f"Unexpected P7 VFX texture destination(s): {sorted(approved_paths)}")

    existing = unreal.load_asset(EXPECTED_ASSET_PATH)
    if existing:
        if not isinstance(existing, unreal.Texture2D):
            raise TypeError(f"Destination exists but is not a Texture2D: {EXPECTED_ASSET_PATH}")
        import_data = existing.get_editor_property("asset_import_data")
        imported_source = import_data.get_first_filename() if import_data else ""
        if not same_path(imported_source, png_path):
            raise RuntimeError(f"Refusing to replace an unrelated asset: {EXPECTED_ASSET_PATH}")

    return {
        "rows": rows,
        "fieldnames": fieldnames,
        "png_path": png_path,
        "svg_hash": svg_hash,
        "png_hash": png_hash,
        "already_imported": bool(existing),
    }


def import_texture(entry: dict) -> dict:
    texture = unreal.load_asset(EXPECTED_ASSET_PATH)
    result = "reused_same_source"
    if not texture:
        task = unreal.AssetImportTask()
        task.set_editor_properties({
            "filename": str(entry["png_path"]),
            "destination_path": DEST_ROOT,
            "destination_name": EXPECTED_ASSET_PATH.rsplit("/", 1)[-1],
            "replace_existing": False,
            "automated": True,
            "save": True,
        })
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(EXPECTED_ASSET_PATH)
        if not texture:
            raise RuntimeError(f"Texture2D import did not create {EXPECTED_ASSET_PATH}")
        result = "imported"

    if not isinstance(texture, unreal.Texture2D):
        raise TypeError(f"Imported asset is not a Texture2D: {EXPECTED_ASSET_PATH}")

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
            raise RuntimeError(f"Texture setting readback failed: {EXPECTED_ASSET_PATH} ({name})")
    if not unreal.EditorAssetLibrary.save_asset(EXPECTED_ASSET_PATH, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save P7 VFX texture: {EXPECTED_ASSET_PATH}")

    import_data = texture.get_editor_property("asset_import_data")
    imported_source = import_data.get_first_filename() if import_data else ""
    if not same_path(imported_source, entry["png_path"]):
        raise RuntimeError(f"Imported source path readback failed: {EXPECTED_ASSET_PATH}")

    return {
        "asset_path": EXPECTED_ASSET_PATH,
        "source_png": str(entry["png_path"]),
        "source_svg_sha256": entry["svg_hash"],
        "export_png_sha256": entry["png_hash"],
        "dimensions_px": [848, 848],
        "frame_count": 16,
        "result": result,
        "texture_settings_readback": "passed",
    }


def update_manifest(entry: dict) -> None:
    evidence_path = "UE5/Records/p7_td/evidence/" + REPORT_PATH.name
    for row in entry["rows"]:
        row["UnrealPath"] = EXPECTED_ASSET_PATH
        if not row["ReviewStatus"].startswith(
            "Approved by user; UE Texture2D import complete; HUD event playback compiled"
        ):
            row["ReviewStatus"] = (
                "Approved by user; UE Texture2D import complete; "
                "runtime playback integration pending"
            )
        existing_evidence = [part.strip() for part in row["EvidencePath"].split(";") if part.strip()]
        if evidence_path not in existing_evidence:
            existing_evidence.append(evidence_path)
        row["EvidencePath"] = "; ".join(existing_evidence)
        note = "Imported as one shared RGBA UI atlas to /Game/Naiwa/P7/VFX/A1; runtime frame playback is presentation-only."
        if note not in row["Notes"]:
            row["Notes"] = (row["Notes"] + " " + note).strip()

    with MANIFEST_PATH.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=entry["fieldnames"], lineterminator="\n")
        writer.writeheader()
        writer.writerows(entry["rows"])


try:
    report["engine"] = unreal.SystemLibrary.get_engine_version()
    prepared = preflight()
    report["preflight"] = {
        "status": "passed",
        "approved_frame_count": 16,
        "already_imported": prepared["already_imported"],
        "texture_dimensions_px": [848, 848],
    }
    unreal.EditorAssetLibrary.make_directory(DEST_ROOT)
    report["assets"] = [import_texture(prepared)]
    update_manifest(prepared)
    report["status"] = "imported_approved_vfx_atlas_runtime_playback_pending"
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    raise
finally:
    EVIDENCE_ROOT.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("NAIWA_P7_A1_VFX_ATLAS_IMPORT " + json.dumps(report, ensure_ascii=False))
