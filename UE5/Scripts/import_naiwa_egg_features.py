"""Import and validate the four feature-bearing P2 egg Skeletal Meshes in UE."""

import json
import os
import pathlib
import traceback

import unreal


ROOT = pathlib.Path(__file__).resolve().parents[2]
DELIVERY = ROOT / "NaiwaCharacter/deliveries/NaiwaEgg_Features_v1"
ASSET_DIR = "/Game/Naiwa/P2/Art/Egg"
MATERIAL_DIR = ASSET_DIR + "/Materials"
BASE_MESH_PATH = ASSET_DIR + "/SK_NaiwaEgg"
SKELETON_PATH = ASSET_DIR + "/SK_NaiwaEgg_Chick_Skeleton"
REPORT_FILE = DELIVERY / "NaiwaEgg_Features_UE_Import_Report.json"
BODY_MATERIALS = {
    "Chick": "/Game/Naiwa/P2/Art/Egg/Materials/MI_NaiwaEgg_Chick",
    "Mouse": "/Game/Naiwa/P2/Art/Egg/Materials/MI_NaiwaEgg_Mouse",
    "Soy": "/Game/Naiwa/P2/Art/Egg/Materials/MI_NaiwaEgg_Soy",
    "Tea": "/Game/Naiwa/P2/Art/Egg/Materials/MI_NaiwaEgg_Tea",
}
ACCENT_MATERIALS = {
    "Chick": ("MI_Accent_Crest", "#E8663C"),
    "Mouse": (None, None),
    "Soy": ("MI_Accent_Scarf", "#C0392B"),
    "Tea": ("MI_Accent_Straw", "#F5EAD2"),
}
MORPH_NAMES = ["EyeWide", "Scream", "Squash"]
EXPECTED_SLOTS = [
    "M_NaiwaEgg_Body",
    "M_NaiwaEgg_Eye",
    "M_NaiwaEgg_Pupil",
    "M_NaiwaEgg_Mouth",
    "M_NaiwaEgg_Accent",
]
BASE_SLOTS = EXPECTED_SLOTS[:4]


def sha256(path):
    import hashlib
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def linear_channel(channel):
    value = channel / 255.0
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def linear_color(hex_value):
    value = hex_value.lstrip("#")
    return unreal.LinearColor(*(linear_channel(int(value[index:index + 2], 16))
                                for index in (0, 2, 4)), 1.0)


def save(asset):
    if asset is None or not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"Failed to save UE asset: {asset}")


def make_or_update_accent_instance(name, color_hex, parent):
    path = MATERIAL_DIR + "/" + name
    instance = unreal.load_asset(path)
    if instance is None:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, MATERIAL_DIR, unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
    if instance is None:
        raise RuntimeError(f"Could not create material instance {path}")
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        instance, "BodyColor", linear_color(color_hex))
    unreal.MaterialEditingLibrary.update_material_instance(instance)
    value = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(instance, "BodyColor")
    expected = linear_color(color_hex)
    if any(abs(actual - wanted) > 1.0e-5 for actual, wanted in zip(
            (value.r, value.g, value.b, value.a),
            (expected.r, expected.g, expected.b, expected.a))):
        raise RuntimeError(f"Accent color did not persist for {name}: {value}")
    save(instance)
    return instance


def imported_bounds_dimensions(mesh):
    bounds = mesh.get_imported_bounds()
    extent = bounds.box_extent
    origin = bounds.origin
    return {
        "xyz_cm": [2.0 * extent.x, 2.0 * extent.y, 2.0 * extent.z],
        "origin_cm": [origin.x, origin.y, origin.z],
    }


def imported_skin_weight_summary(mesh):
    modifier = unreal.SkinWeightModifier()
    if not modifier.set_skeletal_mesh(mesh):
        raise RuntimeError(f"Could not inspect skin weights for {mesh.get_name()}")
    vertex_count = modifier.get_num_vertices()
    influences = {}
    all_root = vertex_count > 0
    for vertex_id in range(vertex_count):
        weights = {str(name): float(value)
                   for name, value in modifier.get_vertex_weights(vertex_id).items()
                   if float(value) > 1.0e-6}
        for name in weights:
            influences[name] = influences.get(name, 0) + 1
        if set(weights) != {"Root"} or abs(weights.get("Root", 0.0) - 1.0) > 1.0e-4:
            all_root = False
    return {
        "vertex_count": vertex_count,
        "weighted_vertex_counts_by_bone": influences,
        "all_vertices_weighted_to_root": all_root,
    }


def import_mesh(kind, skeleton):
    fbx_path = DELIVERY / f"SK_NaiwaEgg_{kind}.fbx"
    build_report_path = DELIVERY / f"SK_NaiwaEgg_{kind}_build_report.json"
    if not fbx_path.is_file() or not build_report_path.is_file():
        raise FileNotFoundError(f"Missing Blender output for {kind}")
    build_report = json.loads(build_report_path.read_text(encoding="utf-8"))
    if build_report.get("status") != "PASS_BLENDER_FEATURE_VARIANT":
        raise RuntimeError(f"Blender build report did not pass for {kind}")
    if build_report.get("fbx_sha256") != sha256(fbx_path):
        raise RuntimeError(f"Blender FBX checksum changed for {kind}")

    options = unreal.FbxImportUI()
    options.set_editor_properties({
        "import_mesh": True,
        "import_as_skeletal": True,
        "import_animations": False,
        "import_materials": False,
        "import_textures": False,
        "create_physics_asset": False,
        "mesh_type_to_import": unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        "skeleton": skeleton,
    })
    skeletal_data = options.get_editor_property("skeletal_mesh_import_data")
    skeletal_data.set_editor_properties({
        "import_morph_targets": True,
        "import_uniform_scale": 100.0,
        "import_mesh_lods": False,
    })
    asset_name = f"SK_NaiwaEgg_{kind}"
    asset_path = ASSET_DIR + "/" + asset_name
    task = unreal.AssetImportTask()
    task.set_editor_properties({
        "filename": str(fbx_path),
        "destination_path": ASSET_DIR,
        "destination_name": asset_name,
        "automated": True,
        "replace_existing": unreal.EditorAssetLibrary.does_asset_exist(asset_path),
        "replace_existing_settings": True,
        "save": True,
        "options": options,
    })
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported_paths = list(task.get_editor_property("imported_object_paths"))
    mesh = unreal.load_asset(asset_path)
    if mesh is None or not isinstance(mesh, unreal.SkeletalMesh):
        raise RuntimeError(f"Expected SkeletalMesh at {asset_path}; imported={imported_paths}")
    return mesh, imported_paths, build_report, fbx_path


def main():
    unreal.EditorAssetLibrary.make_directory(ASSET_DIR)
    unreal.EditorAssetLibrary.make_directory(MATERIAL_DIR)
    base_mesh = unreal.load_asset(BASE_MESH_PATH)
    if base_mesh is None or not isinstance(base_mesh, unreal.SkeletalMesh):
        raise RuntimeError(f"The approved base Skeletal Mesh is missing: {BASE_MESH_PATH}")

    skeleton = unreal.load_asset(SKELETON_PATH)
    if skeleton is None:
        skeleton = base_mesh.get_editor_property("skeleton")

    body_materials = {kind: unreal.load_asset(path) for kind, path in BODY_MATERIALS.items()}
    if any(material is None for material in body_materials.values()):
        missing = [kind for kind, material in body_materials.items() if material is None]
        raise RuntimeError(f"Existing body material instances are missing: {missing}")
    accent_parent = unreal.load_asset(MATERIAL_DIR + "/M_NaiwaEgg_Body")
    if accent_parent is None:
        raise RuntimeError("The existing M_NaiwaEgg_Body master material is missing")
    accent_instances = {}
    for kind, (name, color_hex) in ACCENT_MATERIALS.items():
        if name:
            accent_instances[kind] = make_or_update_accent_instance(name, color_hex, accent_parent)

    variant_reports = []
    for kind in ("Chick", "Mouse", "Soy", "Tea"):
        mesh, imported_paths, build_report, fbx_path = import_mesh(kind, skeleton)
        if skeleton is None:
            skeleton = mesh.get_editor_property("skeleton")
        if skeleton is None:
            raise RuntimeError(f"UE did not create a Skeleton while importing {kind}")
        save(skeleton)

        mesh_skeleton = mesh.get_editor_property("skeleton")
        if mesh_skeleton is None or mesh_skeleton.get_path_name() != skeleton.get_path_name():
            raise RuntimeError(f"{kind} did not reuse the shared Root Skeleton: {mesh_skeleton}")

        materials = list(mesh.get_editor_property("materials"))
        slot_names = [str(material.get_editor_property("material_slot_name")) for material in materials]
        expected_slots = BASE_SLOTS if kind == "Mouse" else EXPECTED_SLOTS
        if slot_names != expected_slots:
            raise RuntimeError(f"{kind} material slots differ from {len(expected_slots)}-slot contract: {slot_names}")
        materials[0].set_editor_property("material_interface", body_materials[kind])
        if kind in accent_instances:
            materials[4].set_editor_property("material_interface", accent_instances[kind])
        # UE drops the unused fifth FBX material slot for Mouse; its ears use the body slot.
        mesh.set_editor_property("materials", materials)
        save(mesh)

        morph_names = sorted(str(name) for name in mesh.get_all_morph_target_names())
        bones = list(build_report["root_bones"])
        weights = imported_skin_weight_summary(mesh)
        bounds = imported_bounds_dimensions(mesh)
        expected_body = build_report["body_bounds_m"]
        expected_feature = build_report["feature_bounds_m"]
        expected_xyz = [
            (max(expected_body[axis][1], expected_feature[axis][1]) -
             min(expected_body[axis][0], expected_feature[axis][0])) * 100.0
            for axis in ("x", "y", "z")
        ]
        asset_materials = list(mesh.get_editor_property("materials"))
        slot_materials = [{
            "slot_name": str(material.get_editor_property("material_slot_name")),
            "material": material.get_editor_property("material_interface").get_path_name()
                if material.get_editor_property("material_interface") else None,
        } for material in asset_materials]
        accent_slot_material = slot_materials[4]["material"] if len(slot_materials) > 4 else None
        checks = {
            "skeletal_mesh_imported": isinstance(mesh, unreal.SkeletalMesh),
            "shared_root_skeleton": mesh_skeleton.get_path_name() == skeleton.get_path_name(),
            "single_root_bone_hierarchy": bones == ["Root"] and
                set(weights["weighted_vertex_counts_by_bone"]) == {"Root"},
            "all_vertices_weighted_to_root": weights["all_vertices_weighted_to_root"],
            "three_named_morphs": morph_names == MORPH_NAMES,
            "material_slots_named": slot_names == expected_slots,
            "body_material_assigned": slot_materials[0]["material"] == body_materials[kind].get_path_name(),
            "accent_material_assigned": accent_slot_material ==
                (accent_instances[kind].get_path_name() if kind in accent_instances else None),
            "no_physics_asset": mesh.get_editor_property("physics_asset") is None,
            "triangle_budget": int(build_report["triangle_count"]) <= 2600 and
                bool(build_report.get("checks", {}).get("triangle_budget")),
            "import_bounds_match_export": all(abs(actual - wanted) <= 0.5
                                               for actual, wanted in zip(bounds["xyz_cm"], expected_xyz)),
            "squash_silhouette_break_8px": build_report["squash_silhouette_break_px_at_96px_body"] >= 8.0,
        }
        report = {
            "kind": kind,
            "fbx": str(fbx_path),
            "fbx_sha256": sha256(fbx_path),
            "imported_paths": imported_paths,
            "skeletal_mesh": mesh.get_path_name(),
            "skeleton": skeleton.get_path_name(),
            "bone_names": bones,
            "morph_targets": morph_names,
            "vertex_count": weights["vertex_count"],
            "weighted_vertex_counts_by_bone": weights["weighted_vertex_counts_by_bone"],
            "triangle_count_from_verified_fbx": int(build_report["triangle_count"]),
            "imported_bounds": bounds,
            "expected_import_bounds_cm": expected_xyz,
            "material_slots": slot_materials,
            "physics_asset": mesh.get_editor_property("physics_asset").get_path_name()
                if mesh.get_editor_property("physics_asset") else None,
            "squash_silhouette_break_px_at_96px_body": build_report["squash_silhouette_break_px_at_96px_body"],
            "checks": checks,
        }
        variant_reports.append(report)
        if not all(checks.values()):
            raise RuntimeError(f"UE validation failed for {kind}: {checks}")

    # The legacy Base mesh remains untouched; only the four new special meshes share this Root Skeleton.
    save(skeleton)
    summary = {
        "status": "PASS_ALL_FOUR_SPECIAL_EGGS_IMPORTED",
        "engine_version": unreal.SystemLibrary.get_engine_version(),
        "base_mesh_preserved": BASE_MESH_PATH,
        "shared_skeleton": skeleton.get_path_name(),
        "accent_materials": {kind: asset.get_path_name() for kind, asset in accent_instances.items()},
        "variants": variant_reports,
    }
    REPORT_FILE.write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log("NAIWA_EGG_FEATURES_UE_IMPORT_PASS " + json.dumps(summary, ensure_ascii=False))
    print("NAIWA_EGG_FEATURES_UE_IMPORT_PASS " + json.dumps(summary, ensure_ascii=False))


try:
    main()
except Exception:
    error = traceback.format_exc()
    report = {"status": "FAIL_SPECIAL_EGG_UE_IMPORT", "error": error}
    DELIVERY.mkdir(parents=True, exist_ok=True)
    REPORT_FILE.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log_error("NAIWA_EGG_FEATURES_UE_IMPORT_FAIL " + error)
    raise
