"""Import owner-approved P4 A1 cleaned frames as independent Texture2D assets.

This script never opens, creates, or switches a level. It is intentionally
gated by the import manifest's exact approval status and preflights every
source image and destination before making any Unreal asset changes.
"""
from __future__ import annotations

import hashlib
import json
import re
import struct
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = ROOT / "UE5/SourceAssets/P4/Characters/Alpha/A1"
MANIFEST_PATH = ROOT / "UE5/SourceAssets/P4/Manifests/P4_A1_UE_Import_manifest.json"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/A1/P4_A1_clean_frame_import_report.json"
DEST_ROOT = "/Game/Naiwa/P4/Art/Alpha/A1/Frames"
APPROVED_STATUS = "owner_approved_for_ue_import"
EXPECTED_SCHEMA = "naiwa.p4.alpha_ue_import/1"
ID_PATTERN = re.compile(r"^[a-z0-9]+(?:_[a-z0-9]+)*$")

report = {
    "status": "running",
    "manifest": str(MANIFEST_PATH),
    "destination_root": DEST_ROOT,
    "engine": unreal.SystemLibrary.get_engine_version(),
    "level_opened_or_changed": False,
    "frames": [],
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def png_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"Not a valid PNG with an IHDR header: {path}")
    return struct.unpack(">II", header[16:24])


def contained_source_path(relative_path: str) -> Path:
    candidate = (SOURCE_DIR / relative_path).resolve()
    try:
        candidate.relative_to(SOURCE_DIR.resolve())
    except ValueError as exc:
        raise ValueError(f"Manifest source escapes the A1 source folder: {relative_path}") from exc
    if not candidate.is_file():
        raise FileNotFoundError(f"Missing cleaned A1 frame: {candidate}")
    return candidate


def normalized_path(value: str) -> str:
    return str(Path(value).resolve()).casefold().replace("/", "\\")


def build_preflight(manifest: dict) -> list[dict]:
    if manifest.get("schema") != EXPECTED_SCHEMA:
        raise ValueError(f"Unexpected A1 import manifest schema: {manifest.get('schema')!r}")
    if manifest.get("source_manifest") != "UE5/SourceAssets/P4/Manifests/P4_A1_Animation_manifest.json":
        raise ValueError("Unexpected source_manifest in A1 UE import manifest")

    frames = manifest.get("frames")
    expected_count = manifest.get("source_cells")
    if not isinstance(frames, list) or not frames or len(frames) != expected_count:
        raise ValueError("Manifest frame count does not match source_cells")
    if manifest.get("cleaned_cells") != len(frames):
        raise ValueError("Manifest cleaned_cells does not match frames")

    texture_import = manifest.get("texture_import", {})
    required_settings = {
        "color_space": "sRGB",
        "alpha": "straight alpha retained",
        "texture_group": "Character",
        "mip_generation": "Sharpen0",
    }
    for key, expected in required_settings.items():
        if texture_import.get(key) != expected:
            raise ValueError(f"Unexpected texture_import.{key}: {texture_import.get(key)!r}")
    expected_dimensions = tuple(manifest.get("atlas_layout", {}).get("cell_size_px", ()))
    if len(expected_dimensions) != 2 or any(not isinstance(value, int) or value <= 0 for value in expected_dimensions):
        raise ValueError("Manifest atlas_layout.cell_size_px must contain two positive integers")

    seen_ids: set[str] = set()
    prepared = []
    common_dimensions = None
    actions = set()
    for frame in frames:
        frame_id = frame.get("id", "")
        action = frame.get("action", "")
        if not ID_PATTERN.fullmatch(frame_id) or not ID_PATTERN.fullmatch(action):
            raise ValueError(f"Unsafe frame id/action in manifest: {frame_id!r}/{action!r}")
        if frame_id in seen_ids:
            raise ValueError(f"Duplicate frame id in manifest: {frame_id}")
        seen_ids.add(frame_id)
        actions.add(action)

        source = contained_source_path(frame.get("clean_file", ""))
        actual_hash = sha256_file(source)
        expected_hash = frame.get("clean_sha256", "").lower()
        if not re.fullmatch(r"[0-9a-f]{64}", expected_hash) or actual_hash != expected_hash:
            raise ValueError(f"Clean frame SHA-256 mismatch for {frame_id}: {actual_hash}")

        dimensions = png_dimensions(source)
        if dimensions != expected_dimensions:
            raise ValueError(
                f"Unexpected cleaned-frame dimensions for {frame_id}: "
                f"{dimensions}, expected {expected_dimensions}"
            )
        if common_dimensions is None:
            common_dimensions = dimensions
        elif dimensions != common_dimensions:
            raise ValueError(f"Frame dimensions differ for {frame_id}: {dimensions} vs {common_dimensions}")

        asset_name = f"T_P4_A1_{frame_id}"
        destination_path = f"{DEST_ROOT}/{action}"
        asset_path = f"{destination_path}/{asset_name}"
        existing = unreal.load_asset(asset_path)
        existing_source = None
        if existing:
            if not isinstance(existing, unreal.Texture2D):
                raise RuntimeError(f"Destination exists but is not a Texture2D: {asset_path}")
            import_data = existing.get_editor_property("asset_import_data")
            existing_source = import_data.get_first_filename() if import_data else ""
            if not existing_source or normalized_path(existing_source) != normalized_path(str(source)):
                raise RuntimeError(
                    f"Refusing to overwrite an asset with a different or unknown source: {asset_path}"
                )

        prepared.append({
            "id": frame_id,
            "action": action,
            "source": source,
            "source_sha256": actual_hash,
            "dimensions_px": list(dimensions),
            "asset_name": asset_name,
            "destination_path": destination_path,
            "asset_path": asset_path,
            "existing": bool(existing),
            "existing_source": existing_source,
        })

    if len(seen_ids) != expected_count:
        raise ValueError("Manifest frame IDs are not unique")
    if manifest.get("action_atlases") != len(actions):
        raise ValueError("Unique action count does not match action_atlases")
    return prepared


def configure_and_save(frame: dict, texture) -> None:
    texture.set_editor_properties({
        "srgb": True,
        "compression_settings": unreal.TextureCompressionSettings.TC_DEFAULT,
        "compression_no_alpha": False,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_SHARPEN0,
        "lod_group": unreal.TextureGroup.TEXTUREGROUP_CHARACTER,
    })
    expected = {
        "srgb": True,
        "compression_settings": unreal.TextureCompressionSettings.TC_DEFAULT,
        "compression_no_alpha": False,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_SHARPEN0,
        "lod_group": unreal.TextureGroup.TEXTUREGROUP_CHARACTER,
    }
    for property_name, value in expected.items():
        actual = texture.get_editor_property(property_name)
        if actual != value:
            raise RuntimeError(
                f"Texture setting readback failed for {frame['asset_path']}: "
                f"{property_name}={actual!r}, expected {value!r}"
            )
    if not unreal.EditorAssetLibrary.save_asset(frame["asset_path"], only_if_is_dirty=False):
        raise RuntimeError(f"Could not save imported texture: {frame['asset_path']}")


def import_one(frame: dict) -> dict:
    texture = unreal.load_asset(frame["asset_path"])
    imported_now = False
    if not texture:
        unreal.EditorAssetLibrary.make_directory(frame["destination_path"])
        task = unreal.AssetImportTask()
        task.set_editor_properties({
            "filename": str(frame["source"]),
            "destination_path": frame["destination_path"],
            "destination_name": frame["asset_name"],
            "replace_existing": False,
            "automated": True,
            "save": False,
        })
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(frame["asset_path"])
        if not texture:
            raise RuntimeError(f"Import did not create {frame['asset_path']}")
        imported_now = True

    configure_and_save(frame, texture)
    return {
        "id": frame["id"],
        "asset_path": frame["asset_path"],
        "source_sha256": frame["source_sha256"],
        "dimensions_px": frame["dimensions_px"],
        "result": "imported" if imported_now else "reused_same_source",
        "texture_readback": "passed",
    }


def main() -> None:
    if not MANIFEST_PATH.is_file():
        raise FileNotFoundError(f"Missing A1 UE import manifest: {MANIFEST_PATH}")
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    report["manifest_status"] = manifest.get("status")
    if manifest.get("status") != APPROVED_STATUS:
        report.update({
            "status": "blocked_pending_owner_approval",
            "required_manifest_status": APPROVED_STATUS,
            "message": "No UE assets changed. Review the cleaned-frame differences, then explicitly approve in the manifest before rerunning.",
        })
        unreal.log_warning("NAIWA_P4_A1_IMPORT_BLOCKED " + json.dumps(report, ensure_ascii=False))
        return

    prepared = build_preflight(manifest)
    report["preflight"] = {
        "status": "passed",
        "frame_count": len(prepared),
        "actions": sorted({frame["action"] for frame in prepared}),
        "dimensions_px": prepared[0]["dimensions_px"],
        "existing_same_source_assets": sum(frame["existing"] for frame in prepared),
    }

    for frame in prepared:
        report["frames"].append(import_one(frame))
    report.update({
        "status": "imported_pending_visual_calibration",
        "imported_count": sum(frame["result"] == "imported" for frame in report["frames"]),
        "reused_count": sum(frame["result"] == "reused_same_source" for frame in report["frames"]),
        "limitations": [
            "Texture import and property readback do not verify visible sprite sampling or character calibration.",
            "The script did not open or switch a level and did not run PIE/Standalone.",
        ],
    })


try:
    main()
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    raise
finally:
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("NAIWA_P4_A1_FRAME_IMPORT " + json.dumps(report, ensure_ascii=False))
