"""In-place import and validate the reviewed NaiwaEgg Skeletal Mesh in UE."""
import hashlib
import json
import os
import pathlib
import traceback

import unreal


ROOT = pathlib.Path(__file__).resolve().parents[2]
DELIVERY = pathlib.Path(os.environ.get(
    "NAIWA_EGG_DELIVERY_DIR", ROOT / "NaiwaCharacter/deliveries/NaiwaEgg_S4-S5_v2")).resolve()
FBX_FILE = pathlib.Path(os.environ.get("NAIWA_EGG_FBX_FILE", DELIVERY / "SK_NaiwaEgg.fbx")).resolve()
BLENDER_REPORT = DELIVERY / "NaiwaEgg_S4-S5_export_report.json"
REPORT_FILE = pathlib.Path(os.environ.get(
    "NAIWA_EGG_UE_REPORT_FILE", DELIVERY / "NaiwaEgg_S4-S5_ue_import_report.json")).resolve()
PARAMS_FILE = ROOT / "NaiwaCharacter/parameters/NaiwaEgg_Parameters_v1.json"
ASSET_DIR = os.environ.get("NAIWA_EGG_ASSET_DIR", "/Game/Naiwa/P2/Art/Egg")
MATERIAL_DIR = ASSET_DIR + "/Materials"
MORPH_NAMES = ["EyeWide", "Scream", "Squash"]
VARIANTS = {
    "Base": "#EF9F27",
    "Chick": "#F5C542",
    "Mouse": "#E6D9B8",
    "Soy": "#CFE0A8",
    "Tea": "#C89A6A",
}
SOLID_COLORS = {
    "M_NaiwaEgg_Eye": "#5E9B33",
    "M_NaiwaEgg_Pupil": "#14210E",
    "M_NaiwaEgg_Mouth": "#2A1A10",
}


def linear_channel(channel):
    value = channel / 255.0
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def linear_color(hex_value):
    value = hex_value.lstrip("#")
    return unreal.LinearColor(*(linear_channel(int(value[index:index + 2], 16))
                                for index in (0, 2, 4)), 1.0)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def save(asset):
    if asset is None or not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"Failed to save UE asset: {asset}")


def imported_bounds_dimensions(mesh):
    bounds = mesh.get_imported_bounds()
    extent = bounds.box_extent
    origin = bounds.origin
    return {
        "xyz_cm": [2.0 * extent.x, 2.0 * extent.y, 2.0 * extent.z],
        "sorted_cm": sorted([2.0 * extent.x, 2.0 * extent.y, 2.0 * extent.z]),
        "origin_cm": [origin.x, origin.y, origin.z],
    }


def imported_bone_hierarchy(mesh):
    """Return UE's imported hierarchy using the FBX armature node above Root, if present."""
    parent = str(mesh.get_bone_parent("Root"))
    start = parent if parent and parent != "None" else "Root"
    names = []
    pending = [start]
    while pending:
        name = pending.pop(0)
        if not name or name == "None" or name in names:
            continue
        names.append(name)
        pending.extend(str(child) for child in mesh.get_bone_children(name))
    return names


def imported_skin_weight_summary(mesh):
    """Verify UE kept all rigid vertex weights on the authored Root bone."""
    modifier = unreal.SkinWeightModifier()
    if not modifier.set_skeletal_mesh(mesh):
        raise RuntimeError("Could not inspect imported skeletal mesh skin weights")
    vertex_count = modifier.get_num_vertices()
    influences = {}
    all_weighted_to_root = vertex_count > 0
    wrapper_weight_total = 0.0
    for vertex_id in range(vertex_count):
        weights = {str(name): float(value)
                   for name, value in modifier.get_vertex_weights(vertex_id).items()
                   if float(value) > 1.0e-6}
        for name, value in weights.items():
            influences[name] = influences.get(name, 0) + 1
        wrapper_weight_total += weights.get("SK_NaiwaEgg_Skeleton", 0.0)
        if set(weights) != {"Root"} or abs(weights.get("Root", 0.0) - 1.0) > 1.0e-4:
            all_weighted_to_root = False
    return {
        "vertex_count": vertex_count,
        "weighted_vertex_counts_by_bone": influences,
        "all_vertices_weighted_to_authored_root": all_weighted_to_root,
        "wrapper_total_weight": wrapper_weight_total,
    }


def import_skeletal_mesh():
    unreal.EditorAssetLibrary.make_directory(ASSET_DIR)
    unreal.EditorAssetLibrary.make_directory(MATERIAL_DIR)

    # Replace in place and replace import settings. Keeping the package paths stable
    # avoids leaving stale loaded objects/redirectors between delete and FBX import.
    target_skeleton = unreal.load_asset(f"{ASSET_DIR}/SK_NaiwaEgg_Skeleton")
    if target_skeleton is None:
        raise RuntimeError("Expected the restored base egg Skeleton before reimport")
    options = unreal.FbxImportUI()
    options.set_editor_properties({
        "import_mesh": True,
        "import_as_skeletal": True,
        "import_animations": False,
        "import_materials": False,
        "import_textures": False,
        "create_physics_asset": False,
        "mesh_type_to_import": unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        "skeleton": target_skeleton,
    })
    skeletal_data = options.get_editor_property("skeletal_mesh_import_data")
    skeletal_data.set_editor_properties({
        "import_morph_targets": True,
        "import_uniform_scale": 100.0,
        "import_mesh_lods": False,
    })

    task = unreal.AssetImportTask()
    task.set_editor_properties({
        "filename": str(FBX_FILE),
        "destination_path": ASSET_DIR,
        "destination_name": "SK_NaiwaEgg",
        "automated": True,
        "replace_existing": True,
        "replace_existing_settings": True,
        "save": True,
        "options": options,
    })
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported_paths = list(task.get_editor_property("imported_object_paths"))
    mesh = unreal.load_asset(f"{ASSET_DIR}/SK_NaiwaEgg")
    if mesh is None or not isinstance(mesh, unreal.SkeletalMesh):
        raise RuntimeError(f"Expected SkeletalMesh at {ASSET_DIR}/SK_NaiwaEgg; imported={imported_paths}")
    return mesh, imported_paths


def main():
    if not FBX_FILE.is_file() or not BLENDER_REPORT.is_file():
        raise FileNotFoundError("The S4-S5 FBX and Blender export report must exist first")
    blender_report = json.loads(BLENDER_REPORT.read_text(encoding="utf-8"))
    if blender_report.get("status") != "PASS_S4S5_BLENDER_EXPORT_AWAITING_UE_IMPORT":
        raise RuntimeError("Blender export gate is not PASS")
    params = json.loads(PARAMS_FILE.read_text(encoding="utf-8"))
    if params.get("spec_version") != "2026-10-19-v4" or blender_report.get("source_spec_version") != "2026-10-19-v4":
        raise RuntimeError("UE import must use the active v4 model and material specification")

    mesh, imported_paths = import_skeletal_mesh()
    morph_names = sorted(str(name) for name in mesh.get_all_morph_target_names())
    dimensions = imported_bounds_dimensions(mesh)
    skeleton = mesh.get_editor_property("skeleton")
    bone_names = imported_bone_hierarchy(mesh)
    skin_weights = imported_skin_weight_summary(mesh)
    wrapper_name = "SK_NaiwaEgg_Skeleton"
    # The P2 plan requires exactly one Root bone. An unweighted FBX armature
    # node is still an extra UE bone and must not be accepted as a passing rig.
    expected_bone_hierarchy = bone_names == ["Root"]
    wrapper_is_inert = wrapper_name not in bone_names or (
        skin_weights["all_vertices_weighted_to_authored_root"] and
        skin_weights["wrapper_total_weight"] == 0.0
    )

    # Reuse the reviewed materials and MI assets. Rebuilding their expressions
    # during the same commandlet as skeletal import can hit rooted UObject asserts.
    body = unreal.load_asset(f"{MATERIAL_DIR}/M_NaiwaEgg_Body")
    if body is None:
        raise RuntimeError("Reviewed egg body material is missing")
    plain_materials = {name: unreal.load_asset(f"{MATERIAL_DIR}/{name}")
                       for name in SOLID_COLORS}
    if any(material is None for material in plain_materials.values()):
        raise RuntimeError("One or more reviewed egg face materials are missing")
    instances = {name: unreal.load_asset(f"{MATERIAL_DIR}/MI_NaiwaEgg_{name}")
                 for name in VARIANTS}
    if any(instance is None for instance in instances.values()):
        raise RuntimeError("One or more reviewed egg material instances are missing")

    slot_materials = {
        "M_NaiwaEgg_Body": instances["Base"],
        "M_NaiwaEgg_Body_Base": instances["Base"],
        "M_NaiwaEgg_Eye": plain_materials["M_NaiwaEgg_Eye"],
        "M_NaiwaEgg_EyeWhite": plain_materials["M_NaiwaEgg_Eye"],
        "M_NaiwaEgg_Pupil": plain_materials["M_NaiwaEgg_Pupil"],
        "M_NaiwaEgg_Mouth": plain_materials["M_NaiwaEgg_Mouth"],
    }
    skeletal_materials = list(mesh.get_editor_property("materials"))
    material_slots = []
    for skeletal_material in skeletal_materials:
        slot_name = str(skeletal_material.get_editor_property("material_slot_name"))
        material = slot_materials.get(slot_name)
        if material is None:
            raise RuntimeError(f"No material assigned for Skeletal Mesh slot {slot_name}")
        skeletal_material.set_editor_property("material_interface", material)
        material_slots.append({"slot_name": slot_name, "material": material.get_path_name()})
    mesh.set_editor_property("materials", skeletal_materials)
    save(mesh)

    instance_values = {}
    for name, instance in instances.items():
        value = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(instance, "BodyColor")
        instance_values[name] = [value.r, value.g, value.b, value.a]
        expected = linear_color(VARIANTS[name])
        if any(abs(actual - wanted) > 1.0e-5 for actual, wanted in zip(
                instance_values[name], (expected.r, expected.g, expected.b, expected.a))):
            raise RuntimeError(f"Reviewed MI color changed for {name}: {value}")

    full_mesh_dimensions_m = blender_report["full_mesh_dimensions_m"]
    expected_dimensions_xyz_cm = [value * 100.0 for value in full_mesh_dimensions_m]
    checks = {
        "skeletal_mesh_imported": isinstance(mesh, unreal.SkeletalMesh),
        "fbx_matches_export_manifest": blender_report.get("fbx_sha256") == sha256(FBX_FILE),
        "three_morph_targets_imported": morph_names == MORPH_NAMES,
        "authored_root_bone_imported": skeleton is not None and "Root" in bone_names,
        "single_root_bone_hierarchy": expected_bone_hierarchy,
        "triangle_count_within_p2_1800_budget": int(blender_report.get("triangle_count", 0)) <= 1800,
        "all_vertex_weights_remain_on_authored_root": skin_weights["all_vertices_weighted_to_authored_root"],
        "ue_armature_wrapper_is_inert": wrapper_is_inert,
        "import_dimensions_match_fbx_within_5mm": all(abs(a - e) <= 0.5 for a, e in
                                                       zip(dimensions["xyz_cm"], expected_dimensions_xyz_cm)),
        "four_material_slots_named": [slot["slot_name"] for slot in material_slots] == [
            "M_NaiwaEgg_Body", "M_NaiwaEgg_Eye", "M_NaiwaEgg_Pupil", "M_NaiwaEgg_Mouth"],
        "five_unreal_material_instances": len(instances) == 5,
        "no_physics_asset": mesh.get_editor_property("physics_asset") is None,
    }
    report = {
        "status": "PASS" if all(checks.values()) else "FAIL",
        "engine_version": unreal.SystemLibrary.get_engine_version(),
        "source_fbx": str(FBX_FILE),
        "source_fbx_sha256": sha256(FBX_FILE),
        "source_spec_version": blender_report.get("source_spec_version"),
        "imported_paths": imported_paths,
        "skeletal_mesh": mesh.get_path_name(),
        "skeleton": skeleton.get_path_name() if skeleton else None,
        "bone_names_from_hierarchy": bone_names,
        "bone_hierarchy_status": "PASS_SINGLE_ROOT" if bone_names == ["Root"]
            else "FAIL_EXTRA_UE_ARMATURE_WRAPPER" if bone_names == [wrapper_name, "Root"]
            else "FAIL_UNEXPECTED_HIERARCHY",
        "triangle_count": int(blender_report.get("triangle_count", 0)),
        "triangle_count": int(blender_report.get("triangle_count", 0)),
        "ue_armature_wrapper_imported": wrapper_name in bone_names,
        "ue_armature_wrapper_is_inert": wrapper_is_inert,
        "imported_skin_weights": skin_weights,
        "morph_targets": morph_names,
        "imported_dimensions": dimensions,
        "import_uniform_scale": 100.0,
        "expected_full_mesh_dimensions_cm_xyz": expected_dimensions_xyz_cm,
        "expected_body_dimensions_cm_xyz": [85.0, 82.025, 115.0],
        "material_slots": material_slots,
        "material_instances": {name: instance.get_path_name() for name, instance in instances.items()},
        "material_instance_body_color_linear": instance_values,
        "physics_asset": mesh.get_editor_property("physics_asset").get_path_name()
            if mesh.get_editor_property("physics_asset") else None,
        "checks": checks,
    }
    REPORT_FILE.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print("NAIWA_EGG_S4_S5_UE_IMPORT_" + report["status"] + " " + json.dumps(report, ensure_ascii=False))
    if not all(checks.values()):
        raise RuntimeError("UE S4-S5 import validation failed")


try:
    main()
except Exception:
    error = traceback.format_exc()
    try:
        report = json.loads(REPORT_FILE.read_text(encoding="utf-8"))
    except Exception:
        report = {}
    report["status"] = "FAIL"
    report["error"] = error
    REPORT_FILE.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print("NAIWA_EGG_S4_S5_UE_IMPORT_FAIL " + error)
    raise
