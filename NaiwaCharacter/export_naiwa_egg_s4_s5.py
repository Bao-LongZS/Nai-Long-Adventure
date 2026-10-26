"""Add the S4 color variants and S5 skeletal FBX export to the reviewed S0-S3 egg."""
import hashlib
import json
import argparse
import pathlib
import shutil
import sys
import traceback

import bpy
from mathutils import Vector


ROOT = pathlib.Path(__file__).resolve().parents[1]
BLENDER_ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ARG_PARSER = argparse.ArgumentParser()
ARG_PARSER.add_argument("--source-blend", type=pathlib.Path)
ARG_PARSER.add_argument("--source-validation", type=pathlib.Path)
ARG_PARSER.add_argument("--params", type=pathlib.Path)
ARG_PARSER.add_argument("--output-dir", type=pathlib.Path)
ARGS, _ = ARG_PARSER.parse_known_args(BLENDER_ARGS)
SOURCE_BLEND = (ARGS.source_blend or ROOT / "NaiwaCharacter/deliveries/NaiwaEgg_S0-S3/NaiwaEgg_S0-S3.blend").resolve()
SOURCE_VALIDATION = (ARGS.source_validation or ROOT / "NaiwaCharacter/deliveries/NaiwaEgg_S0-S3/NaiwaEgg_S0-S3_validation.json").resolve()
PARAMS_FILE = (ARGS.params or ROOT / "NaiwaCharacter/parameters/NaiwaEgg_Parameters_v1.json").resolve()
OUTPUT_DIR = (ARGS.output_dir or ROOT / "NaiwaCharacter/deliveries/NaiwaEgg_S4-S5_v2").resolve()
BLEND_FILE = OUTPUT_DIR / "SK_NaiwaEgg_S4-S5.blend"
FBX_FILE = OUTPUT_DIR / "SK_NaiwaEgg.fbx"
REPORT_FILE = OUTPUT_DIR / "NaiwaEgg_S4-S5_export_report.json"
VARIANTS = {
    "Base": "#EF9F27",
    "Chick": "#F5C542",
    "Mouse": "#E6D9B8",
    "Soy": "#CFE0A8",
    "Tea": "#C89A6A",
}


def linear_channel(channel):
    value = channel / 255.0
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def linear_color(hex_value):
    value = hex_value.lstrip("#")
    return tuple(linear_channel(int(value[index:index + 2], 16)) for index in (0, 2, 4)) + (1.0,)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def set_body_material_color(material, hex_value):
    color = linear_color(hex_value)
    material.diffuse_color = color
    material.use_nodes = True
    shader = next((node for node in material.node_tree.nodes if node.type == "BSDF_PRINCIPLED"), None)
    if shader is None:
        raise RuntimeError(f"No Principled BSDF in {material.name}")
    shader.inputs["Base Color"].default_value = color


def render_variant(obj, body_slot_index, material, color_hex, variant, png_path):
    scene = bpy.context.scene
    set_body_material_color(material, color_hex)
    obj.material_slots[body_slot_index].material = material

    camera_data = bpy.data.cameras.new(f"NaiwaEgg_{variant}_PreviewCamera")
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = 1.5333333333
    camera = bpy.data.objects.new(f"NaiwaEgg_{variant}_PreviewCamera", camera_data)
    scene.collection.objects.link(camera)
    camera.location = (0.0, -3.8, 0.0)
    direction = Vector((0.0, 0.0, 0.0)) - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    scene.camera = camera
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 236
    scene.render.resolution_y = 236
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.film_transparent = False
    scene.render.filepath = str(png_path)
    scene.view_settings.view_transform = "Standard"
    bpy.context.view_layer.update()
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(camera, do_unlink=True)
    bpy.data.cameras.remove(camera_data)


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    if not SOURCE_BLEND.is_file() or not SOURCE_VALIDATION.is_file() or not PARAMS_FILE.is_file():
        raise FileNotFoundError("Reviewed S0-S3 model, validation, or v3 parameter file is missing")

    original_validation = json.loads(SOURCE_VALIDATION.read_text(encoding="utf-8"))
    params = json.loads(PARAMS_FILE.read_text(encoding="utf-8"))
    if params.get("variants") != VARIANTS:
        raise RuntimeError(f"Color palette does not match the active specification: {params.get('variants')}")
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE_BLEND))
    egg = bpy.data.objects.get("SK_NaiwaEgg")
    if egg is None or egg.type != "MESH":
        raise RuntimeError("Expected the reviewed SK_NaiwaEgg body mesh")
    if egg.data.shape_keys is None:
        raise RuntimeError("The reviewed egg mesh is missing its shape keys")

    body_slot_index = next((index for index, slot in enumerate(egg.material_slots)
                            if slot.material and slot.material.name.startswith("M_NaiwaEgg_Body")), None)
    if body_slot_index is None:
        raise RuntimeError("The egg has no body material slot")
    base_material = egg.material_slots[body_slot_index].material
    base_material.name = "M_NaiwaEgg_Body"
    set_body_material_color(base_material, VARIANTS["Base"])
    for slot in egg.material_slots:
        if slot.material and slot.material.name == "M_NaiwaEgg_EyeWhite":
            slot.material.name = "M_NaiwaEgg_Eye"

    # One non-animated Root is sufficient for Unreal to import the morphs as a Skeletal Mesh.
    rig_data = bpy.data.armatures.new("SK_NaiwaEgg_Skeleton")
    rig = bpy.data.objects.new("SK_NaiwaEgg_Skeleton", rig_data)
    bpy.context.scene.collection.objects.link(rig)
    bpy.context.view_layer.objects.active = rig
    rig.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    root_bone = rig_data.edit_bones.new("Root")
    root_bone.head = (0.0, 0.0, 0.0)
    root_bone.tail = (0.0, 0.0, 0.20)
    bpy.ops.object.mode_set(mode="OBJECT")
    rig.show_in_front = True

    for old_group in list(egg.vertex_groups):
        egg.vertex_groups.remove(old_group)
    root_group = egg.vertex_groups.new(name="Root")
    root_group.add(list(range(len(egg.data.vertices))), 1.0, "REPLACE")
    armature_modifier = egg.modifiers.new("NaiwaEgg_RootSkin", "ARMATURE")
    armature_modifier.object = rig
    armature_modifier.use_vertex_groups = True
    egg.parent = rig

    variant_materials = {}
    previews = []
    for name, color in VARIANTS.items():
        material = base_material.copy()
        material.name = f"MI_NaiwaEgg_{name}_Preview"
        set_body_material_color(material, color)
        variant_materials[name] = material
        preview = OUTPUT_DIR / f"NaiwaEgg_{name}_177px.png"
        render_variant(egg, body_slot_index, material, color, name, preview)
        previews.append({"variant": name, "color_hex": color, "image": str(preview), "sha256": sha256(preview)})

    readability_previews = []
    for filename, state, target_height in (
        ("NaiwaEgg_aim_96px.png", "aim_closed", 96),
        ("NaiwaEgg_flight_closed_177px.png", "flight_closed", 177),
        ("NaiwaEgg_flight_scream_177px.png", "flight_scream", 177),
    ):
        source_preview = SOURCE_BLEND.parent / filename
        if not source_preview.is_file():
            raise FileNotFoundError(f"Missing readability reference preview: {source_preview}")
        output_preview = OUTPUT_DIR / filename
        shutil.copy2(source_preview, output_preview)
        readability_previews.append({
            "state": state,
            "target_character_height_px": target_height,
            "image": str(output_preview),
            "sha256": sha256(output_preview),
        })

    # Store all five color swatches in the editable Blender asset. Keep Base assigned to the export mesh.
    egg.material_slots[body_slot_index].material = base_material
    egg.data.calc_loop_triangles()
    triangles = len(egg.data.loop_triangles)
    full_dimensions_m = list(egg.dimensions)
    actual_morphs = [key.name for key in egg.data.shape_keys.key_blocks]
    all_weights_valid = True
    for vertex in egg.data.vertices:
        root_weights = [assignment.weight for assignment in vertex.groups
                        if assignment.group == root_group.index]
        if len(root_weights) != 1 or abs(root_weights[0] - 1.0) > 1.0e-6:
            all_weights_valid = False
            break

    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 1.0
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.select_all(action="DESELECT")
    egg.select_set(True)
    rig.select_set(True)
    bpy.context.scene.render.resolution_x = 768
    bpy.context.scene.render.resolution_y = 768
    bpy.context.scene.render.resolution_percentage = 100
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_FILE))

    bpy.ops.export_scene.fbx(
        filepath=str(FBX_FILE),
        use_selection=True,
        object_types={"ARMATURE", "MESH"},
        use_mesh_modifiers=True,
        use_armature_deform_only=True,
        add_leaf_bones=False,
        armature_nodetype="NULL",
        bake_anim=False,
        path_mode="AUTO",
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_ALL",
        global_scale=100.0,
        axis_forward="-Y",
        axis_up="Z",
        use_custom_props=True,
    )

    maximum_triangles = int(params["mesh_budget"]["maximum_triangles"])
    checks = {
        "source_s0_s3_metrics_pass": original_validation.get("status") == "PASS_METRICS_VISUAL_REVIEW_PENDING",
        "source_is_active_v4": original_validation.get("spec_version") == "2026-10-19-v4",
        "dimensions_match_active_s0_s3": original_validation.get("body_dimensions_m") == [0.85, 0.8202499999999999, 1.15],
        "exact_three_morphs": actual_morphs == ["Basis", "EyeWide", "Scream", "Squash"],
        "single_root_bone": len(rig.data.bones) == 1 and rig.data.bones[0].name == "Root",
        "all_vertices_weighted_to_root": all_weights_valid,
        "triangle_budget": triangles <= maximum_triangles,
        "five_colors_match_active_spec": len(previews) == 5 and {item["color_hex"] for item in previews} == set(VARIANTS.values()),
        "readability_previews_present": len(readability_previews) == 3,
        "fbx_exported": FBX_FILE.is_file() and FBX_FILE.stat().st_size > 4096,
    }
    report = {
        "status": "PASS_S4S5_BLENDER_EXPORT_AWAITING_UE_IMPORT" if all(checks.values()) else "FAIL_S4S5",
        "source_blend": str(SOURCE_BLEND),
        "source_spec_version": original_validation.get("spec_version"),
        "source_blend_sha256": sha256(SOURCE_BLEND),
        "output_blend": str(BLEND_FILE),
        "output_blend_sha256": sha256(BLEND_FILE),
        "fbx": str(FBX_FILE),
        "fbx_sha256": sha256(FBX_FILE) if FBX_FILE.is_file() else None,
        "fbx_bytes": FBX_FILE.stat().st_size if FBX_FILE.is_file() else 0,
        "blender_version": bpy.app.version_string,
        "skeletal_mesh_name": "SK_NaiwaEgg",
        "skeleton_name": "SK_NaiwaEgg_Skeleton",
        "bone_names": [bone.name for bone in rig.data.bones],
        "vertex_count": len(egg.data.vertices),
        "triangle_count": triangles,
        "full_mesh_dimensions_m": full_dimensions_m,
        "morph_targets": actual_morphs,
        "body_variant_previews": previews,
        "readability_previews": readability_previews,
        "blender_preview_materials": [variant_materials[name].name for name in VARIANTS],
        "planned_unreal_material_instances": [f"MI_NaiwaEgg_{name}" for name in VARIANTS],
        "expected_import_dimensions_cm": {"width": 85.0, "depth": 82.025, "height": 115.0},
        "checks": checks,
    }
    REPORT_FILE.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print("NAIWA_EGG_S4_S5_EXPORT_" + report["status"] + " " + json.dumps(report, ensure_ascii=False))
    if not all(checks.values()):
        sys.exit(1)


try:
    main()
except Exception:
    print("NAIWA_EGG_S4_S5_EXPORT_FAIL " + traceback.format_exc())
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    REPORT_FILE.write_text(json.dumps({
        "status": "FAIL_S4S5",
        "source_blend": str(SOURCE_BLEND),
        "error": traceback.format_exc(),
    }, indent=2, ensure_ascii=False), encoding="utf-8")
    sys.exit(1)
