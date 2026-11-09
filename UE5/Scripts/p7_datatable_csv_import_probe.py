"""Temporary UE Editor DataTable CSV import/readback probe for P7 row schemas.

The values in the fixture CSVs are parser probes only, not production balance.
Imported DataTable assets are saved/read back and then deleted by this script.
"""
from pathlib import Path
import json
import os
import traceback

import unreal


ROOT = Path(__file__).resolve().parents[2]
EVIDENCE_DIR = ROOT / "UE5/Records/p7_td/evidence"
LEVEL_CSV = EVIDENCE_DIR / "P7_DataTableCsvImport_Level.csv"
WAVE_CSV = EVIDENCE_DIR / "P7_DataTableCsvImport_Wave.csv"
REPORT_PATH = Path(
    os.environ.get(
        "NAIWA_P7_DATATABLE_REPORT",
        str(EVIDENCE_DIR / "P7_DataTableCsvImportProbe.json"),
    )
)
DESTINATION = "/Game/Naiwa/P7/Tests"
ASSETS = (
    {
        "name": "DT_P7_Level_CsvImportProbe",
        "path": DESTINATION + "/DT_P7_Level_CsvImportProbe",
        "csv": LEVEL_CSV,
        "row_struct": "NaiwaP7LevelRow",
        "columns": (
            "LevelId",
            "RouteACells",
            "RouteBCells",
            "InterferenceTriggerCell",
            "VisibleBuildCells",
            "ObstacleIds",
            "HiddenBuildSites",
            "StartingCoins",
        ),
    },
    {
        "name": "DT_P7_Wave_CsvImportProbe",
        "path": DESTINATION + "/DT_P7_Wave_CsvImportProbe",
        "csv": WAVE_CSV,
        "row_struct": "NaiwaP7WaveRow",
        "columns": ("WaveIndex", "Spawns"),
    },
)


def column_rows(table, columns):
    row_names = [
        str(name)
        for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    ]
    values = {
        column: [
            str(value)
            for value in unreal.DataTableFunctionLibrary.get_data_table_column_as_string(
                table, unreal.Name(column)
            )
        ]
        for column in columns
    }
    if any(len(column_values) != len(row_names) for column_values in values.values()):
        raise RuntimeError(
            "DataTable row/column count mismatch: "
            + json.dumps(
                {
                    "rows": len(row_names),
                    **{key: len(value) for key, value in values.items()},
                }
            )
        )
    return row_names, {
        row_name: {
            column: values[column][index]
            for column in columns
        }
        for index, row_name in enumerate(row_names)
    }


def import_table(spec, created_assets):
    if not spec["csv"].is_file():
        raise FileNotFoundError("Missing P7 CSV fixture: " + str(spec["csv"]))
    if unreal.EditorAssetLibrary.does_asset_exist(spec["path"]):
        raise RuntimeError(
            "Refusing to replace an existing asset: " + spec["path"]
        )

    row_struct_type = getattr(unreal, spec["row_struct"], None)
    if row_struct_type is None:
        raise RuntimeError("Python row struct is unavailable: " + spec["row_struct"])
    row_struct = row_struct_type.static_struct()

    unreal.EditorAssetLibrary.make_directory(DESTINATION)
    factory = unreal.CSVImportFactory()
    settings = factory.get_editor_property("automated_import_settings")
    settings.set_editor_property("import_row_struct", row_struct)
    factory.set_editor_property("automated_import_settings", settings)

    created_assets.append(spec["path"])
    task = unreal.AssetImportTask()
    task.set_editor_properties(
        {
            "filename": str(spec["csv"]),
            "destination_path": DESTINATION,
            "destination_name": spec["name"],
            "automated": True,
            "replace_existing": False,
            "save": True,
            "factory": factory,
        }
    )
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    table = unreal.load_asset(spec["path"])
    if not table:
        imported_paths = task.get_editor_property("imported_object_paths")
        raise RuntimeError(
            "CSV import did not create "
            + spec["path"]
            + "; imported="
            + str(imported_paths)
        )

    expected_struct = row_struct.get_path_name()
    actual_struct = unreal.DataTableFunctionLibrary.get_data_table_row_struct(table)
    actual_struct_path = actual_struct.get_path_name() if actual_struct else ""
    if actual_struct_path != expected_struct:
        raise RuntimeError(
            f"{spec['name']} row struct is {actual_struct_path}; expected {expected_struct}"
        )

    columns = sorted(
        str(name)
        for name in unreal.DataTableFunctionLibrary.get_data_table_column_names(table)
    )
    expected_columns = sorted(spec["columns"])
    if columns != expected_columns:
        raise RuntimeError(
            f"{spec['name']} columns are {columns}; expected {expected_columns}"
        )

    row_names, rows = column_rows(table, spec["columns"])
    unreal.EditorAssetLibrary.save_loaded_asset(table)

    project_dir = Path(unreal.Paths.project_dir()).resolve()
    asset_file = (
        project_dir
        / "Content"
        / "Naiwa"
        / "P7"
        / "Tests"
        / (spec["name"] + ".uasset")
    )
    if not asset_file.is_file() or asset_file.stat().st_size == 0:
        raise RuntimeError(
            f"Saved DataTable package is missing or empty: {asset_file}"
        )

    return {
        "asset": spec["path"],
        "saved_package_bytes": asset_file.stat().st_size,
        "row_struct": actual_struct_path,
        "columns": columns,
        "row_names": sorted(row_names),
        "rows": rows,
        "source_csv": str(spec["csv"]),
        "imported_object_paths": [
            str(path) for path in task.get_editor_property("imported_object_paths")
        ],
    }


def validate_readback(imported):
    level = imported["level"]
    if level["row_names"] != ["P7CsvFileProbe"]:
        raise RuntimeError("Unexpected Level rows: " + str(level["row_names"]))
    level_row = level["rows"]["P7CsvFileProbe"]
    if int(level_row["StartingCoins"]) != 100:
        raise RuntimeError("Level StartingCoins did not round-trip: " + str(level_row))
    if "X=1" not in level_row["RouteACells"] or "Y=5" not in level_row["RouteACells"]:
        raise RuntimeError("RouteACells did not round-trip: " + level_row["RouteACells"])
    if "Obstacle_B" not in level_row["ObstacleIds"]:
        raise RuntimeError("ObstacleIds did not round-trip: " + level_row["ObstacleIds"])
    hidden_sites = level_row["HiddenBuildSites"]
    if "UnlockObstacleId=" not in hidden_sites or "Obstacle_B" not in hidden_sites or "X=14" not in hidden_sites:
        raise RuntimeError("HiddenBuildSites did not round-trip: " + hidden_sites)

    wave = imported["wave"]
    if wave["row_names"] != ["P7WaveCsvProbe"]:
        raise RuntimeError("Unexpected Wave rows: " + str(wave["row_names"]))
    wave_row = wave["rows"]["P7WaveCsvProbe"]
    spawns = wave_row["Spawns"]
    if "Enemy_Fast" not in spawns or "FirstSpawnDelaySteps=60" not in spawns:
        raise RuntimeError("Wave Spawns did not round-trip: " + spawns)
    if int(wave_row["WaveIndex"]) != 1:
        raise RuntimeError("WaveIndex did not round-trip: " + wave_row["WaveIndex"])


created_assets = []
result = {"status": "failed"}
try:
    imported = {
        "level": import_table(ASSETS[0], created_assets),
        "wave": import_table(ASSETS[1], created_assets),
    }
    validate_readback(imported)
    result = {
        "status": "passed",
        "import_factory": "CSVImportFactory",
        "scope": (
            "Editor asset import, saved package, schema and row readback; "
            "fixture values are not production data and gameplay consumption is not tested."
        ),
        "tables": imported,
    }
except Exception:
    result = {"status": "failed", "error": traceback.format_exc()}
finally:
    cleanup = {}
    for asset_path in reversed(created_assets):
        try:
            if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
                cleanup[asset_path] = bool(
                    unreal.EditorAssetLibrary.delete_asset(asset_path)
                )
            else:
                cleanup[asset_path] = True
        except Exception:
            cleanup[asset_path] = False
    result["temporary_asset_cleanup"] = cleanup
    if not all(cleanup.values()):
        result["status"] = "failed_cleanup"

REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(
    json.dumps(result, indent=2, ensure_ascii=False),
    encoding="utf-8",
)
unreal.log("NAIWA_P7_DATATABLE_CSV_IMPORT_PROBE " + json.dumps(result, ensure_ascii=False))
if result["status"] != "passed":
    raise RuntimeError(result.get("error", "Temporary P7 DataTable asset cleanup failed"))
