"""Import the user's selected P7 result badge v02 atlas and update its manifest."""
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
ASSET_ROOT = SOURCE_ROOT / "UI/A1"
MANIFEST_ROOT = SOURCE_ROOT / "Manifests/A1"
V02_MANIFEST = MANIFEST_ROOT / "P7_A1_ResultBadgeAssets_v02.csv"
V03_MANIFEST = MANIFEST_ROOT / "P7_A1_ResultBadgeAssets_v03.csv"
SVG_PATH = ASSET_ROOT / "P7_A1_ResultBadgeAtlas_v02.svg"
PNG_PATH = ASSET_ROOT / "P7_A1_ResultBadgeAtlas_v02.png"
DESTINATION = "/Game/Naiwa/P7/UI/A1/T_P7_A1_ResultBadgeAtlas_v02"
EVIDENCE_ROOT = ROOT / "UE5/Records/p7_td/evidence"
REPORT_PATH = EVIDENCE_ROOT / (
    "P7_A1_SelectedResultBadgeV02Import_" + datetime.now().strftime("%Y-%m-%d_%H-%M-%S") + ".json"
)

report = {
    "status": "running",
    "engine": None,
    "selection": "User selected original result badge v02 over v03 on 2026-11-12",
    "scope": "Import the v02 victory/loss atlas and hook its two cells to the P7 result panel; retain v03 as unselected history",
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
        data = stream.read(26)
    if len(data) != 26 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise ValueError(f"Invalid PNG header: {path}")
    width, height = struct.unpack(">II", data[16:24])
    return width, height, data[24], data[25]


def read_manifest(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames:
            raise ValueError(f"Manifest has no header: {path}")
        return list(reader.fieldnames), list(reader)


def write_manifest(path: Path, fields: list[str], rows: list[dict[str, str]]) -> None:
    with path.open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def same_path(first: str, second: Path) -> bool:
    if not first:
        return False
    return Path(first).resolve().as_posix().casefold() == second.resolve().as_posix().casefold()


def import_texture(source: Path, expected_hash: str) -> dict:
    destination = unreal.load_asset(DESTINATION)
    action = "reused_same_source"
    if destination:
        if not isinstance(destination, unreal.Texture2D):
            raise TypeError(f"Destination exists but is not Texture2D: {DESTINATION}")
        data = destination.get_editor_property("asset_import_data")
        imported = data.get_first_filename() if data else ""
        if not same_path(imported, source):
            raise RuntimeError(f"Refusing to replace unrelated asset: {DESTINATION}")
    else:
        unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P7/UI/A1")
        task = unreal.AssetImportTask()
        task.set_editor_properties({
            "filename": str(source),
            "destination_path": "/Game/Naiwa/P7/UI/A1",
            "destination_name": "T_P7_A1_ResultBadgeAtlas_v02",
            "replace_existing": False,
            "automated": True,
            "save": True,
        })
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        destination = unreal.load_asset(DESTINATION)
        if not destination:
            raise RuntimeError(f"Atlas import created no asset at {DESTINATION}")
        action = "imported"
    if not isinstance(destination, unreal.Texture2D):
        raise TypeError(f"Imported object is not Texture2D: {DESTINATION}")

    settings = {
        "srgb": True,
        "compression_settings": unreal.TextureCompressionSettings.TC_DEFAULT,
        "compression_no_alpha": False,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
        "never_stream": True,
    }
    destination.set_editor_properties(settings)
    for key, expected in settings.items():
        if destination.get_editor_property(key) != expected:
            raise RuntimeError(f"Texture setting readback failed: {key}")
    if not unreal.EditorAssetLibrary.save_asset(DESTINATION, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save Texture2D: {DESTINATION}")
    data = destination.get_editor_property("asset_import_data")
    imported = data.get_first_filename() if data else ""
    if not same_path(imported, source) or sha256_file(source) != expected_hash:
        raise RuntimeError("Source path or hash readback failed after import")
    return {
        "asset_id": "P7-UI-A1-RESULT-ATLAS",
        "version": "v02",
        "asset_path": DESTINATION,
        "source_path": str(source),
        "source_sha256": expected_hash,
        "dimensions_px": [832, 416],
        "action": action,
        "texture_group": "UI",
        "mipmaps": False,
        "srgb": True,
        "alpha": True,
        "texture_settings_readback": "passed",
    }


try:
    unreal.log("P7 selected result badge v02 import started")
    report["engine"] = unreal.SystemLibrary.get_engine_version()
    for path in (SVG_PATH, PNG_PATH, V02_MANIFEST, V03_MANIFEST):
        if not path.is_file() or not path.resolve().is_relative_to(SOURCE_ROOT.resolve()):
            raise FileNotFoundError(path)
    v02_fields, v02_rows = read_manifest(V02_MANIFEST)
    v03_fields, v03_rows = read_manifest(V03_MANIFEST)
    required = {"AssetId", "Version", "SourceHash", "ExportHash", "ExportPath", "ReviewStatus", "EvidencePath"}
    if not required.issubset(v02_fields):
        raise ValueError(f"v02 manifest missing fields: {sorted(required - set(v02_fields))}")
    expected_hashes = {"SourceHash": sha256_file(SVG_PATH), "ExportHash": sha256_file(PNG_PATH)}
    width, height, depth, color_type = png_metadata(PNG_PATH)
    if (width, height, depth, color_type) != (832, 416, 8, 6):
        raise ValueError(f"Unexpected result badge atlas metadata: {(width, height, depth, color_type)}")
    for key, expected in expected_hashes.items():
        if any(row[key].lower() != expected for row in v02_rows):
            raise ValueError(f"v02 manifest hash mismatch: {key}")
    if len(v02_rows) != 2 or {row["AssetId"] for row in v02_rows} != {
        "P7-UI-A1-RESULT-WIN", "P7-UI-A1-RESULT-LOSS"
    }:
        raise ValueError("v02 result badge manifest must cover victory and loss cells")
    report["assets"].append(import_texture(PNG_PATH, expected_hashes["ExportHash"]))

    evidence = "UE5/Records/p7_td/evidence/" + REPORT_PATH.name
    for row in v02_rows:
        row["ReviewStatus"] = "User-selected v02 candidate; UE Texture2D imported; victory/loss panel hooked up; viewport review pending"
        row["EvidencePath"] = evidence
    write_manifest(V02_MANIFEST, v02_fields, v02_rows)
    for row in v03_rows:
        row["ReviewStatus"] = "Not selected by user on 2026-11-12; v02 chosen; v03 retained as unselected alternative"
        row["EvidencePath"] = evidence
    write_manifest(V03_MANIFEST, v03_fields, v03_rows)
    report["status"] = "imported_and_wired_user_selected_result_badge_v02"
    report["source_sha256"] = expected_hashes["SourceHash"]
    report["export_sha256"] = expected_hashes["ExportHash"]
    unreal.log("P7 selected result badge v02 import completed")
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    raise
finally:
    EVIDENCE_ROOT.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("NAIWA_P7_SELECTED_RESULT_BADGE_V02_IMPORT " + json.dumps(report, ensure_ascii=False))
