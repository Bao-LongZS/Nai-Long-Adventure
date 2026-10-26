"""Persist P2 egg material usage flags required by skeletal meshes and morph targets."""
import json
import pathlib
import traceback

import unreal


ROOT = pathlib.Path(__file__).resolve().parents[2]
REPORT_PATH = ROOT / ".workbuddy/work/20261019_P2测试基线/M2b/egg_material_usage_result.json"
MATERIAL_ROOT = "/Game/Naiwa/P2/Art/Egg/Materials"
GEO_PARENT_PATH = "/Game/Naiwa/P2/Art/Geo/Materials/M_P2Geo_Solid"
BASE_MATERIALS = [
    "M_NaiwaEgg_Body",
    "M_NaiwaEgg_Eye",
    "M_NaiwaEgg_Pupil",
    "M_NaiwaEgg_Mouth",
]
INSTANCES = [
    "MI_NaiwaEgg_Base",
    "MI_NaiwaEgg_Chick",
    "MI_NaiwaEgg_Mouse",
    "MI_NaiwaEgg_Soy",
    "MI_NaiwaEgg_Tea",
]


def main():
    report = {
        "engine_version": unreal.SystemLibrary.get_engine_version(),
        "base_materials": [],
        "geo_material": None,
        "material_instances": [],
    }
    usages = (
        (unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, "skeletal_mesh_usage"),
        (unreal.MaterialUsage.MATUSAGE_MORPH_TARGETS, "morph_targets_usage"),
    )

    for asset_name in BASE_MATERIALS:
        path = f"{MATERIAL_ROOT}/{asset_name}"
        material = unreal.load_asset(path)
        if material is None or not isinstance(material, unreal.Material):
            raise RuntimeError(f"Expected a Material asset at {path}")
        for usage, _ in usages:
            unreal.MaterialEditingLibrary.set_base_material_usage(material, usage, True)
        errors = unreal.MaterialEditingLibrary.recompile_material(material)
        if errors:
            raise RuntimeError(f"Material compile errors for {path}: {errors}")
        if not unreal.EditorAssetLibrary.save_loaded_asset(material):
            raise RuntimeError(f"Could not save {path}")
        entry = {"path": material.get_path_name()}
        for usage, key in usages:
            entry[key] = bool(unreal.MaterialEditingLibrary.has_material_usage(material, usage))
            if not entry[key]:
                raise RuntimeError(f"Usage flag {key} did not persist on {path}")
        report["base_materials"].append(entry)

    geo_material = unreal.load_asset(GEO_PARENT_PATH)
    if geo_material is None or not isinstance(geo_material, unreal.Material):
        raise RuntimeError(f"Expected Geo parent material at {GEO_PARENT_PATH}")
    for usage, _ in usages:
        unreal.MaterialEditingLibrary.set_base_material_usage(geo_material, usage, True)
    errors = unreal.MaterialEditingLibrary.recompile_material(geo_material)
    if errors:
        raise RuntimeError(f"Material compile errors for {GEO_PARENT_PATH}: {errors}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(geo_material):
        raise RuntimeError(f"Could not save {GEO_PARENT_PATH}")
    geo_entry = {"path": geo_material.get_path_name()}
    for usage, key in usages:
        geo_entry[key] = bool(unreal.MaterialEditingLibrary.has_material_usage(geo_material, usage))
        if not geo_entry[key]:
            raise RuntimeError(f"Geo parent missing persisted usage flag {key}")
    report["geo_material"] = geo_entry

    expected_parent = f"{MATERIAL_ROOT}/M_NaiwaEgg_Body.M_NaiwaEgg_Body"
    for asset_name in INSTANCES:
        path = f"{MATERIAL_ROOT}/{asset_name}"
        instance = unreal.load_asset(path)
        if instance is None or not isinstance(instance, unreal.MaterialInstanceConstant):
            raise RuntimeError(f"Expected a MaterialInstanceConstant at {path}")
        parent = instance.get_editor_property("parent")
        parent_path = parent.get_path_name() if parent else None
        if parent_path != expected_parent:
            raise RuntimeError(f"Unexpected parent for {path}: {parent_path}")
        report["material_instances"].append({"path": instance.get_path_name(), "parent": parent_path})

    report["status"] = "PASS"
    unreal.log("NAIWA_P2_EGG_MATERIAL_USAGE_PASS")
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


try:
    main()
except Exception:
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps({"status": "FAIL", "error": traceback.format_exc()},
                                      ensure_ascii=False, indent=2), encoding="utf-8")
    raise
