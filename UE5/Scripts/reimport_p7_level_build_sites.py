"""Safely update only the P7 Alpha level DataTable's build site positions."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import traceback

import unreal


ROOT = Path(__file__).resolve().parents[2]
CSV_SOURCE = ROOT / "UE5/SourceAssets/P7/Data/P7_Alpha01_Level_v01.csv"
ASSET_PATH = "/Game/Naiwa/P7/Data/DT_P7_Alpha01_Level"
REPORT_PATH = Path(
    os.environ.get(
        "NAIWA_P7_LEVEL_REIMPORT_REPORT",
        str(ROOT / "UE5/Records/p7_td/evidence/P7_LevelBuildSitesReimport.json"),
    )
)


def read_column(table, column: str) -> dict[str, str]:
    row_names = [
        str(name)
        for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    ]
    values = [
        str(value)
        for value in unreal.DataTableFunctionLibrary.get_data_table_column_as_string(
            table, unreal.Name(column)
        )
    ]
    if len(row_names) != len(values):
        raise RuntimeError("DataTable row and column counts do not match for " + column)
    return dict(zip(row_names, values))


def run_import():
    if not CSV_SOURCE.is_file():
        raise FileNotFoundError("Missing P7 level CSV: " + str(CSV_SOURCE))
    if not unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        raise RuntimeError("Refusing to import because the target DataTable is missing: " + ASSET_PATH)

    row_struct = unreal.NaiwaP7LevelRow.static_struct()
    factory = unreal.CSVImportFactory()
    settings = factory.get_editor_property("automated_import_settings")
    settings.set_editor_property("import_row_struct", row_struct)
    factory.set_editor_property("automated_import_settings", settings)

    task = unreal.AssetImportTask()
    task.set_editor_properties({
        "filename": str(CSV_SOURCE),
        "destination_path": "/Game/Naiwa/P7/Data",
        "destination_name": "DT_P7_Alpha01_Level",
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
        raise RuntimeError("Imported DataTable row struct mismatch: " + actual_struct_path)

    visible_rows = read_column(table, "VisibleBuildCells")
    hidden_rows = read_column(table, "HiddenBuildSites")
    if set(visible_rows) != {"P7_Alpha01"} or set(hidden_rows) != {"P7_Alpha01"}:
        raise RuntimeError("Unexpected level DataTable rows")
    visible = visible_rows["P7_Alpha01"]
    hidden = hidden_rows["P7_Alpha01"]
    if visible.count("Y=7") != 8 or "Y=8" in visible:
        raise RuntimeError("Visible build cells did not read back at logical row 7: " + visible)
    if hidden.count("Y=8") != 2 or "Y=9" in hidden:
        raise RuntimeError("Hidden build cells did not read back at logical row 8: " + hidden)
    if "X=5,Y=8" not in hidden or "X=14,Y=8" not in hidden:
        raise RuntimeError("Unexpected hidden build site locations: " + hidden)

    return {
        "status": "passed",
        "asset": ASSET_PATH,
        "row_struct": actual_struct_path,
        "source_csv": str(CSV_SOURCE),
        "source_sha256": hashlib.sha256(CSV_SOURCE.read_bytes()).hexdigest(),
        "visible_build_cells": visible,
        "hidden_build_sites": hidden,
        "updated_asset_count": 1,
    }


try:
    result = run_import()
except Exception:
    result = {"status": "failed", "error": traceback.format_exc()}

REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
unreal.log("NAIWA_P7_LEVEL_BUILD_SITES_REIMPORT " + json.dumps(result, ensure_ascii=False))
if result["status"] != "passed":
    raise RuntimeError(result["error"])
