"""W1 proof: import three P4 upgrade rows into a native DataTable via Editor Python."""
from pathlib import Path
import json
import traceback

import unreal


ROOT = Path(__file__).resolve().parents[2]
CSV_PATH = ROOT / "UE5/SourceAssets/P4/Data/P4UpgradeSmoke.csv"
ASSET_PATH = "/Game/Naiwa/P4/Data/DT_P4_Upgrade_Smoke"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/datatable_probe.json"


def run_probe() -> dict:
    if not CSV_PATH.is_file():
        raise FileNotFoundError(f"Missing P4 CSV source: {CSV_PATH}")

    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P4/Data")
    row_struct_type = unreal.NaiwaP4UpgradeRow
    row_struct = row_struct_type.static_struct()
    # DataTableFactory only creates a new, empty table. CSV files must use
    # CSVImportFactory, whose automated settings carry the selected row type.
    factory = unreal.CSVImportFactory()
    import_settings = factory.get_editor_property("automated_import_settings")
    import_settings.set_editor_property("import_row_struct", row_struct)
    factory.set_editor_property("automated_import_settings", import_settings)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(CSV_PATH))
    task.set_editor_property("destination_path", "/Game/Naiwa/P4/Data")
    task.set_editor_property("destination_name", "DT_P4_Upgrade_Smoke")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("factory", factory)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    table = unreal.load_asset(ASSET_PATH)
    if not table:
        raise RuntimeError(f"DataTable import did not create {ASSET_PATH}; imported={task.get_editor_property('imported_object_paths')}")
    columns = (
        "Title", "EffectKind", "P0", "P1", "P2", "bUnique",
        "PrerequisiteCardIds", "ExcludedCardIds", "RedlineGroup", "Rarity", "IconSoftRef",
    )
    actual_row_struct = unreal.DataTableFunctionLibrary.get_data_table_row_struct(table)
    actual_row_struct_path = actual_row_struct.get_path_name() if actual_row_struct else ""
    if actual_row_struct_path != row_struct.get_path_name():
        raise RuntimeError(
            f"DataTable row struct is {actual_row_struct_path}; expected {row_struct.get_path_name()}"
        )
    column_names = sorted(
        str(name) for name in unreal.DataTableFunctionLibrary.get_data_table_column_names(table)
    )
    expected_columns = sorted(columns)
    if column_names != expected_columns:
        raise RuntimeError(f"Unexpected DataTable columns: {column_names}; expected {expected_columns}")

    # DataTable rows are stored in a map; normalize the API result instead of
    # treating its iteration order as part of the import contract.
    row_names = sorted(str(name) for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(table))
    expected = sorted(["bounce_ball", "rounder_belly", "cant_hold"])
    if row_names != expected:
        raise RuntimeError(f"Unexpected DataTable row names: {row_names}; expected {expected}")

    # Both APIs walk the same DataTable row map, so values align with the
    # unsorted names above. Keep that mapping before sorting for the report.
    raw_row_names = [str(name) for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
    column_values = {
        column: [str(value) for value in unreal.DataTableFunctionLibrary.get_data_table_column_as_string(
            table, unreal.Name(column)
        )]
        for column in columns
    }
    if any(len(values) != len(raw_row_names) for values in column_values.values()):
        raise RuntimeError(
            "DataTable column readback count mismatch: "
            + json.dumps({"rows": len(raw_row_names), **{key: len(value) for key, value in column_values.items()}})
        )
    rows = {
        row_name: {column: column_values[column][index] for column in columns}
        for index, row_name in enumerate(raw_row_names)
    }
    expected_values = {
        "bounce_ball": {"EffectKind": "projectile_bounce", "P0": 1.0, "P1": 0.40, "P2": 690.0, "RedlineGroup": "low"},
        "rounder_belly": {"EffectKind": "charge_capacity_recovery", "P0": 3.0, "P1": 0.34, "P2": 0.0, "RedlineGroup": "medium"},
        "cant_hold": {"EffectKind": "laugh_energy_on_hit", "P0": 15.0, "P1": 0.9, "P2": 0.0, "RedlineGroup": "medium"},
    }
    for row_name, expected_row in expected_values.items():
        actual = rows[row_name]
        for column, expected_value in expected_row.items():
            if column in ("P0", "P1", "P2"):
                if abs(float(actual[column]) - expected_value) > 1e-5:
                    raise RuntimeError(f"DataTable row {row_name}.{column}={actual[column]}, expected {expected_value}")
            elif actual[column].casefold() != expected_value.casefold():
                raise RuntimeError(f"DataTable row {row_name}.{column}={actual[column]}, expected {expected_value}")
        if rows[row_name]["bUnique"].casefold() != "true":
            raise RuntimeError(f"DataTable row {row_name}.bUnique={rows[row_name]['bUnique']}, expected True")
        for column in ("PrerequisiteCardIds", "ExcludedCardIds"):
            if rows[row_name][column].strip():
                raise RuntimeError(f"DataTable row {row_name}.{column} should be empty, got {rows[row_name][column]}")

    return {
        "status": "passed",
        "asset": ASSET_PATH,
        "row_struct": row_struct.get_path_name(),
        "columns": column_names,
        "factory": "CSVImportFactory",
        "imported_object_paths": [str(path) for path in task.get_editor_property("imported_object_paths")],
        "row_names": row_names,
        "row_count": len(row_names),
        "rows": rows,
        "cpp_compile_after_csv_edit": False,
    }


try:
    result = run_probe()
except Exception:
    result = {"status": "failed", "error": traceback.format_exc()}

REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
unreal.log("NAIWA_P4_DATATABLE_PROBE " + json.dumps(result, ensure_ascii=False))
if result["status"] != "passed":
    raise RuntimeError(result["error"])
