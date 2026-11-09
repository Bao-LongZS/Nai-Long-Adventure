"""Import the P4 W3 enemy and wave CSV sources as native Unreal DataTables."""
from pathlib import Path
import json
import traceback

import unreal


ROOT = Path(__file__).resolve().parents[2]
DATA_DIR = ROOT / "UE5/SourceAssets/P4/Data"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/W3/p4_w3_datatable_import_report.json"


def import_table(csv_name: str, asset_name: str, row_struct_type: type) -> dict:
    csv_path = DATA_DIR / csv_name
    if not csv_path.is_file():
        raise FileNotFoundError(f"Missing W3 CSV source: {csv_path}")

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
    if not row_names:
        raise RuntimeError(f"{asset_name} has no imported rows")
    return {
        "asset": asset_path,
        "row_struct": actual_struct.get_path_name(),
        "row_count": len(row_names),
        "row_names": row_names,
        "imported_object_paths": [str(path) for path in task.get_editor_property("imported_object_paths")],
    }


try:
    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P4/Data")
    result = {
        "status": "passed",
        "factory": "CSVImportFactory",
        "enemy_table": import_table("P4EnemyAlpha.csv", "DT_P4_Enemy_Alpha", unreal.NaiwaP4EnemyRow),
        "wave_table": import_table("P4WaveAlpha.csv", "DT_P4_Waves_Alpha", unreal.NaiwaP4WaveSpawnRow),
    }
except Exception:
    result = {"status": "failed", "error": traceback.format_exc()}

REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
unreal.log("NAIWA_P4_W3_DATATABLE_IMPORT " + json.dumps(result, ensure_ascii=False))
if result["status"] != "passed":
    raise RuntimeError(result["error"])
