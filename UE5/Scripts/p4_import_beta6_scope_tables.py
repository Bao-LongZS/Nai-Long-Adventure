"""Import the generated P4 Beta6 projections as native Unreal DataTables."""

from pathlib import Path
import csv
import hashlib
import json
import os
import traceback

import unreal


ROOT = Path(__file__).resolve().parents[2]
DATA_DIR = ROOT / "UE5/SourceAssets/P4/Data/Beta6"
MANIFEST_PATH = DATA_DIR / "P4Beta6_ScopeProjection.json"
CONTENT_MANIFEST_PATH = DATA_DIR / "P4Beta6_ContentProjection.json"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_DataTableImport.json"
REPORT_PATH = Path(os.environ.get("NAIWA_P4_BETA6_IMPORT_REPORT", str(REPORT_PATH)))


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def import_table(
    csv_name: str,
    asset_name: str,
    row_struct_type: type,
    expected_count: int,
    object_ref_columns: tuple[str, ...] = (),
) -> dict:
    csv_path = DATA_DIR / csv_name
    if not csv_path.is_file():
        raise FileNotFoundError(f"Missing Beta6 CSV source: {csv_path}")

    factory = unreal.CSVImportFactory()
    settings = factory.get_editor_property("automated_import_settings")
    row_struct = row_struct_type.static_struct()
    settings.set_editor_property("import_row_struct", row_struct)
    factory.set_editor_property("automated_import_settings", settings)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(csv_path))
    task.set_editor_property("destination_path", "/Game/Naiwa/P4/Data")
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("factory", factory)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = f"/Game/Naiwa/P4/Data/{asset_name}.{asset_name}"
    table = unreal.load_asset(asset_path)
    if not table:
        raise RuntimeError(f"CSV import did not create {asset_path}")
    actual_struct = unreal.DataTableFunctionLibrary.get_data_table_row_struct(table)
    if not actual_struct or actual_struct.get_path_name() != row_struct.get_path_name():
        raise RuntimeError(f"{asset_name} uses the wrong row struct")
    row_names = sorted(str(name) for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(table))
    if len(row_names) != expected_count:
        raise RuntimeError(f"{asset_name} imported {len(row_names)} rows; expected {expected_count}")

    object_ref_readback = {}
    if object_ref_columns:
        column_names = {
            str(name) for name in unreal.DataTableFunctionLibrary.get_data_table_column_names(table)
        }
        missing_columns = sorted(set(object_ref_columns) - column_names)
        if missing_columns:
            raise RuntimeError(f"{asset_name} is missing expected object-reference columns: {missing_columns}")

        with csv_path.open(encoding="utf-8-sig", newline="") as source_file:
            source_rows = list(csv.DictReader(source_file))
        source_refs = {
            row["RowName"]: {column: row[column] for column in object_ref_columns}
            for row in source_rows
        }
        raw_row_names = [
            str(name) for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
        ]
        for column in object_ref_columns:
            values = [
                str(value)
                for value in unreal.DataTableFunctionLibrary.get_data_table_column_as_string(
                    table, unreal.Name(column)
                )
            ]
            if len(values) != len(raw_row_names):
                raise RuntimeError(
                    f"{asset_name}.{column} readback returned {len(values)} values "
                    f"for {len(raw_row_names)} rows"
                )
            actual_by_row = dict(zip(raw_row_names, values))
            for row_name, row_refs in source_refs.items():
                reference = row_refs[column]
                if "'" not in reference:
                    raise RuntimeError(f"{asset_name}.{column} has an invalid source reference for {row_name}: {reference}")
                object_path = reference.split("'", 1)[1].rsplit("'", 1)[0]
                package_path = object_path.rsplit(".", 1)[0]
                actual_value = actual_by_row.get(row_name, "")
                if package_path not in actual_value:
                    raise RuntimeError(
                        f"{asset_name}.{column} readback for {row_name} does not contain "
                        f"the projected asset path {package_path}: {actual_value}"
                    )
                object_ref_readback.setdefault(row_name, {})[column] = package_path

    return {
        "asset": asset_path,
        "row_struct": actual_struct.get_path_name(),
        "row_count": len(row_names),
        "row_names": row_names,
        "object_ref_readback": object_ref_readback,
        "csv_sha256": sha256(csv_path),
        "imported_object_paths": [str(path) for path in task.get_editor_property("imported_object_paths")],
    }


try:
    if not MANIFEST_PATH.is_file():
        raise FileNotFoundError(f"Missing generated scope manifest: {MANIFEST_PATH}")
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    if not CONTENT_MANIFEST_PATH.is_file():
        raise FileNotFoundError(f"Missing generated content manifest: {CONTENT_MANIFEST_PATH}")
    content_manifest = json.loads(CONTENT_MANIFEST_PATH.read_text(encoding="utf-8"))
    expected_hashes = {
        Path(item["path"]).name: item["sha256"]
        for item in manifest["outputs"]
    }
    expected_hashes.update({
        Path(item["path"]).name: item["sha256"]
        for item in content_manifest["outputs"]
    })
    for csv_name, expected_hash in expected_hashes.items():
        csv_path = DATA_DIR / csv_name
        if not csv_path.is_file() or sha256(csv_path) != expected_hash:
            raise RuntimeError(f"Beta6 CSV source hash does not match the projection manifest: {csv_name}")
    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P4/Data")

    imported = {
        "chapters": import_table(
            "P4Beta6_Chapters.csv",
            "DT_P4_Beta6_Chapters",
            unreal.NaiwaP4Beta6ChapterRow,
            6,
        ),
        "themes": import_table(
            "P4Beta6_Themes.csv",
            "DT_P4_Beta6_Themes",
            unreal.NaiwaP4Beta6ThemeRow,
            6,
            object_ref_columns=("BackdropTexture",),
        ),
        "wave_spawns": import_table(
            "P4Beta6_WaveSpawns.csv",
            "DT_P4_Beta6_WaveSpawns",
            unreal.NaiwaP4Beta6WaveSpawnRow,
            66,
        ),
        "enemies": import_table(
            "P4Beta6_Enemies.csv",
            "DT_P4_Beta6_Enemies",
            unreal.NaiwaP4Beta6EnemyRow,
            12,
        ),
        "bosses": import_table(
            "P4Beta6_Bosses.csv",
            "DT_P4_Beta6_Bosses",
            unreal.NaiwaP4Beta6BossRow,
            2,
        ),
        "boss_attacks": import_table(
            "P4Beta6_BossAttacks.csv",
            "DT_P4_Beta6_BossAttacks",
            unreal.NaiwaP4Beta6BossAttackRow,
            8,
        ),
    }
    manifest["ueDataTableImport"] = {
        "status": "passed",
        "importedAssets": imported,
        "rowCounts": {key: value["row_count"] for key, value in imported.items()},
        "note": "Native DataTable row counts and row structs were read back; all six theme BackdropTexture object paths were also read back and matched the projection.",
    }
    content_manifest["ueDataTableImport"] = {
        "status": "passed",
        "importedAssets": {key: imported[key] for key in ("enemies", "bosses", "boss_attacks")},
        "rowCounts": {key: imported[key]["row_count"] for key in ("enemies", "bosses", "boss_attacks")},
        "note": "Native design-projection row counts and row structs were read back from the imported UE assets; unresolved rows remain disabled for combat use.",
    }
    result = {
        "status": "passed",
        "factory": "CSVImportFactory",
        "source_manifest": str(MANIFEST_PATH.relative_to(ROOT)).replace("\\", "/"),
        "source_json_sha256": manifest["source"]["chapterDataSha256"],
        "imported_tables": imported,
    }
except Exception:
    result = {"status": "failed", "error": traceback.format_exc()}

REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
if result["status"] == "passed":
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    CONTENT_MANIFEST_PATH.write_text(json.dumps(content_manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
unreal.log("NAIWA_P4_BETA6_DATATABLE_IMPORT " + json.dumps(result, ensure_ascii=False))
if result["status"] != "passed":
    raise RuntimeError(result["error"])
