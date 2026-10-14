"""Reimport only the pinned E2 mesh, with a backup and before/after checks.

Run after build_graybox.py and validate_import.py, using UnrealEditor-Cmd
<project> -run=PythonScript -script=<this file> -unattended -NullRHI -abslog=<path>.
Do not run concurrently with another editor for this project. Legacy FbxFactory
is specified explicitly. UE's skeletal reimport handler disables animation
import; existing renamed clips must remain byte-for-byte unchanged on disk.

The backup is a copy, never a deletion or move. On failure, inspect the report
and stop the editor before restoring any affected files from the backup.
"""
import datetime
import hashlib
import json
import math
import pathlib
import re
import shutil
import traceback

import unreal


ROOT = pathlib.Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
CONTENT = PROJECT / "Content/Naiwa"
RECORDS = ROOT / "UE5/Records"
SOURCE = ROOT / "NaiwaCharacter/versions/20261007_H2_E2_overall_refinement/candidate_E2/Naiwa_Character.fbx"
BLEND = SOURCE.with_suffix(".blend")
EXPECTED = {
    "fbx": "8dae9ce28c4b44465dcb8e73dd462804c3a561daee9eebaf79842900a3005147",
    "blend": "317f62278d63217636bf393f831e2fb41ce21acfefbe21beed6ad6d4dc75f322",
}
ACTIONS = ("Idle", "Walk", "Jump", "Laugh")
RECORD = RECORDS / "reimport_validation.json"
STAMP = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S_%fZ")
REPORT = {
    "phase": "mesh_only_reimport_validation",
    "timestamp_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
    "engine": unreal.SystemLibrary.get_engine_version(),
    "status": "running",
    "importer": "Legacy FbxFactory / ReimportFbxSkeletalMeshFactory",
    "checks": {},
    "warnings": [],
    "unverified": ["rendered material equivalence and runtime behavior after reimport"],
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check(name, condition, reason):
    REPORT["checks"][name] = {
        "status": "unverified" if condition is None else "passed" if condition else "failed",
        "reason": reason,
    }


def path_name(obj):
    return obj.get_path_name() if obj else None


def vec(value):
    return [float(value.x), float(value.y), float(value.z)]


def pose(value):
    q = value.rotation
    result = {"translation": vec(value.translation), "rotation": [q.x, q.y, q.z, q.w],
              "scale": vec(value.scale3d)}
    if not all(math.isfinite(v) for values in result.values() for v in values):
        raise ValueError("An animation pose contains non-finite values")
    if sum(v * v for v in result["rotation"]) < 1e-12 or any(abs(v) < 1e-12 for v in result["scale"]):
        raise ValueError("An animation pose has invalid quaternion or singular scale")
    return result


def sources():
    return {"fbx": {"path": str(SOURCE), "sha256": digest(SOURCE)},
            "blend": {"path": str(BLEND), "sha256": digest(BLEND)}}


def file_snapshot():
    result = {}
    # Include dependent blueprints/materials and all asset sidecars, not just AS paths.
    for file in sorted(CONTENT.rglob("*")):
        if file.is_file():
            result[file.relative_to(CONTENT).as_posix()] = {
                "sha256": digest(file), "bytes": file.stat().st_size,
            }
    return result


def tag_number(data, name):
    value = unreal.AssetRegistryHelpers.get_tag_value(data, name)
    if isinstance(value, tuple):
        if len(value) == 2 and isinstance(value[0], bool):
            if not value[0]:
                raise ValueError("Missing render count tag: " + name)
            value = value[1]
        elif len(value) == 1:
            value = value[0]
    if value is None or isinstance(value, bool) or value == "":
        raise ValueError("Missing render count tag: " + name)
    return int(value)


def graph(material):
    if not isinstance(material, unreal.Material):
        return {"material": path_name(material), "graph": None}
    node = unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
    return {"material": path_name(material),
            "base_color_node": node.get_class().get_name() if node else None,
            "base_color_output": unreal.MaterialEditingLibrary.get_material_property_input_node_output_name(
                material, unreal.MaterialProperty.MP_BASE_COLOR),
            "skeletal_mesh_usage": unreal.MaterialEditingLibrary.has_material_usage(
                material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)}


def snapshot(mesh):
    data = unreal.AssetRegistryHelpers.create_asset_data(mesh)
    component = unreal.SkeletalMeshComponent()
    component.set_skeletal_mesh_asset(mesh)
    names = [str(component.get_bone_name(i)) for i in range(component.get_num_bones())]
    bounds = mesh.get_imported_bounds()
    result = {
        "mesh": {"path": path_name(mesh), "skeleton": path_name(mesh.get_editor_property("skeleton")),
                 "bounds": {"origin": vec(bounds.origin), "extent": vec(bounds.box_extent)},
                 "has_vertex_colors": mesh.has_vertex_colors(),
                 "render_counts": {n: tag_number(data, n) for n in ("Vertices", "Triangles", "Bones", "LODs")},
                 "bones": [{"name": n, "parent": str(component.get_parent_bone(n))} for n in names],
                 "materials": [{"slot": str(s.material_slot_name), **graph(s.material_interface)}
                               for s in mesh.get_editor_property("materials")]},
        "animations": {},
        "dependent_references": {},
        "asset_paths": sorted(str(p) for p in unreal.EditorAssetLibrary.list_assets(
            "/Game/Naiwa", recursive=True, include_folder=False)),
    }
    for action in ACTIONS:
        asset = unreal.load_asset("/Game/Naiwa/AS_Naiwa_" + action)
        if not isinstance(asset, unreal.AnimSequence):
            raise RuntimeError("Missing expected clip: " + action)
        frames = unreal.AnimationLibrary.get_num_frames(asset)
        duration = asset.get_play_length()
        samples = [
            [pose(unreal.AnimationLibrary.get_bone_pose_for_frame(asset, n, frame, False)) for n in names]
            for frame in range(frames + 1)
        ]
        serialized = json.dumps(samples, sort_keys=True, separators=(",", ":"), allow_nan=False).encode("utf-8")
        result["animations"][action] = {
            "path": path_name(asset), "skeleton": path_name(asset.get_editor_property("skeleton")),
            "duration_seconds": duration, "frame_intervals": frames,
            "sampled_keys": unreal.AnimationLibrary.get_num_keys(asset),
            "tracks": [str(n) for n in unreal.AnimationLibrary.get_animation_track_names(asset)],
            "all_bone_all_frame_pose_sha256": hashlib.sha256(serialized).hexdigest(),
        }
    for name in ("ABP_Naiwa", "BP_NaiwaCharacter"):
        asset = unreal.load_asset("/Game/Naiwa/Blueprints/" + name)
        if asset is None:
            result["dependent_references"][name] = {"present": False}
            continue
        item = {"present": True, "path": path_name(asset)}
        if isinstance(asset, unreal.AnimBlueprint):
            item["target_skeleton"] = path_name(asset.get_editor_property("target_skeleton"))
        cls = unreal.BlueprintEditorLibrary.generated_class(asset)
        if cls:
            cdo = unreal.get_default_object(cls)
            for prop in ("skeletal_mesh_asset", "animation_class", "idle_sequence", "walk_sequence", "jump_sequence", "laugh_sequence"):
                try:
                    item[prop] = path_name(cdo.get_editor_property(prop))
                except Exception:
                    # ABP has no SkeletalMeshAsset/AnimationClass; character has both.
                    pass
        result["dependent_references"][name] = item
    return result


def reimport():
    REPORT["sources_before"] = sources()
    for kind, source in REPORT["sources_before"].items():
        if source["sha256"] != EXPECTED[kind]:
            raise RuntimeError("Pinned " + kind + " hash mismatch; reimport was not attempted")
    mesh = unreal.load_asset("/Game/Naiwa/SK_Naiwa")
    if not isinstance(mesh, unreal.SkeletalMesh):
        raise RuntimeError("Expected existing SkeletalMesh is unavailable")
    if not unreal.EditorAssetLibrary.does_asset_exist("/Game/Naiwa/Blueprints/BP_NaiwaCharacter"):
        raise RuntimeError("Run saved graybox setup before this stability test")
    baseline_path = RECORDS / "import_validation.json"
    if not baseline_path.exists():
        raise RuntimeError("Run validate_import.py before reimport validation")
    baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
    if baseline.get("status") != "import_data_verified_pending_visual_runtime_validation":
        raise RuntimeError("Baseline import data audit has not passed; fix baseline before reimport")
    REPORT["baseline_import_validation"] = {
        "path": str(baseline_path), "timestamp_utc": baseline.get("timestamp_utc"),
        "status": baseline.get("status"),
    }
    REPORT["before"] = snapshot(mesh)
    REPORT["files_before"] = file_snapshot()
    backup = RECORDS / "ReimportBackups" / STAMP / "Content/Naiwa"
    shutil.copytree(CONTENT, backup)
    REPORT["backup"] = {"path": str(backup), "files": len(REPORT["files_before"]),
                        "restore_instruction": "Stop all editors for NaiwaP0 before copying affected backup files back to Content/Naiwa."}
    RECORD.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2, allow_nan=False), encoding="utf-8")

    # The legacy reimport handler uses this existing object instead of the task's
    # temporary skeletal_mesh_import_data. Apply the pinned choices here.
    skin = mesh.get_editor_property("asset_import_data")
    if not isinstance(skin, unreal.FbxSkeletalMeshImportData):
        raise RuntimeError("Existing mesh import data is not legacy FbxSkeletalMeshImportData")
    skin.set_editor_properties({
        "vertex_color_import_option": unreal.VertexColorImportOption.REPLACE,
        "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS,
        "import_uniform_scale": 1.0, "convert_scene": True, "convert_scene_unit": True,
        "update_skeleton_reference_pose": False, "use_t0_as_ref_pose": False,
    })
    options = unreal.FbxImportUI()
    options.set_editor_properties({
        "automated_import_should_detect_type": False,
        "mesh_type_to_import": unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        "import_as_skeletal": True, "import_mesh": True, "import_animations": False,
        "import_materials": False, "import_textures": False, "create_physics_asset": False,
        "skeleton": mesh.get_editor_property("skeleton"),
        "reset_to_fbx_on_material_conflict": False,
    })
    task = unreal.AssetImportTask()
    task.set_editor_properties({
        "filename": str(SOURCE), "destination_path": "/Game/Naiwa", "destination_name": "SK_Naiwa",
        "automated": True, "replace_existing": True, "replace_existing_settings": False,
        "save": False, "options": options, "factory": unreal.FbxFactory(),
    })
    command_line = unreal.SystemLibrary.get_command_line()
    match = re.search(r'(?:^|\s)-abslog=(?:"([^"]+)"|([^\s]+))', command_line, re.IGNORECASE)
    log_path = pathlib.Path(match.group(1) or match.group(2)) if match else None
    log_offset = len(log_path.read_text(encoding="utf-8", errors="replace")) if log_path and log_path.exists() else 0
    unreal.log("NAIWA_MESH_ONLY_REIMPORT_BEGIN")
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.log("NAIWA_MESH_ONLY_REIMPORT_RETURNED")
    REPORT["imported_object_paths"] = [str(p) for p in task.get_editor_property("imported_object_paths")]
    REPORT["task_result_paths"] = [path_name(obj) for obj in task.get_objects()]
    check("task_returned_existing_mesh_only",
          REPORT["task_result_paths"] == ["/Game/Naiwa/SK_Naiwa.SK_Naiwa"],
          "The import task may return only the existing mesh, with no new Skeleton/clip/material objects")
    if log_path and log_path.exists():
        tail = log_path.read_text(encoding="utf-8", errors="replace")[log_offset:]
        REPORT["import_log_evidence"] = {
            "path": str(log_path),
            "atomic_reimport_logged": "Performing atomic reimport of" in tail,
            "import_success_logged": "-- imported successfully" in tail,
            "import_failure_logged": "-- import failed" in tail or "-- cannot reimport" in tail,
        }
        REPORT["warnings"] = [line for line in tail.splitlines() if "Warning:" in line]
        evidence = REPORT["import_log_evidence"]
        performed = (False if evidence["import_failure_logged"] else
                     True if evidence["atomic_reimport_logged"] and evidence["import_success_logged"] else None)
        check("actual_reimport_success_log", performed,
              "Factory returning an existing object is insufficient; require atomic reimport and successful import log messages")
    else:
        check("actual_reimport_success_log", None, "-abslog was unavailable; verify factory success in the external process log")
    after_mesh = unreal.load_asset("/Game/Naiwa/SK_Naiwa")
    REPORT["after"] = snapshot(after_mesh)
    before, after = REPORT["before"], REPORT["after"]
    for name in ("path", "skeleton", "bounds", "has_vertex_colors", "render_counts", "bones", "materials"):
        check("mesh." + name + ".stable", before["mesh"][name] == after["mesh"][name],
              "Mesh property compared before/after same-source reimport")
    for action in ACTIONS:
        check("clip." + action + ".data_unchanged", before["animations"][action] == after["animations"][action],
              "Path, Skeleton, duration, tracks, frame count and every-bone/every-frame pose fingerprint must remain unchanged")
    check("dependent_references_unchanged", before["dependent_references"] == after["dependent_references"],
          "ABP Skeleton and character mesh/class/sequence references must remain unchanged")
    check("asset_paths_unchanged", before["asset_paths"] == after["asset_paths"],
          "Mesh-only reimport must not create duplicate animation/material/skeleton assets")
    saved = unreal.EditorAssetLibrary.save_loaded_asset(after_mesh, only_if_is_dirty=False)
    check("mesh_saved", saved, "Save only the mesh; the four animation packages are not saved or reimported")
    REPORT["files_after"] = file_snapshot()
    changed = [name for name in sorted(set(REPORT["files_before"]) | set(REPORT["files_after"]))
               if REPORT["files_before"].get(name) != REPORT["files_after"].get(name)]
    REPORT["changed_content_files"] = changed
    for action in ACTIONS:
        prefix = "AS_Naiwa_" + action
        files = [name for name in REPORT["files_before"] if pathlib.PurePosixPath(name).stem == prefix]
        check("clip." + action + ".package_bytes_unchanged",
              bool(files) and all(REPORT["files_before"][name] == REPORT["files_after"].get(name) for name in files),
              "Every animation package/sidecar SHA256 is identical before and after the mesh reimport")
    unexpected = [name for name in changed if pathlib.PurePosixPath(name).stem != "SK_Naiwa"]
    check("only_mesh_package_changed", not unexpected,
          "Only SK_Naiwa package/sidecars may change; unrelated assets must remain byte-for-byte unchanged")
    REPORT["sources_after"] = sources()
    check("pinned_sources_unchanged", REPORT["sources_before"] == REPORT["sources_after"],
          "Original FBX and Blender source bytes remain unchanged")
    statuses = [item["status"] for item in REPORT["checks"].values()]
    REPORT["status"] = ("reimport_stability_failed" if "failed" in statuses else
                        "reimport_data_validation_incomplete" if "unverified" in statuses else
                        "mesh_reimport_data_stable_pending_visual_runtime_validation")


try:
    RECORDS.mkdir(parents=True, exist_ok=True)
    reimport()
except Exception:
    REPORT["status"] = "reimport_validation_failed"
    REPORT["fatal_error"] = traceback.format_exc()
finally:
    RECORD.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2, allow_nan=False), encoding="utf-8")
    unreal.log("NAIWA_REIMPORT_VALIDATION: " + REPORT["status"])
if REPORT["status"] in ("reimport_stability_failed", "reimport_validation_failed"):
    raise RuntimeError("Mesh reimport validation failed; inspect " + str(RECORD))
