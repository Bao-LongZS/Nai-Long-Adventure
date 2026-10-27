"""Create the deterministic unlit color material used by the P3 M1 prototype."""
import json
import pathlib
import unreal


ASSET_PATH = "/Game/Naiwa/P3/Materials/M_P3M1FlatColor"
REPORT = pathlib.Path(__file__).resolve().parents[2] / ".workbuddy/work/20261026_P3方案/M1/material_build_report.json"

try:
    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P3/Materials")
    material = unreal.load_asset(ASSET_PATH)
    if not material:
        factory = unreal.MaterialFactoryNew()
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_P3M1FlatColor", "/Game/Naiwa/P3/Materials", unreal.Material, factory
        )
    assert material, "Could not create P3 M1 color material"
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    names = [str(name) for name in unreal.MaterialEditingLibrary.get_vector_parameter_names(material)]
    if "Color" not in names:
        color = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionVectorParameter, -400, 0
        )
        color.set_editor_property("parameter_name", "Color")
        color.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
        unreal.MaterialEditingLibrary.connect_material_property(
            color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
        )

    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_asset(ASSET_PATH, only_if_is_dirty=False), "Could not save material"
    result = {"status": "created_or_validated", "asset": ASSET_PATH, "shading_model": str(material.get_editor_property("shading_model")),
              "vector_parameters": [str(name) for name in unreal.MaterialEditingLibrary.get_vector_parameter_names(material)]}
except Exception as error:
    result = {"status": "failed", "error": str(error)}

REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
unreal.log("NAIWA_P3_M1_MATERIAL " + json.dumps(result, ensure_ascii=False))
if result["status"] != "created_or_validated":
    raise RuntimeError(result["error"])
