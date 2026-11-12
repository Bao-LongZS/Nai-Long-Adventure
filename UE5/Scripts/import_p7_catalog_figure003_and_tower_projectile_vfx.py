"""Import the user-requested standing laugh replacement and Dog/Mouse projectile atlas."""
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
CATALOG_V03 = SOURCE_ROOT / "Manifests/A1/P7_A1_CatalogHUDPortraitAssets_v03.csv"
CATALOG_V02 = SOURCE_ROOT / "Manifests/A1/P7_A1_CatalogHUDPortraitAssets_v02.csv"
PROJECTILE_MANIFEST = SOURCE_ROOT / "Manifests/A1/P7_A1_TowerProjectileVFXAssets_v01.csv"
EVIDENCE_ROOT = ROOT / "UE5/Records/p7_td/evidence"
REPORT_PATH = EVIDENCE_ROOT / (
    "P7_A1_Catalog003AndTowerProjectileVFXImport_"
    + datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    + ".json"
)
CATALOG_DEST = "/Game/Naiwa/P7/Characters/A1/CatalogDirect"
PROJECTILE_DEST = "/Game/Naiwa/P7/VFX/A1/T_P7_A1_TowerProjectileVFX_v01"
PDF_HASH = "1b3d6198eb393e735d34c0ac0ccd6fbc8f1cfc5fb7e91847a7028e25057c9361"
CATALOG_SPECS = {
    "P7-CHR-A1-CAT-139": ("v02", "P7_A1_CharacterHUD_Idle_Figure139_v02.png", (583, 583), "T_P7_A1_CharacterHUD_Idle_Figure139_v02"),
    "P7-CHR-A1-CAT-003": ("v03", "P7_A1_CharacterHUD_Laugh_Figure003_v03.png", (604, 604), "T_P7_A1_CharacterHUD_Laugh_Figure003_v03"),
    "P7-CHR-A1-CAT-121": ("v02", "P7_A1_CharacterHUD_Victory_Figure121_v02.png", (604, 604), "T_P7_A1_CharacterHUD_Victory_Figure121_v02"),
    "P7-CHR-A1-CAT-106": ("v02", "P7_A1_CharacterHUD_Distress_Figure106_v02.png", (604, 604), "T_P7_A1_CharacterHUD_Distress_Figure106_v02"),
}
EXPECTED_PROJECTILE_CELLS = {
    (16 + column * 208, 16, "P7_Tower_MilkDog", column) for column in range(4)
} | {
    (16 + column * 208, 224, "P7_Tower_MilkMouse", column) for column in range(4)
}

report = {
    "status": "running",
    "engine": None,
    "scope": "user-requested Figure003 standing laugh replacement and independent MilkDog/MilkMouse projectile VFX; existing v07 shot remains MilkChicken; no gameplay data or level changed",
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
    fields, rows = read_csv(CATALOG_V03)
    required = {"AssetId", "Version", "Filename", "SHA256", "Width", "Height", "SourcePDFSHA256", "ReviewStatus", "UnrealPath", "EvidencePath"}
    if not required.issubset(fields):
        raise ValueError(f"Catalog manifest missing fields: {sorted(required - set(fields))}")
    imports = []
    for asset_id, (version, filename, expected_size, object_name) in CATALOG_SPECS.items():
        matches = [row for row in rows if row.get("AssetId") == asset_id and row.get("Version") == version]
        if len(matches) != 1:
            raise ValueError(f"Expected one current catalog row for {asset_id}/{version}; found {len(matches)}")
        row = matches[0]
        allowed_status = row["ReviewStatus"].startswith("User-approved visual") or row["ReviewStatus"].startswith("User-directed replacement")
        if not allowed_status or row["SourcePDFSHA256"].lower() != PDF_HASH or row["Filename"] != filename:
            raise ValueError(f"Catalog row is not the requested figure mapping: {asset_id}")
        source = (SOURCE_ROOT / "Characters/A1/CatalogDirect" / filename).resolve()
        if not source.is_relative_to(SOURCE_ROOT.resolve()) or not source.is_file():
            raise FileNotFoundError(source)
        if sha256_file(source) != row["SHA256"].lower():
            raise ValueError(f"Catalog export hash mismatch: {asset_id}")
        width, height, depth, color_type = png_metadata(source)
        if (width, height, depth, color_type) != (*expected_size, 8, 2):
            raise ValueError(f"Unexpected RGB PNG metadata for {asset_id}: {(width, height, depth, color_type)}")
        if (int(row["Width"]), int(row["Height"])) != (width, height):
            raise ValueError(f"Catalog manifest dimensions disagree: {asset_id}")
        destination = f"{CATALOG_DEST}/{object_name}"
        if row["UnrealPath"] != destination:
            raise ValueError(f"Unexpected catalog destination for {asset_id}: {row['UnrealPath']}")
        imports.append({"asset_id": asset_id, "version": version, "source": source,
                        "hash": sha256_file(source), "size": expected_size,
                        "destination": destination, "alpha": False, "row": row})
    return fields, rows, imports


def preflight_projectile_atlas() -> tuple[list[str], list[dict[str, str]], dict]:
    fields, rows = read_csv(PROJECTILE_MANIFEST)
    if len(rows) != 8:
        raise ValueError(f"Expected eight Dog/Mouse projectile frames; found {len(rows)}")
    source = (SOURCE_ROOT / "VFX/A1/P7_A1_TowerProjectileVFX_v01.svg").resolve()
    exported = (SOURCE_ROOT / "VFX/A1/P7_A1_TowerProjectileVFX_v01.png").resolve()
    for file in (source, exported):
        if not file.is_relative_to(SOURCE_ROOT.resolve()) or not file.is_file():
            raise FileNotFoundError(file)
    source_hash, export_hash = sha256_file(source), sha256_file(exported)
    width, height, depth, color_type = png_metadata(exported)
    if (width, height, depth, color_type) != (848, 432, 8, 6):
        raise ValueError(f"Unexpected projectile atlas PNG metadata: {(width, height, depth, color_type)}")
    cells = set()
    ids = set()
    for row in rows:
        if row["AssetId"] in ids:
            raise ValueError(f"Duplicate projectile AssetId: {row['AssetId']}")
        ids.add(row["AssetId"])
        if row["Version"] != "v01" or row["ReviewStatus"].startswith("User-directed") is False:
            raise ValueError(f"Projectile frame is outside the requested v01 batch: {row['AssetId']}")
        if row["SourceHash"].lower() != source_hash or row["ExportHash"].lower() != export_hash:
            raise ValueError(f"Projectile manifest hash mismatch: {row['AssetId']}")
        cell = (int(row["AtlasX"]), int(row["AtlasY"]), row["TowerId"], int(row["FrameIndex"]))
        if cell not in EXPECTED_PROJECTILE_CELLS or cell in cells:
            raise ValueError(f"Unexpected or duplicate projectile cell: {cell}")
        cells.add(cell)
        if row["UnrealPath"] != PROJECTILE_DEST or (int(row["AtlasW"]), int(row["AtlasH"])) != (192, 192):
            raise ValueError(f"Projectile atlas destination/rect mismatch: {row['AssetId']}")
    if cells != EXPECTED_PROJECTILE_CELLS:
        raise ValueError("Projectile manifest does not cover both four-frame sequences")
    return fields, rows, {"source": exported, "source_hash": source_hash, "export_hash": export_hash, "size": (848, 432)}


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
        parent = destination.rsplit("/", 1)[0]
        unreal.EditorAssetLibrary.make_directory(parent)
        task = unreal.AssetImportTask()
        task.set_editor_properties({
            "filename": str(config["source"]), "destination_path": parent,
            "destination_name": destination.rsplit("/", 1)[-1], "replace_existing": False,
            "automated": True, "save": True,
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
        raise RuntimeError(f"Source changed during import: {config['destination']}")
    return {"asset_id": config["asset_id"], "version": config["version"],
            "asset_path": destination, "source_path": str(config["source"]),
            "source_sha256": config["hash"], "dimensions_px": list(config["size"]),
            "action": action, "texture_group": "UI", "mipmaps": False, "srgb": True,
            "alpha": config["alpha"], "texture_settings_readback": "passed"}


try:
    unreal.log("P7 Figure003 and independent Dog/Mouse projectile atlas import started")
    report["engine"] = unreal.SystemLibrary.get_engine_version()
    catalog_fields, catalog_rows, catalog_imports = preflight_catalog()
    projectile_fields, projectile_rows, projectile = preflight_projectile_atlas()
    projectile_import = {
        "asset_id": "P7-VFX-A1-TOWER-PROJECTILE-ATLAS",
        "version": "v01", "source": projectile["source"], "hash": projectile["export_hash"],
        "size": projectile["size"], "destination": PROJECTILE_DEST, "alpha": True,
    }
    for config in catalog_imports + [projectile_import]:
        report["assets"].append(import_texture(config))

    evidence_path = "UE5/Records/p7_td/evidence/" + REPORT_PATH.name
    for config in catalog_imports:
        row = config["row"]
        if config["asset_id"] == "P7-CHR-A1-CAT-003":
            row["Status"] = "按用户要求换为图录003站立捧腹；UE Texture2D 已导入；HUD 显示接入，viewport 验收待完成"
            row["ReviewStatus"] = "User-directed replacement; UE Texture2D imported; P7 HUD hooked up; viewport acceptance pending"
        else:
            row["Status"] = "用户已通过；UE Texture2D 已导入；HUD 显示接入，运行画面验收待完成"
            row["ReviewStatus"] = "User-approved visual; UE Texture2D imported; P7 HUD hooked up; viewport acceptance pending"
        row["EvidencePath"] = evidence_path
    write_csv(CATALOG_V03, catalog_fields, catalog_rows)

    legacy_fields, legacy_rows = read_csv(CATALOG_V02)
    old_laugh = [row for row in legacy_rows if row.get("AssetId") == "P7-CHR-A1-CAT-079" and row.get("Version") == "v02"]
    if len(old_laugh) != 1:
        raise ValueError(f"Expected one historical Figure079 v02 record; found {len(old_laugh)}")
    old_laugh[0]["Status"] = "已被 v03 图录003站立捧腹姿态替代；旧图与导入资源保留追溯"
    old_laugh[0]["ReviewStatus"] = "Superseded by user-directed Figure003 replacement on 2026-11-12; historical asset retained"
    old_laugh[0]["EvidencePath"] = evidence_path
    write_csv(CATALOG_V02, legacy_fields, legacy_rows)

    for row in projectile_rows:
        row["ReviewStatus"] = "User-directed; UE Texture2D atlas imported; Dog/Mouse HUD attack VFX hooked up; viewport review pending"
        row["EvidencePath"] = evidence_path
        row["Notes"] = f"{row['Notes']}; imported as transparent UI atlas; presentation-only attack visuals, gameplay values unchanged"
    write_csv(PROJECTILE_MANIFEST, projectile_fields, projectile_rows)
    report["status"] = "imported_and_wired_requested_catalog_replacement_and_tower_projectile_vfx"
    report["catalog_source_pdf_sha256"] = PDF_HASH
    unreal.log("P7 Figure003 and independent Dog/Mouse projectile atlas import completed")
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    raise
finally:
    EVIDENCE_ROOT.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("NAIWA_P7_CATALOG003_PROJECTILE_IMPORT " + json.dumps(report, ensure_ascii=False))
