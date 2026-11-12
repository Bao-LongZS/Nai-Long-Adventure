"""Import the explicitly approved P7 catalog HUD poses and T-junction gate atlas."""
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
CATALOG_MANIFEST = SOURCE_ROOT / "Manifests/A1/P7_A1_CatalogHUDPortraitAssets_v02.csv"
GATE_MANIFEST = SOURCE_ROOT / "Manifests/A1/P7_A1_ForkGateStates_v03.csv"
EVIDENCE_ROOT = ROOT / "UE5/Records/p7_td/evidence"
REPORT_PATH = EVIDENCE_ROOT / (
    "P7_ApprovedCatalogPortraitAndForkGateImport_"
    + datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    + ".json"
)

CATALOG_DEST = "/Game/Naiwa/P7/Characters/A1/CatalogDirect"
GATE_DEST = "/Game/Naiwa/P7/Environment/A1"
PDF_SHA256 = "1b3d6198eb393e735d34c0ac0ccd6fbc8f1cfc5fb7e91847a7028e25057c9361"
CATALOG_SPECS = {
    "P7-CHR-A1-CAT-139": ("P7_A1_CharacterHUD_Idle_Figure139_v02.png", (583, 583), "T_P7_A1_CharacterHUD_Idle_Figure139_v02"),
    "P7-CHR-A1-CAT-079": ("P7_A1_CharacterHUD_Laugh_Figure079_v02.png", (604, 604), "T_P7_A1_CharacterHUD_Laugh_Figure079_v02"),
    "P7-CHR-A1-CAT-121": ("P7_A1_CharacterHUD_Victory_Figure121_v02.png", (604, 604), "T_P7_A1_CharacterHUD_Victory_Figure121_v02"),
    "P7-CHR-A1-CAT-106": ("P7_A1_CharacterHUD_Distress_Figure106_v02.png", (604, 604), "T_P7_A1_CharacterHUD_Distress_Figure106_v02"),
}
GATE_SPECS = {
    "P7-ENV-A1-GATE-A-STRAIGHT": (0, 0),
    "P7-ENV-A1-GATE-B-SPLIT": (984, 1),
    "P7-ENV-A1-GATE-B-MERGE": (1968, 2),
}

report = {
    "status": "running",
    "engine": None,
    "scope": "four user-approved catalog HUD poses and one user-approved T-junction gate atlas; no gameplay data or map asset changed",
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
    return width, height, header[24], header[25]


def read_csv(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames:
            raise ValueError(f"CSV has no header: {path}")
        return list(reader.fieldnames), list(reader)


def write_csv(path: Path, fieldnames: list[str], rows: list[dict[str, str]]) -> None:
    with path.open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def same_path(first: str, second: Path) -> bool:
    if not first:
        return False
    try:
        return Path(first).resolve().as_posix().casefold() == second.resolve().as_posix().casefold()
    except OSError:
        return False


def preflight_catalog() -> tuple[list[str], list[dict[str, str]], list[dict]]:
    fieldnames, rows = read_csv(CATALOG_MANIFEST)
    required = {"AssetId", "Version", "Filename", "SHA256", "Width", "Height", "SourcePDFSHA256", "Status", "ReviewStatus", "UnrealPath", "EvidencePath"}
    if not required.issubset(fieldnames):
        raise ValueError(f"Catalog manifest is missing fields: {sorted(required - set(fieldnames))}")
    imports = []
    for asset_id, (filename, expected_size, object_name) in CATALOG_SPECS.items():
        matches = [row for row in rows if row.get("AssetId") == asset_id and row.get("Version") == "v02"]
        if len(matches) != 1:
            raise ValueError(f"Expected one current catalog row for {asset_id}; found {len(matches)}")
        row = matches[0]
        if row["Filename"] != filename or row["SourcePDFSHA256"].lower() != PDF_SHA256:
            raise ValueError(f"Unexpected catalog source mapping for {asset_id}")
        if not row["ReviewStatus"].startswith("User-approved visual"):
            raise ValueError(f"Refusing to import unapproved catalog pose: {asset_id}")
        source = (SOURCE_ROOT / "Characters/A1/CatalogDirect" / filename).resolve()
        if not source.is_relative_to(SOURCE_ROOT.resolve()) or not source.is_file():
            raise FileNotFoundError(source)
        if sha256_file(source) != row["SHA256"].lower():
            raise ValueError(f"Catalog export hash mismatch: {asset_id}")
        width, height, bit_depth, color_type = png_metadata(source)
        if (width, height) != expected_size or bit_depth != 8 or color_type != 2:
            raise ValueError(f"Unexpected RGB PNG metadata for {asset_id}: {(width, height, bit_depth, color_type)}")
        if int(row["Width"]) != width or int(row["Height"]) != height:
            raise ValueError(f"Catalog manifest dimensions disagree for {asset_id}")
        destination = f"{CATALOG_DEST}/{object_name}"
        if row["UnrealPath"] != destination:
            raise ValueError(f"Unexpected Unreal destination for {asset_id}: {row['UnrealPath']}")
        imports.append({
            "asset_id": asset_id,
            "version": "v02",
            "source": source,
            "hash": sha256_file(source),
            "size": expected_size,
            "destination": destination,
            "manifest_row": row,
            "alpha": False,
        })
    return fieldnames, rows, imports


def preflight_gate() -> tuple[list[str], list[dict[str, str]], dict]:
    fieldnames, rows = read_csv(GATE_MANIFEST)
    by_id = {row.get("AssetId"): row for row in rows}
    if len(by_id) != len(rows) or set(by_id) != set(GATE_SPECS):
        raise ValueError("Gate manifest rows/IDs do not match the approved v03 atlas")
    source = (SOURCE_ROOT / "Environment/A1/P7_A1_ForkGateStates_v03.svg").resolve()
    exported = (SOURCE_ROOT / "Environment/A1/P7_A1_ForkGateStates_v03.png").resolve()
    if not source.is_file() or not exported.is_file():
        raise FileNotFoundError("The v03 gate SVG or PNG export is missing")
    source_hash = sha256_file(source)
    export_hash = sha256_file(exported)
    width, height, bit_depth, color_type = png_metadata(exported)
    if (width, height, bit_depth, color_type) != (2928, 528, 8, 6):
        raise ValueError(f"Unexpected gate atlas PNG metadata: {(width, height, bit_depth, color_type)}")
    imports = []
    for asset_id, (expected_x, frame_index) in GATE_SPECS.items():
        row = by_id[asset_id]
        if not row["ReviewStatus"].startswith("User-approved visual"):
            raise ValueError(f"Refusing to import unapproved gate state: {asset_id}")
        if row["Version"] != "v03" or row["SourceHash"].lower() != source_hash or row["ExportHash"].lower() != export_hash:
            raise ValueError(f"Gate manifest hash/version mismatch: {asset_id}")
        if (int(row["AtlasX"]) != expected_x or int(row["AtlasY"]) != 0 or
                int(row["AtlasW"]) != 960 or int(row["AtlasH"]) != 528 or int(row["FrameIndex"]) != frame_index):
            raise ValueError(f"Gate atlas rectangle mismatch: {asset_id}")
        if row["UnrealPath"] != f"{GATE_DEST}/T_P7_A1_ForkGateStates_v03":
            raise ValueError(f"Unexpected gate Unreal destination: {row['UnrealPath']}")
        imports.append({"asset_id": asset_id, "version": "v03", "manifest_row": row,
                        "source": exported, "hash": export_hash, "size": (2928, 528),
                        "destination": row["UnrealPath"], "alpha": True})
    return fieldnames, rows, {"source_hash": source_hash, "export_hash": export_hash, "imports": imports}


def import_texture(config: dict) -> dict:
    destination = config["destination"]
    texture = unreal.load_asset(destination)
    action = "reused_same_source"
    if texture:
        if not isinstance(texture, unreal.Texture2D):
            raise TypeError(f"Destination exists but is not a Texture2D: {destination}")
        import_data = texture.get_editor_property("asset_import_data")
        imported_source = import_data.get_first_filename() if import_data else ""
        if not same_path(imported_source, config["source"]):
            raise RuntimeError(f"Refusing to replace an unrelated texture: {destination}")
    else:
        unreal.EditorAssetLibrary.make_directory(destination.rsplit("/", 1)[0])
        task = unreal.AssetImportTask()
        task.set_editor_properties({
            "filename": str(config["source"]),
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

    expected_settings = {
        "srgb": True,
        "compression_settings": unreal.TextureCompressionSettings.TC_DEFAULT,
        "compression_no_alpha": not config["alpha"],
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
        "never_stream": True,
    }
    texture.set_editor_properties(expected_settings)
    for name, expected in expected_settings.items():
        if texture.get_editor_property(name) != expected:
            raise RuntimeError(f"Texture setting readback failed: {destination} ({name})")
    if not unreal.EditorAssetLibrary.save_asset(destination, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save imported texture: {destination}")
    import_data = texture.get_editor_property("asset_import_data")
    imported_source = import_data.get_first_filename() if import_data else ""
    if not same_path(imported_source, config["source"]):
        raise RuntimeError(f"Imported source path readback failed: {destination}")
    if sha256_file(config["source"]) != config["hash"]:
        raise RuntimeError(f"Source changed during import: {config['asset_id']}")
    return {
        "asset_id": config["asset_id"], "version": config["version"], "asset_path": destination,
        "source_path": str(config["source"]), "source_sha256": config["hash"],
        "dimensions_px": list(config["size"]), "action": action,
        "texture_group": "UI", "mipmaps": False, "srgb": True,
        "alpha": config["alpha"], "texture_settings_readback": "passed",
    }


try:
    unreal.log("P7 approved catalog portrait and fork gate import started")
    report["engine"] = unreal.SystemLibrary.get_engine_version()
    catalog_fields, catalog_rows, catalog_imports = preflight_catalog()
    gate_fields, gate_rows, gate_data = preflight_gate()
    for config in catalog_imports + gate_data["imports"]:
        report["assets"].append(import_texture(config))

    evidence_path = "UE5/Records/p7_td/evidence/" + REPORT_PATH.name
    catalog_by_id = {row["AssetId"]: row for row in catalog_rows}
    for asset_id, (_, _, object_name) in CATALOG_SPECS.items():
        row = catalog_by_id[asset_id]
        row["Status"] = "用户已通过；UE Texture2D 已导入；HUD 显示接入，运行画面验收待完成"
        row["ReviewStatus"] = "User-approved visual; UE Texture2D imported; P7 HUD hooked up; viewport acceptance pending"
        row["EvidencePath"] = evidence_path
        row["UnrealPath"] = f"{CATALOG_DEST}/{object_name}"
    write_csv(CATALOG_MANIFEST, catalog_fields, catalog_rows)

    for row in gate_rows:
        row["ReviewStatus"] = "User-approved visual; UE Texture2D atlas imported; T-junction states wired; viewport acceptance pending"
        row["EvidencePath"] = evidence_path
        row["Notes"] = "User approved v03; imported as a shared RGBA UI atlas and sliced by Canvas UV at Alpha01 (1,5)/(18,5); runtime viewport acceptance pending"
    write_csv(GATE_MANIFEST, gate_fields, gate_rows)
    report["status"] = "imported_and_wired_approved_catalog_and_gate_art"
    report["catalog_source_pdf_sha256"] = PDF_SHA256
    unreal.log("P7 approved catalog portrait and fork gate import completed")
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    raise
finally:
    EVIDENCE_ROOT.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("NAIWA_P7_APPROVED_CATALOG_GATE_IMPORT " + json.dumps(report, ensure_ascii=False))
