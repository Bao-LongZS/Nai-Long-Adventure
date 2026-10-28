"""Scratch-only W1 probe for updating an existing P4 DataTable from CSV."""
from __future__ import annotations

import json
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
CSV_SOURCE = ROOT / "UE5/SourceAssets/P4/Data/P4UpgradeSmoke.csv"
ASSET_PATH = "/Game/Naiwa/P4/Data/DT_P4_Upgrade_ReimportProbe"
PROJECT_DIR = Path(unreal.Paths.project_dir()).resolve()
SCRATCH_MARKER = ".workbuddy/P4BuildScratch"
REPORT_PATH = PROJECT_DIR / "Saved/P4DataTableReimportProbe.json"
VARIANT_CSV = PROJECT_DIR.parent / "P4DataTableReimportProbe/P4UpgradeSmoke_variant.csv"


def import_csv(csv_path: Path, row_struct):
    factory = unreal.CSVImportFactory()
    settings = factory.get_editor_property("automated_import_settings")
    settings.set_editor_property("import_row_struct", row_struct)
    factory.set_editor_property("automated_import_settings", settings)

    task = unreal.AssetImportTask()
    task.set_editor_properties({
        "filename": str(csv_path),
        "destination_path": "/Game/Naiwa/P4/Data",
        "destination_name": "DT_P4_Upgrade_ReimportProbe",
        "replace_existing": True,
        "automated": True,
        "save": True,
        "factory": factory,
    })
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    table = unreal.load_asset(ASSET_PATH)
    if not table:
        raise RuntimeError(
            f"CSV import did not produce {ASSET_PATH}; "
            f"imported={task.get_editor_property('imported_object_paths')}"
        )
    return table


def read_float(table, row_id: str, column_name: str) -> float:
    row_names = [
        str(name)
        for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    ]
    values = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(
        table, unreal.Name(column_name)
    )
    rows = dict(zip(row_names, values))
    if row_id not in rows:
        raise RuntimeError(f"Missing DataTable row {row_id}; found={sorted(rows)}")
    return float(rows[row_id])


def run_probe() -> dict:
    project_text = str(PROJECT_DIR).replace("\\", "/").casefold()
    marker_text = SCRATCH_MARKER.replace("\\", "/").casefold()
    if marker_text not in project_text:
        raise RuntimeError(
            f"Refusing to run outside the isolated P4 scratch project: {PROJECT_DIR}"
        )
    if not CSV_SOURCE.is_file():
        raise FileNotFoundError(f"Missing source CSV: {CSV_SOURCE}")

    source_text = CSV_SOURCE.read_text(encoding="utf-8-sig")
    baseline_row = "bounce_ball,跳弹奶球,projectile_bounce,1,0.40,690,"
    variant_row = "bounce_ball,跳弹奶球,projectile_bounce,1.25,0.40,690,"
    if source_text.count(baseline_row) != 1:
        raise RuntimeError("Expected exactly one baseline bounce_ball CSV row")
    VARIANT_CSV.parent.mkdir(parents=True, exist_ok=True)
    VARIANT_CSV.write_text(
        source_text.replace(baseline_row, variant_row, 1),
        encoding="utf-8-sig",
        newline="",
    )

    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P4/Data")
    row_struct = unreal.NaiwaP4UpgradeRow.static_struct()

    baseline_table = import_csv(CSV_SOURCE, row_struct)
    baseline_value = read_float(baseline_table, "bounce_ball", "P0")
    if abs(baseline_value - 1.0) > 1e-5:
        raise RuntimeError(f"Baseline bounce_ball.P0={baseline_value}; expected 1.0")

    # Reimport the modified CSV into the same native DataTable without a C++ build.
    updated_table = import_csv(VARIANT_CSV, row_struct)
    updated_value = read_float(updated_table, "bounce_ball", "P0")
    if abs(updated_value - 1.25) > 1e-5:
        raise RuntimeError(f"Reimported bounce_ball.P0={updated_value}; expected 1.25")

    restored_table = import_csv(CSV_SOURCE, row_struct)
    restored_value = read_float(restored_table, "bounce_ball", "P0")
    if abs(restored_value - 1.0) > 1e-5:
        raise RuntimeError(f"Restored bounce_ball.P0={restored_value}; expected 1.0")

    return {
        "status": "passed",
        "project": str(PROJECT_DIR),
        "asset": ASSET_PATH,
        "csv_import_factory": "CSVImportFactory",
        "row_struct": row_struct.get_path_name(),
        "row": "bounce_ball",
        "column": "P0",
        "baseline_value": baseline_value,
        "reimported_value": updated_value,
        "restored_value": restored_value,
        "cpp_recompiled_between_imports": False,
        "scope": "DataTable import/readback only; gameplay runtime consumption is not implemented by this probe.",
    }


try:
    result = run_probe()
except Exception:
    result = {"status": "failed", "error": traceback.format_exc()}

REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
unreal.log("NAIWA_P4_DATATABLE_REIMPORT_PROBE " + json.dumps(result, ensure_ascii=False))
if result["status"] != "passed":
    raise RuntimeError(result["error"])
