import json
import pathlib
import unreal

result = {}
for path in [
    "/Game/LevelPrototyping/Materials/M_FlatCol",
    "/Game/LevelPrototyping/Materials/M_PrototypeGrid",
    "/Game/Naiwa/Gameplay/Materials/M_NaiwaBody_Gameplay",
]:
    asset = unreal.load_asset(path)
    entry = {"loaded": bool(asset), "class": asset.get_class().get_name() if asset else None}
    if asset:
        try:
            entry["vectors"] = [str(x) for x in unreal.MaterialEditingLibrary.get_vector_parameter_names(asset)]
        except Exception as e:
            entry["vector_error"] = str(e)
        try:
            entry["shading_model"] = str(asset.get_editor_property("shading_model"))
        except Exception as e:
            entry["shading_error"] = str(e)
    result[path] = entry

out = pathlib.Path(r"C:\Users\31314\Desktop\Nai-Long-Adventure\.workbuddy\work\20261026_P3方案\M1\material_inspection.json")
out.write_text(json.dumps(result, indent=2), encoding="utf-8")
unreal.log("NAIWA_P3_MATERIAL_INSPECTION " + json.dumps(result))
