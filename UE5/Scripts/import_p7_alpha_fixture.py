"""Import the provisional P7 Alpha CSV fixture as persistent Unreal DataTables."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import traceback

import unreal


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "UE5/SourceAssets/P7/Data"
CONTENT = "/Game/Naiwa/P7/Data"
EVIDENCE = ROOT / "UE5/Records/p7_td/evidence"
REPORT = Path(
    os.environ.get(
        "NAIWA_P7_ALPHA_IMPORT_REPORT",
        str(EVIDENCE / "P7_AlphaFixtureImport.json"),
    )
)


def table_spec(name, source_name, row_struct, columns, row_names):
    return {
        "name": name,
        "source": SOURCE / source_name,
        "row_struct": row_struct,
        "columns": tuple(columns),
        "row_names": tuple(row_names),
        "asset_path": CONTENT + "/" + name,
    }


SPECS = (
    table_spec(
        "DT_P7_Alpha01_Level",
        "P7_Alpha01_Level_v01.csv",
        "NaiwaP7LevelRow",
        (
            "LevelId",
            "RouteACells",
            "RouteBCells",
            "InterferenceTriggerCell",
            "VisibleBuildCells",
            "ObstacleIds",
            "HiddenBuildSites",
            "StartingCoins",
        ),
        ("P7_Alpha01",),
    ),
    table_spec(
        "DT_P7_Alpha01_Towers",
        "P7_Alpha01_Towers_v01.csv",
        "NaiwaP7TowerRow",
        (
            "TowerId",
            "Role",
            "Level",
            "CostCoins",
            "Damage",
            "AttackIntervalSteps",
            "RangeMilliCells",
            "SplashRadiusMilliCells",
            "SlowPercent",
            "SlowDurationSteps",
        ),
        (
            "P7_MilkChicken_L1",
            "P7_MilkChicken_L2",
            "P7_MilkChicken_L3",
            "P7_MilkDog_L1",
            "P7_MilkDog_L2",
            "P7_MilkDog_L3",
            "P7_MilkMouse_L1",
            "P7_MilkMouse_L2",
            "P7_MilkMouse_L3",
        ),
    ),
    table_spec(
        "DT_P7_Alpha01_Enemies",
        "P7_Alpha01_Enemies_v01.csv",
        "NaiwaP7EnemyRow",
        (
            "EnemyId",
            "Kind",
            "MaxHealth",
            "SpeedMilliCellsPerSecond",
            "InterferenceRadiusMilliCells",
            "InterferenceDurationSteps",
        ),
        ("P7_Enemy_Normal", "P7_Enemy_Fast", "P7_Enemy_Jammer"),
    ),
    table_spec(
        "DT_P7_Alpha01_Obstacles",
        "P7_Alpha01_Obstacles_v01.csv",
        "NaiwaP7ObstacleRow",
        ("ObstacleId", "Cell", "MaxHealth", "RewardCoins"),
        ("P7_Obstacle_CrateA", "P7_Obstacle_CrateB"),
    ),
    table_spec(
        "DT_P7_Alpha01_Waves",
        "P7_Alpha01_Waves_v01.csv",
        "NaiwaP7WaveRow",
        ("WaveIndex", "Spawns"),
        tuple("P7_Wave_{:02d}".format(index) for index in range(1, 9)),
    ),
)


def read_rows(table, columns):
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
    if any(len(items) != len(row_names) for items in values.values()):
        raise RuntimeError("DataTable row/column count mismatch: " + repr(values))
    rows = {
        row_name: {
            column: values[column][index]
            for column in columns
        }
        for index, row_name in enumerate(row_names)
    }
    return row_names, rows


def import_one(spec, created_assets):
    source = spec["source"]
    if not source.is_file():
        raise FileNotFoundError("Missing source CSV: " + str(source))
    if unreal.EditorAssetLibrary.does_asset_exist(spec["asset_path"]):
        raise RuntimeError(
            "Refusing to overwrite an existing DataTable: " + spec["asset_path"]
        )

    row_struct_type = getattr(unreal, spec["row_struct"], None)
    if row_struct_type is None:
        raise RuntimeError("Unreal Python row type is unavailable: " + spec["row_struct"])

    factory = unreal.CSVImportFactory()
    settings = factory.get_editor_property("automated_import_settings")
    settings.set_editor_property("import_row_struct", row_struct_type.static_struct())
    factory.set_editor_property("automated_import_settings", settings)
    unreal.EditorAssetLibrary.make_directory(CONTENT)

    task = unreal.AssetImportTask()
    task.set_editor_properties(
        {
            "filename": str(source),
            "destination_path": CONTENT,
            "destination_name": spec["name"],
            "automated": True,
            "replace_existing": False,
            "save": True,
            "factory": factory,
        }
    )
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    created_assets.append(spec["asset_path"])

    table = unreal.load_asset(spec["asset_path"])
    if not table:
        raise RuntimeError("CSV import did not create " + spec["asset_path"])

    actual_struct = unreal.DataTableFunctionLibrary.get_data_table_row_struct(table)
    actual_struct_path = actual_struct.get_path_name() if actual_struct else ""
    expected_struct_path = row_struct_type.static_struct().get_path_name()
    if actual_struct_path != expected_struct_path:
        raise RuntimeError(
            spec["name"] + " row struct mismatch: " + actual_struct_path
        )

    actual_columns = sorted(
        str(name)
        for name in unreal.DataTableFunctionLibrary.get_data_table_column_names(table)
    )
    if actual_columns != sorted(spec["columns"]):
        raise RuntimeError(spec["name"] + " columns mismatch: " + repr(actual_columns))

    row_names, rows = read_rows(table, spec["columns"])
    if tuple(sorted(row_names)) != tuple(sorted(spec["row_names"])):
        raise RuntimeError(spec["name"] + " row names mismatch: " + repr(row_names))
    unreal.EditorAssetLibrary.save_loaded_asset(table)

    package = ROOT / "UE5/NaiwaP0/Content/Naiwa/P7/Data" / (spec["name"] + ".uasset")
    if not package.is_file() or package.stat().st_size <= 0:
        raise RuntimeError("Saved DataTable package is missing: " + str(package))

    return {
        "asset_path": spec["asset_path"],
        "source_csv": str(source),
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "saved_package": str(package),
        "saved_package_bytes": package.stat().st_size,
        "row_struct": actual_struct_path,
        "columns": actual_columns,
        "row_names": sorted(row_names),
        "rows": rows,
    }


created_assets = []
report = {"status": "failed", "import_factory": "CSVImportFactory"}
try:
    imported = [import_one(spec, created_assets) for spec in SPECS]
    report = {
        "status": "passed",
        "fixture_id": "P7_Alpha01_v01",
        "approval_state": "technical validation candidate; G0 design sign-off pending",
        "import_factory": "CSVImportFactory",
        "table_count": len(imported),
        "tables": imported,
    }
except Exception:
    report = {"status": "failed", "error": traceback.format_exc()}
finally:
    cleanup = {}
    if report["status"] != "passed":
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
    report["failure_cleanup"] = cleanup

REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
if report["status"] != "passed":
    unreal.log_error(report.get("error", "P7 Alpha fixture import failed"))
    raise RuntimeError("P7 Alpha fixture import failed; see " + str(REPORT))
unreal.log("Imported and read back five P7 Alpha fixture DataTables from CSV.")
