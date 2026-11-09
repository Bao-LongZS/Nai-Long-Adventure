"""Import the latest W4 Boss key-pose batch as a preview-only UE DataTable."""
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
CSV_PATH = P4_SOURCE / "Data/P4_W4_Boss_RuntimePreviewFrames.csv"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/W4/P4_W4_Boss_RuntimePreview_import_report_v01.json"
DEST_ROOT = "/Game/Naiwa/P4/Art/Alpha/W4_Boss/Sequences"
TABLE_PATH = "/Game/Naiwa/P4/Data/DT_P4_W4_Boss_RuntimePreviewFrames"

report = {
    "schema": "naiwa.p4.w4_boss_runtime_preview_import/1",
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

    idle_sequences = manifest.get("boss_sequence_batch", {}).get("sequences", [])
    action_sequences = manifest.get("boss_action_sequence_batch", {}).get("sequences", [])
    sequences = idle_sequences + action_sequences
    if len(idle_sequences) != 3 or len(action_sequences) != 7:
        raise ValueError(f"Expected 3 idle view loops and 7 WIP action sequences, got {len(idle_sequences)} + {len(action_sequences)}")
    if any(item.get("state") == "charge_attack" for item in action_sequences):
        raise ValueError("Refusing to import the rejected charge_attack pose sequence")

    assets_by_file = {asset.get("file"): asset for asset in manifest.get("assets", [])}
    prepared: list[dict] = []
    seen_rows: set[tuple[str, str, int]] = set()
    for sequence in sequences:
        state_id = sequence.get("state")
        view_id = sequence.get("view")
        frame_files = sequence.get("frame_files", [])
        playback_mode = sequence.get("playback_mode")
        if not state_id or view_id not in {"front", "back", "side_right"}:
            raise ValueError(f"Unsupported Boss state/view in {sequence.get('sequence_id')!r}")
        if playback_mode not in {"loop", "once"}:
            raise ValueError(f"Unsupported playback mode for {sequence.get('sequence_id')}")
        if len(frame_files) != sequence.get("frame_count") or not frame_files:
            raise ValueError(f"Frame count mismatch for {sequence.get('sequence_id')}")

        durations = sequence.get("frame_durations_seconds")
        if durations is None:
            duration = sequence.get("frame_duration_seconds")
            durations = [duration] * len(frame_files)
        if len(durations) != len(frame_files):
            raise ValueError(f"Frame duration count mismatch for {sequence.get('sequence_id')}")

        for index, relative_file in enumerate(frame_files):
            entry = assets_by_file.get(relative_file)
            if not entry or entry.get("runtime_ready") is not False:
                raise ValueError(f"Missing WIP asset metadata: {relative_file}")
            if entry.get("sequence_id") and entry.get("sequence_id") != sequence.get("sequence_id"):
                raise ValueError(f"Sequence manifest mismatch: {relative_file}")
            if entry.get("frame_index") != index:
                raise ValueError(f"Frame order/duration mismatch: {relative_file}")
            if entry.get("frame_duration_seconds") is not None and entry.get("frame_duration_seconds") != durations[index]:
                raise ValueError(f"Frame duration metadata mismatch: {relative_file}")
            row_key = (state_id, view_id, index)
            if row_key in seen_rows:
                raise ValueError(f"Duplicate Boss row: {row_key}")
            seen_rows.add(row_key)

            source = (ROOT / relative_file).resolve()
            if not source.is_file():
                raise FileNotFoundError(source)
            actual_hash = sha256_file(source)
            if actual_hash != entry.get("sha256", "").lower():
                raise ValueError(f"Source hash mismatch for {relative_file}: {actual_hash}")
            width, height, color_type = read_png(source)
            if (width, height, color_type) != (512, 512, 6):
                raise ValueError(f"Expected 512x512 RGBA PNG for {relative_file}: {(width, height, color_type)}")
            dimensions = entry.get("dimensions_px") or entry.get("candidate_dimensions_px")
            if dimensions != [width, height] or durations[index] <= 0.0:
                raise ValueError(f"Dimensions or duration invalid for {relative_file}")

            asset_name = f"T_P4_W4_Boss_{state_id}_{view_id}_{index:02d}"
            package_path = f"{DEST_ROOT}/{state_id}_{view_id}/{asset_name}"
            existing = unreal.load_asset(package_path)
            if existing:
                if not isinstance(existing, unreal.Texture2D):
                    raise RuntimeError(f"Destination is not a Texture2D: {package_path}")
                import_data = existing.get_editor_property("asset_import_data")
                existing_source = import_data.get_first_filename() if import_data else ""
                if not existing_source or Path(existing_source).resolve().as_posix().casefold() != source.as_posix().casefold():
                    raise RuntimeError(f"Refusing to overwrite a different or unknown source: {package_path}")
            prepared.append({
                "state_id": state_id,
                "view_id": view_id,
                "frame_index": index,
                "duration_seconds": float(durations[index]),
                "loop": playback_mode == "loop",
                "source": source,
                "sha256": actual_hash,
                "package_path": package_path,
                "asset_name": asset_name,
                "existing": bool(existing),
            })

    if len(prepared) != 26 or len(seen_rows) != 26:
        raise ValueError(f"Expected 26 Boss preview frames, found {len(prepared)}")
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
        "state_id": entry["state_id"],
        "view_id": entry["view_id"],
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
        writer.writerow(["Name", "StateId", "ViewId", "FrameIndex", "DurationSeconds", "bLoop", "FrameTexture"])
        for entry in entries:
            object_ref = f"Texture2D'{entry['package_path']}.{entry['asset_name']}'"
            row_name = f"{entry['state_id']}_{entry['view_id']}_{entry['frame_index']:02d}"
            writer.writerow([
                row_name,
                entry["state_id"],
                entry["view_id"],
                entry["frame_index"],
                f"{entry['duration_seconds']:.6f}",
                "true" if entry["loop"] else "false",
                object_ref,
            ])

    factory = unreal.CSVImportFactory()
    settings = factory.get_editor_property("automated_import_settings")
    settings.set_editor_property("import_row_struct", unreal.NaiwaP4BossFrameRow.static_struct())
    factory.set_editor_property("automated_import_settings", settings)
    task = unreal.AssetImportTask()
    task.set_editor_properties({
        "filename": str(CSV_PATH),
        "destination_path": "/Game/Naiwa/P4/Data",
        "destination_name": "DT_P4_W4_Boss_RuntimePreviewFrames",
        "replace_existing": True,
        "automated": True,
        "save": True,
        "factory": factory,
    })
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    table = unreal.load_asset(TABLE_PATH)
    if not table:
        raise RuntimeError(f"Boss frame DataTable import did not create {TABLE_PATH}")
    actual_struct = unreal.DataTableFunctionLibrary.get_data_table_row_struct(table)
    expected_struct = unreal.NaiwaP4BossFrameRow.static_struct()
    if not actual_struct or actual_struct.get_path_name() != expected_struct.get_path_name():
        raise RuntimeError(f"Unexpected Boss DataTable row struct: {actual_struct}")
    row_names = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    if len(row_names) != len(entries):
        raise RuntimeError(f"Expected 26 Boss frame rows, found {len(row_names)}")
    if not unreal.EditorAssetLibrary.save_asset(TABLE_PATH, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save Boss frame DataTable: {TABLE_PATH}")
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
        "idle_sequence_count": 3,
        "action_sequence_count": 7,
        "frame_count": len(prepared),
        "dimensions_px": [512, 512],
        "existing_same_source_assets": sum(entry["existing"] for entry in prepared),
        "charge_attack_imported": False,
    }
    report["assets"] = [import_texture(entry) for entry in prepared]
    report["frame_table_result"] = import_frame_table(prepared)
    report["status"] = "imported_preview_only_pending_runtime_review"
    report["limitations"] = [
        "Boss animation candidates use transform/timing previews and adjacent key poses; they are not production animation sequences.",
        "The rejected charge_attack state was excluded; the runtime uses move art while the simulated Boss charges.",
        "This report does not update the global source-manifest ue_imported or runtime_ready flags.",
        "Texture import settings were read back after save; gameplay-state presentation requires a separate runtime review.",
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
    unreal.log("NAIWA_P4_W4_BOSS_RUNTIME_PREVIEW_IMPORT " + json.dumps(report, ensure_ascii=False))
