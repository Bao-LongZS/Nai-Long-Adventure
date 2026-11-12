"""Safely update only the P7 Alpha enemy DataTable from its source CSV."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import traceback

import unreal


ROOT = Path(__file__).resolve().parents[2]
CSV_SOURCE = ROOT / "UE5/SourceAssets/P7/Data/P7_Alpha01_Enemies_v01.csv"
ASSET_PATH = "/Game/Naiwa/P7/Data/DT_P7_Alpha01_Enemies"
REPORT_PATH = Path(
    os.environ.get(
        "NAIWA_P7_ENEMY_REIMPORT_REPORT",
        str(ROOT / "UE5/Records/p7_td/evidence/P7_EnemyTuningReimport.json"),
    )
)
EXPECTED_SPEEDS = {
    "P7_Enemy_Normal": 2400,
    "P7_Enemy_Fast": 4500,
    "P7_Enemy_Jammer": 1800,
}
COLUMNS = (
    "EnemyId",
    "Kind",
    "MaxHealth",
    "SpeedMilliCellsPerSecond",
    "InterferenceRadiusMilliCells",
    "InterferenceDurationSteps",
)


def read_table_rows(table):
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
        for column in COLUMNS
    }
    if any(len(items) != len(row_names) for items in values.values()):
        raise RuntimeError("DataTable row and column counts do not match")
    return {
        row_name: {column: values[column][index] for column in COLUMNS}
        for index, row_name in enumerate(row_names)
    }


def run_import():
    if not CSV_SOURCE.is_file():
        raise FileNotFoundError("Missing P7 enemy CSV: " + str(CSV_SOURCE))
    if not unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        raise RuntimeError("Refusing to import because the target DataTable is missing: " + ASSET_PATH)

    row_struct = unreal.NaiwaP7EnemyRow.static_struct()
    factory = unreal.CSVImportFactory()
    settings = factory.get_editor_property("automated_import_settings")
    settings.set_editor_property("import_row_struct", row_struct)
    factory.set_editor_property("automated_import_settings", settings)

    task = unreal.AssetImportTask()
    task.set_editor_properties({
        "filename": str(CSV_SOURCE),
        "destination_path": "/Game/Naiwa/P7/Data",
        "destination_name": "DT_P7_Alpha01_Enemies",
        "automated": True,
        "replace_existing": True,
        "save": True,
        "factory": factory,
    })
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    table = unreal.load_asset(ASSET_PATH)
    if not table:
        raise RuntimeError("CSV import did not produce " + ASSET_PATH)
    actual_struct = unreal.DataTableFunctionLibrary.get_data_table_row_struct(table)
    actual_struct_path = actual_struct.get_path_name() if actual_struct else ""
    if actual_struct_path != row_struct.get_path_name():
        raise RuntimeError(
            "Imported DataTable row struct mismatch: " + actual_struct_path
        )

    rows = read_table_rows(table)
    if set(rows) != set(EXPECTED_SPEEDS):
        raise RuntimeError("Unexpected enemy rows after import: " + repr(sorted(rows)))
    actual_speeds = {
        row_id: int(float(row["SpeedMilliCellsPerSecond"]))
        for row_id, row in rows.items()
    }
    if actual_speeds != EXPECTED_SPEEDS:
        raise RuntimeError("Enemy speed readback mismatch: " + repr(actual_speeds))

    return {
        "status": "passed",
        "asset": ASSET_PATH,
        "row_struct": actual_struct_path,
        "source_csv": str(CSV_SOURCE),
        "source_sha256": hashlib.sha256(CSV_SOURCE.read_bytes()).hexdigest(),
        "updated_rows": actual_speeds,
        "updated_asset_count": 1,
    }


try:
    result = run_import()
except Exception:
    result = {"status": "failed", "error": traceback.format_exc()}

REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
unreal.log("NAIWA_P7_ENEMY_TUNING_REIMPORT " + json.dumps(result, ensure_ascii=False))
if result["status"] != "passed":
    raise RuntimeError(result["error"])
