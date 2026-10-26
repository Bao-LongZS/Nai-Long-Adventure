"""Independently re-import the generated FBX in Blender and inspect its rig, scale, and morphs."""
import argparse
import json
import pathlib
import sys
import traceback

import bpy


ROOT = pathlib.Path(__file__).resolve().parents[1]
BLENDER_ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ARG_PARSER = argparse.ArgumentParser()
ARG_PARSER.add_argument("--delivery-dir", type=pathlib.Path)
ARG_PARSER.add_argument("--fbx-file", type=pathlib.Path)
ARGS, _ = ARG_PARSER.parse_known_args(BLENDER_ARGS)
DELIVERY = (ARGS.delivery_dir or ROOT / "NaiwaCharacter/deliveries/NaiwaEgg_S4-S5_v2").resolve()
FBX_FILE = (ARGS.fbx_file or DELIVERY / "SK_NaiwaEgg.fbx").resolve()
REPORT_FILE = DELIVERY / "NaiwaEgg_FBX_roundtrip_report.json"


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(FBX_FILE), use_anim=False)
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    armatures = [obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"]
    mesh_info = []
    for obj in meshes:
        keys = [key.name for key in obj.data.shape_keys.key_blocks] if obj.data.shape_keys else []
        morph_delta_max = {}
        if obj.data.shape_keys:
            basis = obj.data.shape_keys.key_blocks.get("Basis")
            for key in obj.data.shape_keys.key_blocks:
                if key.name == "Basis" or basis is None:
                    continue
                morph_delta_max[key.name] = max(
                    (key.data[index].co - basis.data[index].co).length
                    for index in range(len(key.data)))
        weighted_vertex_counts = {}
        all_vertices_weighted_to_root = bool(obj.data.vertices)
        for vertex in obj.data.vertices:
            weights = {}
            for assignment in vertex.groups:
                if assignment.weight > 1.0e-6:
                    group_name = obj.vertex_groups[assignment.group].name
                    weights[group_name] = assignment.weight
                    weighted_vertex_counts[group_name] = weighted_vertex_counts.get(group_name, 0) + 1
            if set(weights) != {"Root"} or abs(weights.get("Root", 0.0) - 1.0) > 1.0e-4:
                all_vertices_weighted_to_root = False
        mesh_info.append({
            "object": obj.name,
            "shape_keys": keys,
            "morph_delta_max_m": morph_delta_max,
            "dimensions_m": list(obj.dimensions),
            "vertex_count": len(obj.data.vertices),
            "weighted_vertex_counts_by_bone": weighted_vertex_counts,
            "all_vertices_weighted_to_root": all_vertices_weighted_to_root,
            "material_slots": [material.name if material else None for material in obj.data.materials],
        })
    report = {
        "blender_version": bpy.app.version_string,
        "fbx": str(FBX_FILE),
        "mesh_count": len(meshes),
        "armature_count": len(armatures),
        "armature_bones": [bone.name for arm in armatures for bone in arm.data.bones],
        "meshes": mesh_info,
        "checks": {
            "one_mesh": len(meshes) == 1,
            "one_root_bone": len(armatures) == 1 and [bone.name for bone in armatures[0].data.bones] == ["Root"],
            "all_vertices_weighted_to_root": len(mesh_info) == 1 and mesh_info[0]["all_vertices_weighted_to_root"],
            "three_named_morphs": len(mesh_info) == 1 and mesh_info[0]["shape_keys"] == ["Basis", "EyeWide", "Scream", "Squash"],
        },
    }
    report["status"] = "PASS" if all(report["checks"].values()) else "FAIL"
    REPORT_FILE.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print("NAIWA_EGG_FBX_ROUNDTRIP_" + report["status"] + " " + json.dumps(report, ensure_ascii=False))
    if report["status"] != "PASS":
        sys.exit(1)


try:
    main()
except Exception:
    failure = {"status": "FAIL", "error": traceback.format_exc()}
    REPORT_FILE.write_text(json.dumps(failure, indent=2, ensure_ascii=False), encoding="utf-8")
    print("NAIWA_EGG_FBX_ROUNDTRIP_FAIL " + failure["error"])
    sys.exit(1)
