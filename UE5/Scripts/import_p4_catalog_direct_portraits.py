"""Import the user's directly selected P4 appendix-catalog portraits as UI textures."""
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
SOURCE_ROOT = ROOT / "UE5/SourceAssets/P4/UI/Alpha/CatalogDirect"
MANIFEST_PATH = SOURCE_ROOT / "P4_A1_CatalogPortraits_manifest_v01.csv"
EVIDENCE_ROOT = ROOT / "UE5/Records/p4_alpha/evidence/CatalogDirect"
REPORT_PATH = EVIDENCE_ROOT / (
    "P4_A1_CatalogPortraits_Import_"
    + datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    + ".json"
)
CONTENT_ROOT = "/Game/Naiwa/P4/UI/Alpha/CatalogDirect"
PDF_SHA256 = "1b3d6198eb393e735d34c0ac0ccd6fbc8f1cfc5fb7e91847a7028e25057c9361"
SPECS = {
    "P4-A1-CAT-139": ("P4_A1_CatalogPortrait_Idle_Figure139_v01.png", (583, 583), "T_P4_A1_CatalogPortrait_Idle_Figure139_v01"),
    "P4-A1-CAT-003": ("P4_A1_CatalogPortrait_Laugh_Figure003_v01.png", (604, 604), "T_P4_A1_CatalogPortrait_Laugh_Figure003_v01"),
    "P4-A1-CAT-121": ("P4_A1_CatalogPortrait_Victory_Figure121_v01.png", (604, 604), "T_P4_A1_CatalogPortrait_Victory_Figure121_v01"),
    "P4-A1-CAT-106": ("P4_A1_CatalogPortrait_Distress_Figure106_v01.png", (604, 604), "T_P4_A1_CatalogPortrait_Distress_Figure106_v01"),
}

report = {
    "schema": "naiwa.p4.catalog_direct_portrait_import/1",
    "status": "running",
    "engine": None,
    "scope": "four direct appendix-catalog portrait textures only; no HUD layout, gameplay, map, or animation changes",
    "destination_root": CONTENT_ROOT,
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


def read_manifest() -> tuple[list[str], list[dict[str, str]]]:
    with MANIFEST_PATH.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames:
            raise ValueError("Portrait manifest has no header")
        return list(reader.fieldnames), list(reader)


def write_manifest(fields: list[str], rows: list[dict[str, str]]) -> None:
    with MANIFEST_PATH.open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def same_path(first: str, second: Path) -> bool:
    if not first:
        return False
    try:
        return Path(first).resolve().as_posix().casefold() == second.resolve().as_posix().casefold()
    except OSError:
        return False


def preflight() -> tuple[list[str], list[dict[str, str]], list[dict]]:
    fields, rows = read_manifest()
    if len(rows) != len(SPECS):
        raise ValueError(f"Expected {len(SPECS)} portrait rows, found {len(rows)}")
    by_id = {row.get("AssetId"): row for row in rows}
    if len(by_id) != len(rows) or set(by_id) != set(SPECS):
        raise ValueError("Portrait manifest rows do not match the four selected catalog figures")

    imports = []
    for asset_id, (filename, expected_size, object_name) in SPECS.items():
        row = by_id[asset_id]
        if row.get("Filename") != filename or row.get("SourcePDFSHA256", "").lower() != PDF_SHA256:
            raise ValueError(f"Unexpected source mapping for {asset_id}")
        if row.get("RuntimeUse", "") == "":
            raise ValueError(f"Missing UI-only use description for {asset_id}")
        source = (SOURCE_ROOT / filename).resolve()
        if not source.is_relative_to(SOURCE_ROOT.resolve()) or not source.is_file():
            raise FileNotFoundError(source)
        actual_hash = sha256_file(source)
        if actual_hash != row.get("SHA256", "").lower():
            raise ValueError(f"Portrait source hash mismatch for {asset_id}")
        width, height, bit_depth, color_type = png_metadata(source)
        if (width, height) != expected_size or bit_depth != 8 or color_type != 2:
            raise ValueError(f"Expected {expected_size} 8-bit RGB PNG for {asset_id}; got {(width, height, bit_depth, color_type)}")
        if (int(row["Width"]), int(row["Height"])) != expected_size:
            raise ValueError(f"Manifest dimensions disagree for {asset_id}")
        destination = f"{CONTENT_ROOT}/{object_name}"
        if row.get("UnrealPath") != destination:
            raise ValueError(f"Unexpected UE path for {asset_id}: {row.get('UnrealPath')}")

        texture = unreal.load_asset(destination)
        imported_source = ""
        if texture:
            if not isinstance(texture, unreal.Texture2D):
                raise TypeError(f"Destination exists but is not a Texture2D: {destination}")
            import_data = texture.get_editor_property("asset_import_data")
            imported_source = import_data.get_first_filename() if import_data else ""
            if not same_path(imported_source, source):
                raise RuntimeError(f"Refusing to overwrite an unrelated texture: {destination}")

        imports.append({
            "asset_id": asset_id,
            "figure": row["FigureNo"],
            "caption": row["FigureCaption"],
            "source": source,
            "source_sha256": actual_hash,
            "dimensions_px": [width, height],
            "destination": destination,
            "existing": bool(texture),
        })
    return fields, rows, imports


def import_texture(config: dict) -> dict:
    destination = config["destination"]
    texture = unreal.load_asset(destination)
    action = "reused_same_source"
    if not texture:
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
        "compression_no_alpha": True,
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
    if sha256_file(config["source"]) != config["source_sha256"]:
        raise RuntimeError(f"Source changed during import: {config['asset_id']}")
    return {
        "asset_id": config["asset_id"],
        "figure": config["figure"],
        "caption": config["caption"],
        "asset_path": destination,
        "source_path": str(config["source"]),
        "source_sha256": config["source_sha256"],
        "dimensions_px": config["dimensions_px"],
        "action": action,
        "texture_group": "UI",
        "mipmaps": False,
        "srgb": True,
        "alpha": False,
        "texture_settings_readback": "passed",
    }


try:
    unreal.log("P4 direct appendix-catalog portrait import started")
    report["engine"] = unreal.SystemLibrary.get_engine_version()
    fields, rows, imports = preflight()
    for config in imports:
        report["assets"].append(import_texture(config))

    evidence_path = REPORT_PATH.relative_to(ROOT).as_posix()
    for row in rows:
        row["Status"] = "UE Texture2D imported; static UI portrait only; HUD/runtime visual review pending"
        row["ReviewStatus"] = "User-directed catalog art; UI texture imported; HUD hookup and 720p/1080p viewport review pending"
        row["ImportReport"] = evidence_path
    write_manifest(fields, rows)
    report["status"] = "textures_imported_pending_hud_layout_and_runtime_review"
    report["catalog_source_pdf_sha256"] = PDF_SHA256
    unreal.log("P4 direct appendix-catalog portrait import completed")
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    raise
finally:
    EVIDENCE_ROOT.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("NAIWA_P4_CATALOG_DIRECT_PORTRAIT_IMPORT " + json.dumps(report, ensure_ascii=False))
