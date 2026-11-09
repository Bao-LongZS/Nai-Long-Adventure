"""Import W4 transform-only VFX frames as isolated A4 preview assets.

The v06 source manifest remains WIP and is never promoted by this script.
"""
from __future__ import annotations

import csv
import hashlib
import json
import struct
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
P4_SOURCE = ROOT / "UE5/SourceAssets/P4"
MANIFEST_PATH = P4_SOURCE / "Manifests/P4_W4_BossAndVFX_manifest_v06.json"
CSV_PATH = P4_SOURCE / "Data/P4_W4_VFX_RuntimePreviewFrames.csv"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/W4/P4_W4_VFX_RuntimePreview_import_report_v01.json"
DEST_ROOT = "/Game/Naiwa/P4/Art/Alpha/W4_FX/Sequences"
TABLE_PATH = "/Game/Naiwa/P4/Data/DT_P4_W4_VFX_RuntimePreviewFrames"

report = {
    "schema": "naiwa.p4.w4_vfx_runtime_preview_import/1",
    "status": "running",
    "source_manifest": str(MANIFEST_PATH),
    "destination_root": DEST_ROOT,
    "frame_table": TABLE_PATH,
    "engine": unreal.SystemLibrary.get_engine_version(),
    "source_manifest_promoted": False,
    "level_opened_or_changed": False,
    "runtime_played": False,
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_png(path: Path) -> tuple[int, int, int]:
    with path.open("rb") as stream:
        header = stream.read(26)
    if len(header) != 26 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"Invalid PNG header: {path}")
    width, height = struct.unpack(">II", header[16:24])
    return width, height, header[25]


def preflight(manifest: dict) -> list[dict]:
    if str(manifest.get("schema_version")) not in {"1", "1.0"}:
        raise ValueError(f"Unexpected manifest schema version: {manifest.get('schema_version')!r}")
    constraints = manifest.get("global_constraints", {})
    if constraints.get("runtime_ready") is not False or constraints.get("ue_imported") is not False:
        raise ValueError("Expected the source package to remain explicitly WIP/unimported")
    batch = manifest.get("vfx_sequence_batch", {})
    if batch.get("status") != "transform_timing_preview_wip":
        raise ValueError(f"Unexpected WIP batch status: {batch.get('status')!r}")
    sequences = batch.get("sequences", [])
    assets_by_id = {asset.get("asset_id"): asset for asset in manifest.get("assets", [])}
    if len(sequences) != 13 or sum(item.get("frame_count", 0) for item in sequences) != 57:
        raise ValueError("Expected exactly 13 preview sequences and 57 frames")

    prepared = []
    seen_ids: set[str] = set()
    for sequence in sequences:
        effect_id = sequence.get("effect_id", "")
        frame_files = sequence.get("frame_files", [])
        if not effect_id.startswith("fx_") or effect_id in seen_ids:
            raise ValueError(f"Unexpected or duplicate effect id: {effect_id!r}")
        seen_ids.add(effect_id)
        if len(frame_files) != sequence.get("frame_count") or not frame_files:
            raise ValueError(f"Frame count mismatch for {effect_id}")
        durations = [asset for asset in assets_by_id.values() if asset.get("sequence_id") == sequence["sequence_id"]]
        if len(durations) != len(frame_files):
            raise ValueError(f"Manifest asset coverage mismatch for {effect_id}")
        by_file = {asset.get("file"): asset for asset in durations}
        loop = sequence.get("playback_mode") == "loop"
        if sequence.get("playback_mode") not in {"loop", "once"}:
            raise ValueError(f"Unsupported playback mode for {effect_id}")

        for frame_index, relative_file in enumerate(frame_files):
            entry = by_file.get(relative_file)
            if not entry or entry.get("frame_index") != frame_index:
                raise ValueError(f"Missing or unordered frame metadata: {relative_file}")
            source = (ROOT / relative_file).resolve()
            if not source.is_file():
                raise FileNotFoundError(source)
            actual_hash = sha256_file(source)
            if actual_hash != entry.get("sha256", "").lower():
                raise ValueError(f"Source hash mismatch for {relative_file}: {actual_hash}")
            width, height, color_type = read_png(source)
            if (width, height, color_type) != (512, 512, 6):
                raise ValueError(f"Expected 512x512 RGBA PNG for {relative_file}: {(width, height, color_type)}")
            if entry.get("dimensions_px") != [width, height]:
                raise ValueError(f"Manifest dimensions mismatch for {relative_file}")
            if entry.get("frame_duration_seconds", 0.0) <= 0.0:
                raise ValueError(f"Invalid duration for {relative_file}")

            filename = f"T_P4_W4_FX_{effect_id}_{frame_index:02d}"
            package_path = f"{DEST_ROOT}/{effect_id}/{filename}"
            existing = unreal.load_asset(package_path)
            if existing:
                if not isinstance(existing, unreal.Texture2D):
                    raise RuntimeError(f"Destination is not a Texture2D: {package_path}")
                import_data = existing.get_editor_property("asset_import_data")
                existing_source = import_data.get_first_filename() if import_data else ""
                if not existing_source or Path(existing_source).resolve().as_posix().casefold() != source.as_posix().casefold():
                    raise RuntimeError(f"Refusing to overwrite a different or unknown source: {package_path}")
            prepared.append({
                "effect_id": effect_id,
                "frame_index": frame_index,
                "duration_seconds": entry["frame_duration_seconds"],
                "loop": loop,
                "source": source,
                "sha256": actual_hash,
                "package_path": package_path,
                "asset_name": filename,
                "existing": bool(existing),
            })

    if len(prepared) != 57 or len(seen_ids) != 13:
        raise ValueError(f"Unexpected prepared coverage: {len(prepared)} frames, {len(seen_ids)} effects")
    return prepared


def import_texture(entry: dict) -> dict:
    texture = unreal.load_asset(entry["package_path"])
    imported_now = False
    if not texture:
        destination = entry["package_path"].rsplit("/", 1)[0]
        unreal.EditorAssetLibrary.make_directory(destination)
        task = unreal.AssetImportTask()
        task.set_editor_properties({
            "filename": str(entry["source"]),
            "destination_path": destination,
            "destination_name": entry["asset_name"],
            "replace_existing": False,
            "automated": True,
            "save": False,
        })
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(entry["package_path"])
        if not texture or not isinstance(texture, unreal.Texture2D):
            raise RuntimeError(f"Texture2D import did not create {entry['package_path']}")
        imported_now = True

    expected = {
        "srgb": True,
        "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        "compression_no_alpha": False,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_SHARPEN0,
        "lod_group": unreal.TextureGroup.TEXTUREGROUP_CHARACTER,
        "never_stream": True,
    }
    texture.set_editor_properties(expected)
    for name, value in expected.items():
        if texture.get_editor_property(name) != value:
            raise RuntimeError(f"Texture setting readback failed: {entry['package_path']} property {name}")
    if not unreal.EditorAssetLibrary.save_asset(entry["package_path"], only_if_is_dirty=False):
        raise RuntimeError(f"Could not save Texture2D: {entry['package_path']}")
    return {
        "effect_id": entry["effect_id"],
        "frame_index": entry["frame_index"],
        "asset_path": entry["package_path"],
        "source": str(entry["source"]),
        "source_sha256": entry["sha256"],
        "duration_seconds": entry["duration_seconds"],
        "playback_mode": "loop" if entry["loop"] else "once",
        "result": "imported" if imported_now else "reused_same_source",
        "texture_readback": "passed",
        "settings": {
            "compression": "TC_EditorIcon (uncompressed RGBA8)",
            "mip_generation": "Sharpen0",
            "texture_group": "Character",
            "never_stream": True,
        },
    }


def import_frame_table(entries: list[dict]) -> dict:
    CSV_PATH.parent.mkdir(parents=True, exist_ok=True)
    with CSV_PATH.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(["Name", "EffectId", "FrameIndex", "DurationSeconds", "bLoop", "FrameTexture"])
        for entry in entries:
            object_ref = f"Texture2D'{entry['package_path']}.{entry['asset_name']}'"
            row_name = f"{entry['effect_id']}_{entry['frame_index']:02d}"
            writer.writerow([
                row_name,
                entry["effect_id"],
                entry["frame_index"],
                f"{entry['duration_seconds']:.6f}",
                "true" if entry["loop"] else "false",
                object_ref,
            ])

    factory = unreal.CSVImportFactory()
    settings = factory.get_editor_property("automated_import_settings")
    settings.set_editor_property("import_row_struct", unreal.NaiwaP4VfxFrameRow.static_struct())
    factory.set_editor_property("automated_import_settings", settings)
    task = unreal.AssetImportTask()
    task.set_editor_properties({
        "filename": str(CSV_PATH),
        "destination_path": "/Game/Naiwa/P4/Data",
        "destination_name": "DT_P4_W4_VFX_RuntimePreviewFrames",
        "replace_existing": True,
        "automated": True,
        "save": True,
        "factory": factory,
    })
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    table = unreal.load_asset(TABLE_PATH)
    if not table:
        raise RuntimeError(f"Frame DataTable import did not create {TABLE_PATH}")
    actual_struct = unreal.DataTableFunctionLibrary.get_data_table_row_struct(table)
    expected_struct = unreal.NaiwaP4VfxFrameRow.static_struct()
    if not actual_struct or actual_struct.get_path_name() != expected_struct.get_path_name():
        raise RuntimeError(f"Unexpected VFX DataTable row struct: {actual_struct}")
    row_names = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    if len(row_names) != len(entries):
        raise RuntimeError(f"Expected 57 VFX frame rows, found {len(row_names)}")
    if not unreal.EditorAssetLibrary.save_asset(TABLE_PATH, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save VFX frame DataTable: {TABLE_PATH}")
    return {
        "asset_path": TABLE_PATH,
        "row_struct": actual_struct.get_path_name(),
        "row_count": len(row_names),
        "csv_source": str(CSV_PATH),
    }


def main() -> None:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    prepared = preflight(manifest)
    report["manifest_version"] = manifest.get("version")
    report["preflight"] = {
        "status": "passed",
        "sequence_count": 13,
        "frame_count": len(prepared),
        "dimensions_px": [512, 512],
        "existing_same_source_assets": sum(entry["existing"] for entry in prepared),
    }
    report["assets"] = [import_texture(entry) for entry in prepared]
    report["frame_table_result"] = import_frame_table(prepared)
    report["status"] = "imported_preview_only_pending_runtime_review"
    report["limitations"] = [
        "Frames remain transform/timing preview WIP; no per-frame shape redraw or accessibility redraw variant was added.",
        "This report does not update the global source-manifest ue_imported or runtime_ready flags.",
        "Texture import settings were read back after save; event behavior requires a separate A4 runtime review.",
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
    unreal.log("NAIWA_P4_W4_VFX_RUNTIME_PREVIEW_IMPORT " + json.dumps(report, ensure_ascii=False))

