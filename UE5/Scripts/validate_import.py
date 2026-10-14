"""Read-only UE validation of the pinned E2 mesh and four imported clips.

Run with UnrealEditor-Cmd <project> -run=PythonScript -script=<this file>.
The only persistent write is UE5/Records/import_validation.json. This validates
asset data, not rendered appearance, character behavior, or measured performance.
"""
import datetime
import hashlib
import json
import math
import pathlib
import re
import traceback

import unreal


ROOT = pathlib.Path(__file__).resolve().parents[2]
RECORD_PATH = ROOT / "UE5/Records/import_validation.json"
SOURCE = ROOT / "NaiwaCharacter/versions/20261007_H2_E2_overall_refinement/candidate_E2/Naiwa_Character.fbx"
EXPECTED_HASH = "8dae9ce28c4b44465dcb8e73dd462804c3a561daee9eebaf79842900a3005147"
BLEND_SOURCE = SOURCE.with_suffix(".blend")
EXPECTED_BLEND_HASH = "317f62278d63217636bf393f831e2fb41ce21acfefbe21beed6ad6d4dc75f322"
SOURCE_BONES = ["Root", "Torso", "UpperArm.L", "Forearm.L", "UpperArm.R",
                "Forearm.R", "Thigh.L", "Shin.L", "Thigh.R", "Shin.R"]
ACTIONS = ("Idle", "Walk", "Jump", "Laugh")
# The pinned clips were created in build_naiwa.py: each pose_key writes zero
# rotation to omitted bones. Walk's constant nonzero Torso rotation still needs
# a track; a static pose that differs from reference must not be lost silently.
SOURCE_NON_REFERENCE_BONES = {
    "Idle": {"Torso"},
    "Walk": {"Torso", "UpperArm.L", "UpperArm.R", "Thigh.L", "Thigh.R", "Shin.L", "Shin.R"},
    "Jump": {"Torso", "Thigh.L", "Thigh.R"},
    "Laugh": {"Torso", "UpperArm.L", "UpperArm.R"},
}
TOLERANCE = {"translation_cm": 0.01, "rotation_degrees": 0.01, "scale": 0.00001}
REPORT = {
    "phase": "import_data_validation",
    "timestamp_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
    "engine": unreal.SystemLibrary.get_engine_version(),
    "status": "running",
    "checks": {},
    "errors": [],
    "mesh": {},
    "animations": [],
    "root_motion_tolerance": TOLERANCE,
    "unverified": ["rendered appearance and material equivalence to E2",
                   "per-vertex color values and posed foot geometry",
                   "input, capsule collision, jump and landing behavior",
                   "in-game frame time and memory", "reimport stability"],
}


def record_check(name, value, reason):
    """None means unavailable; never interpret unavailable evidence as success."""
    REPORT["checks"][name] = {
        "status": "unverified" if value is None else ("passed" if value else "failed"),
        "reason": reason,
    }


def attempt(label, callback, fallback=None):
    try:
        return callback()
    except Exception as exc:
        REPORT["errors"].append({"operation": label, "error": str(exc)})
        return fallback


def vector(value):
    return [float(value.x), float(value.y), float(value.z)]


def transform(value):
    # Transform's rotation is a quaternion; do not compare Euler angles near wraps.
    rotation = value.rotation
    pose = {"translation_cm": vector(value.translation),
            "rotation_quaternion_xyzw": [float(rotation.x), float(rotation.y),
                                         float(rotation.z), float(rotation.w)],
            "scale": vector(value.scale3d)}
    if not is_finite_pose(pose):
        raise ValueError("A sampled transform contains non-finite values")
    if sum(v * v for v in pose["rotation_quaternion_xyzw"]) < 1e-12:
        raise ValueError("A sampled transform has an invalid zero quaternion")
    if any(abs(v) < 1e-12 for v in pose["scale"]):
        raise ValueError("A sampled transform has singular scale")
    return pose


def is_finite_pose(pose):
    return all(math.isfinite(component)
               for values in pose.values() for component in values)


def distance(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def quaternion_distance_degrees(a, b):
    length_a = math.sqrt(sum(x * x for x in a))
    length_b = math.sqrt(sum(x * x for x in b))
    if length_a < 1e-12 or length_b < 1e-12:
        raise ValueError("A sampled root quaternion has zero length")
    cosine = abs(sum(x * y for x, y in zip(a, b)) / (length_a * length_b))
    return math.degrees(2 * math.acos(max(-1.0, min(1.0, cosine))))


def summarize_motion(samples):
    if not samples or any(sample is None for sample in samples):
        return {"status": "unverified", "reason": "Incomplete transform samples"}
    if not all(is_finite_pose(sample) for sample in samples):
        return {"status": "invalid", "reason": "A transform contains non-finite values"}
    first = samples[0]
    translation = max(distance(sample["translation_cm"], first["translation_cm"])
                      for sample in samples)
    rotation = max(quaternion_distance_degrees(sample["rotation_quaternion_xyzw"],
                                              first["rotation_quaternion_xyzw"])
                   for sample in samples)
    scale = max(max(abs(x - y) for x, y in zip(sample["scale"], first["scale"]))
                for sample in samples)
    fixed = (translation <= TOLERANCE["translation_cm"]
             and rotation <= TOLERANCE["rotation_degrees"]
             and scale <= TOLERANCE["scale"])
    return {"status": "fixed_within_tolerance" if fixed else "drift_detected",
            "sample_count": len(samples),
            "max_translation_from_first_cm": translation,
            "endpoint_translation_from_first_cm": distance(samples[-1]["translation_cm"], first["translation_cm"]),
            "max_rotation_from_first_degrees": rotation,
            "max_scale_component_change_from_first": scale}


def canonical_bone(name):
    # UE may sanitize FBX '.' into '_'. Match only an unambiguous whole name.
    return re.sub(r"[^a-z0-9]", "", str(name).lower())


def read_tag(asset_data, name):
    value = unreal.AssetRegistryHelpers.get_tag_value(asset_data, name)
    # Depending on Python binding generation, an out-parameter may be a tuple.
    if isinstance(value, tuple):
        if len(value) == 2 and isinstance(value[0], bool):
            if not value[0]:
                raise ValueError("Asset registry tag is absent: " + name)
            value = value[1]
        elif len(value) == 1:
            value = value[0]
    if value is None or isinstance(value, bool) or value == "":
        raise ValueError("Asset registry tag is absent: " + name)
    return int(value)


def audit_materials(mesh):
    """Read saved shader links; presence of a VertexColor node alone is insufficient."""
    result = []
    for index, slot in enumerate(mesh.get_editor_property("materials")):
        name = str(slot.material_slot_name)
        material = slot.material_interface
        item = {"index": index, "slot": name,
                "material": material.get_path_name() if material else None}
        result.append(item)
        low = name.lower()
        expected = ("M_NaiwaGlint" if "glint" in low else
                    "M_NaiwaEye" if "eye surface" in low else
                    "M_NaiwaMouth" if "mouth" in low else "M_NaiwaBody")
        item["expected_material"] = expected
        record_check("material_slot_" + str(index) + ".assigned",
                     material is not None and material.get_name() == expected,
                     "Saved mesh slot should reference the intended E2 P0 material")
        if not isinstance(material, unreal.Material):
            record_check("material_slot_" + str(index) + ".base_color_source", None,
                         "Base color graph is unavailable for a missing material or non-base material")
            continue
        node = attempt("material." + expected + ".base_color_input",
                       lambda m=material: unreal.MaterialEditingLibrary.get_material_property_input_node(
                           m, unreal.MaterialProperty.MP_BASE_COLOR))
        output = attempt("material." + expected + ".base_color_output",
                         lambda m=material: unreal.MaterialEditingLibrary.get_material_property_input_node_output_name(
                             m, unreal.MaterialProperty.MP_BASE_COLOR))
        usage = attempt("material." + expected + ".skeletal_mesh_usage",
                        lambda m=material: unreal.MaterialEditingLibrary.has_material_usage(
                            m, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH))
        item.update(base_color_node_class=node.get_class().get_name() if node else None,
                    base_color_output=output, skeletal_mesh_usage=usage)
        vertex_material = expected in ("M_NaiwaBody", "M_NaiwaEye")
        expected_class = (unreal.MaterialExpressionVertexColor if vertex_material
                          else unreal.MaterialExpressionConstant3Vector)
        record_check("material_slot_" + str(index) + ".base_color_source",
                     None if node is None else isinstance(node, expected_class),
                     "BaseColor is directly connected to VertexColor for skin/eyes and a color constant for glint/mouth")
        if node is not None:
            outputs = attempt("material." + expected + ".node_outputs",
                              lambda n=node: list(unreal.MaterialEditingLibrary.get_material_expression_output_names(n)))
            item["node_output_names"] = outputs
            # An empty output name is pin 0. VertexColor pin 0 is RGB in UE 5.8.
            if vertex_material:
                record_check("material_slot_" + str(index) + ".rgb_output",
                             None if output is None else str(output).lower() in ("", "rgb", "rgba"),
                             "Recorded output name selects VertexColor pin 0/RGB instead of an individual color channel")
            else:
                value = attempt("material." + expected + ".constant_color",
                                lambda n=node: n.get_editor_property("constant"))
                if value:
                    item["constant_linear_rgba"] = [value.r, value.g, value.b, value.a]
        record_check("material_slot_" + str(index) + ".skeletal_mesh_usage", usage,
                     "Saved material supports the skeletal mesh shader usage")
    record_check("five_material_slots", len(result) == 5,
                 "FBX combines the two glint meshes into one slot; there should be five imported material slots")
    return result


def sample_animation(asset, bone_names, parents, source_to_ue, reference_poses, action):
    name = asset.get_name()
    data = {"path": asset.get_path_name(), "class": asset.get_class().get_name(),
            "skeleton": None, "samples": [], "bone_motion": {}}
    skeleton = attempt(name + ".skeleton", lambda: asset.get_editor_property("skeleton"))
    if skeleton:
        data["skeleton"] = skeleton.get_path_name()
    duration = attempt(name + ".duration", asset.get_play_length)
    frames = attempt(name + ".frames", lambda: unreal.AnimationLibrary.get_num_frames(asset))
    keys = attempt(name + ".keys", lambda: unreal.AnimationLibrary.get_num_keys(asset))
    tracks = attempt(name + ".tracks", lambda: [str(n) for n in unreal.AnimationLibrary.get_animation_track_names(asset)])
    # FName equality is case-insensitive. UE returns lower-case track strings
    # while mesh bone display strings preserve the imported case.
    track_names = {track.casefold() for track in tracks} if tracks is not None else None
    data.update(duration_seconds=duration, frame_intervals=frames, sampled_keys=keys,
                tracks=tracks, sampling_note="GetNumFrames = sampled keys - 1; samples include both endpoints")
    if duration is not None and frames is not None and duration > 0:
        data["effective_sample_rate_hz"] = frames / duration
    else:
        data["effective_sample_rate_hz"] = None
    record_check(name + ".positive_duration_and_frames",
                 None if duration is None or frames is None else duration > 0 and frames > 0,
                 "Duration and frame intervals read through UE animation APIs")
    record_check(name + ".consistent_keys",
                 None if frames is None or keys is None else keys == frames + 1,
                 "Sampled keys should include the terminal frame")
    data["track_name_comparison"] = "UE FName semantics: exact full name, case-insensitive; punctuation is preserved"
    data["missing_source_bone_tracks"] = ([source for source, ue_name in source_to_ue.items()
                                           if ue_name.casefold() not in track_names]
                                          if track_names is not None else None)

    sampled_bones = []
    # Verify the actual UE root and every parent of E2 Root. An FBX rig object can
    # become a new skeletal root while the source Root remains its child.
    source_root = source_to_ue.get("Root")
    if source_root:
        cursor = source_root
        while cursor and cursor not in sampled_bones:
            sampled_bones.append(cursor)
            cursor = parents.get(cursor)
    sampled_bones.extend(b for b in bone_names if parents.get(b) is None and b not in sampled_bones)
    root_chain_bones = list(sampled_bones)
    torso = source_to_ue.get("Torso")
    if torso and torso not in sampled_bones:
        sampled_bones.append(torso)
    sampled_bones.extend(b for b in bone_names if b not in sampled_bones)
    data["sampled_bones"] = sampled_bones
    data["root_chain_bones"] = root_chain_bones

    if duration is None or frames is None or frames <= 0 or not bone_names:
        data["root_track_motion"] = {"status": "unverified", "reason": "Frame count, duration or skeleton unavailable"}
        record_check(name + ".root_chain_fixed", None, "Root transform sampling unavailable")
        record_check(name + ".source_bone_track_coverage", None, "Track coverage requires valid actual poses")
        return data

    for frame in range(frames + 1):
        time = float(duration) * frame / frames
        item = {"frame": frame, "time_seconds": time, "bone_local_transforms": {}}
        item["ue_root_track_transform"] = attempt(
            name + ".root_track.frame_" + str(frame),
            lambda: transform(unreal.AnimationLibrary.extract_root_track_transform(asset, time)))
        for bone_name in sampled_bones:
            item["bone_local_transforms"][bone_name] = attempt(
                name + "." + bone_name + ".frame_" + str(frame),
                lambda b=bone_name: transform(unreal.AnimationLibrary.get_bone_pose_for_frame(asset, b, frame, False)))
        data["samples"].append(item)
    data["root_track_motion"] = summarize_motion([s["ue_root_track_transform"] for s in data["samples"]])
    for bone_name in sampled_bones:
        data["bone_motion"][bone_name] = summarize_motion(
            [s["bone_local_transforms"][bone_name] for s in data["samples"]])
        data["bone_motion"][bone_name]["evidence"] = (
            "animation track" if track_names is not None and bone_name.casefold() in track_names
            else "reference pose fallback" if tracks is not None else "track coverage unknown")
        if track_names is not None and bone_name.casefold() not in track_names:
            reference = reference_poses.get(bone_name)
            samples = [s["bone_local_transforms"][bone_name] for s in data["samples"]]
            reference_comparison = summarize_motion([reference] + samples) if reference else {
                "status": "unverified", "reason": "Mesh reference pose is unavailable"}
            data["bone_motion"][bone_name]["reference_pose_comparison"] = reference_comparison
    actual_poses_complete = all(s["bone_local_transforms"][bone_name] is not None
                                for s in data["samples"] for bone_name in sampled_bones)
    record_check(name + ".all_bone_all_frame_poses_valid", actual_poses_complete,
                 "All 11 imported bones are sampled at every frame; transforms must be finite with nonzero quaternion and nonsingular scale")
    coverage = None
    if track_names is not None and len(source_to_ue) == len(SOURCE_BONES):
        coverage = actual_poses_complete
        for source, ue_name in source_to_ue.items():
            if ue_name.casefold() in track_names:
                continue
            # Only zero-rotation reference bones in the pinned source can omit a
            # track. Missing animated/constant-offset source poses are errors.
            expected_reference = source not in SOURCE_NON_REFERENCE_BONES[action]
            motion = data["bone_motion"][ue_name]
            matches_reference = motion.get("reference_pose_comparison", {}).get("status") == "fixed_within_tolerance"
            coverage = coverage and expected_reference and matches_reference
    record_check(name + ".source_bone_track_coverage", coverage,
                 "Each of the 10 source bones must exist and have valid sampled poses, with a case-insensitive matching track or a verified static reference-pose fallback expected by the pinned source action")
    expected_moving = SOURCE_NON_REFERENCE_BONES[action] - ({"Torso"} if action == "Walk" else set())
    data["expected_moving_source_bones"] = sorted(expected_moving)
    moving_valid = (all(data["bone_motion"][source_to_ue[source]]["status"] == "drift_detected"
                        for source in expected_moving)
                    if expected_moving.issubset(source_to_ue) else None)
    record_check(name + ".expected_animated_bones_move", moving_valid,
                 "Pinned source action's moving bones must actually vary across imported frames; valid static transforms alone do not prove animation survived")
    root_states = [data["root_track_motion"]["status"]]
    root_states.extend(data["bone_motion"][b]["status"] for b in data["root_chain_bones"])
    root_known = bool(data["root_chain_bones"]) and all(s in ("fixed_within_tolerance", "drift_detected") for s in root_states)
    record_check(name + ".root_chain_fixed",
                 all(s == "fixed_within_tolerance" for s in root_states) if root_known else None,
                 "Every sampled UE root and source Root ancestor transform is checked at every imported frame; Torso motion is recorded separately")
    return data


def validate():
    current_hash = attempt("source_sha256", lambda: hashlib.sha256(SOURCE.read_bytes()).hexdigest())
    REPORT["source"] = {"path": str(SOURCE), "sha256": current_hash}
    record_check("pinned_source_hash", None if current_hash is None else current_hash == EXPECTED_HASH,
                 "The imported source remains the formally pinned E2 FBX")
    blend_hash = attempt("blend_source_sha256", lambda: hashlib.sha256(BLEND_SOURCE.read_bytes()).hexdigest())
    REPORT["blend_source"] = {"path": str(BLEND_SOURCE), "sha256": blend_hash}
    record_check("pinned_blend_hash", None if blend_hash is None else blend_hash == EXPECTED_BLEND_HASH,
                 "The original E2 Blender file remains byte-for-byte unchanged")
    mesh = unreal.load_asset("/Game/Naiwa/SK_Naiwa")
    record_check("skeletal_mesh_present", isinstance(mesh, unreal.SkeletalMesh), "Load /Game/Naiwa/SK_Naiwa")
    if not isinstance(mesh, unreal.SkeletalMesh):
        raise RuntimeError("Expected SkeletalMesh is missing or has the wrong class")
    data = REPORT["mesh"]
    data.update(path=mesh.get_path_name(), class_name=mesh.get_class().get_name())
    skeleton = mesh.get_editor_property("skeleton")
    data["skeleton"] = skeleton.get_path_name() if skeleton else None
    data["has_vertex_colors"] = attempt("mesh.has_vertex_colors", mesh.has_vertex_colors)
    record_check("vertex_colors_present", data["has_vertex_colors"], "SkeletalMesh.has_vertex_colors reads the imported mesh flag")
    bounds = attempt("mesh.get_imported_bounds", mesh.get_imported_bounds)
    if bounds:
        data["imported_bounds_cm"] = {"origin": vector(bounds.origin), "extent": vector(bounds.box_extent),
                                      "size": [2 * value for value in vector(bounds.box_extent)],
                                      "sphere_radius": float(bounds.sphere_radius)}
    data["materials"] = audit_materials(mesh)
    asset_data = attempt("asset_data_from_loaded_mesh", lambda: unreal.AssetRegistryHelpers.create_asset_data(mesh))
    data["lod0_render_statistics"] = {"source": "USkeletalMesh asset registry tags from loaded UObject",
                                      "vertices": None, "triangles": None, "bones": None, "lods": None}
    for label, tag in (("vertices", "Vertices"), ("triangles", "Triangles"), ("bones", "Bones"), ("lods", "LODs")):
        if asset_data is not None:
            data["lod0_render_statistics"][label] = attempt("mesh.tag." + tag, lambda t=tag: read_tag(asset_data, t))
    statistics = data["lod0_render_statistics"]
    values = [statistics[k] for k in ("vertices", "triangles", "bones", "lods")]
    record_check("render_statistics_available", None if any(v is None for v in values) else all(v > 0 for v in values),
                 "Counts use UE render data tags, not source Blender counts")
    subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    editor_vertices = attempt("mesh.editor_lod0_vertices", lambda: subsystem.get_num_verts(mesh, 0))
    editor_lods = attempt("mesh.editor_lod_count", lambda: subsystem.get_lod_count(mesh))
    data["editor_statistics"] = {"lod0_vertices": editor_vertices, "lods": editor_lods,
                                 "source": "SkeletalMeshEditorSubsystem"}
    record_check("vertex_count_cross_check",
                 None if editor_vertices is None or statistics["vertices"] is None else editor_vertices == statistics["vertices"],
                 "Editor subsystem and asset registry must agree on LOD0 vertices")
    record_check("lod_count_cross_check",
                 None if editor_lods is None or statistics["lods"] is None else editor_lods == statistics["lods"],
                 "Editor subsystem and asset registry must agree on LOD count")

    # This transient component is never registered, spawned, or saved.
    component = unreal.SkeletalMeshComponent()
    component.set_skeletal_mesh_asset(mesh)
    bone_names = [str(component.get_bone_name(index)) for index in range(component.get_num_bones())]
    bone_set = set(bone_names)
    parents = {}
    reference_poses = {bone_name: attempt("mesh.reference_pose." + bone_name,
                                        lambda i=index: transform(component.get_ref_pose_transform(i)))
                       for index, bone_name in enumerate(bone_names)}
    data["reference_bone_local_transforms"] = reference_poses
    for bone_name in bone_names:
        parent = str(component.get_parent_bone(bone_name))
        parents[bone_name] = parent if parent in bone_set else None
    roots = [bone_name for bone_name in bone_names if parents[bone_name] is None]
    source_to_ue = {}
    for source_bone in SOURCE_BONES:
        matches = [name for name in bone_names if canonical_bone(name) == canonical_bone(source_bone)]
        if len(matches) == 1:
            source_to_ue[source_bone] = matches[0]
    extras = [name for name in bone_names if name not in source_to_ue.values()]
    extra_rig_roots = [name for name in extras if name in roots]
    data["bone_hierarchy"] = {"bones": bone_names, "parents": parents, "roots": roots,
                              "source_to_ue": source_to_ue, "extra_ue_bones": extras,
                              "extra_root_bones": extra_rig_roots,
                              "fbx_rig_root_present": bool(extra_rig_roots),
                              "source_root_parent": parents.get(source_to_ue.get("Root")),
                              "note": "Extra UE root bones are observed from the mesh; the rig object name is not inferred from animation asset names"}
    record_check("source_bones_preserved", len(source_to_ue) == len(SOURCE_BONES), "All 10 E2 source bones map to an unambiguous UE bone name")
    record_check("single_ue_skeleton_root", len(roots) == 1, "Mesh hierarchy enumerated through a transient SkeletalMeshComponent")
    record_check("bone_tag_matches_mesh", None if statistics["bones"] is None else statistics["bones"] == len(bone_names),
                 "Asset registry bone count agrees with mesh reference skeleton enumeration")

    for action in ACTIONS:
        asset = unreal.load_asset("/Game/Naiwa/AS_Naiwa_" + action)
        record_check(action + ".asset_present", isinstance(asset, unreal.AnimSequence), "Load expected renamed animation asset")
        if not isinstance(asset, unreal.AnimSequence):
            continue
        clip = sample_animation(asset, bone_names, parents, source_to_ue, reference_poses, action)
        clip["action"] = action
        REPORT["animations"].append(clip)
        record_check(action + ".shares_mesh_skeleton", clip["skeleton"] == data["skeleton"] and clip["skeleton"] is not None,
                     "Animation and mesh reference the same Skeleton object path")
    statuses = [check["status"] for check in REPORT["checks"].values()]
    if "failed" in statuses:
        REPORT["status"] = "import_data_checks_failed"
    elif "unverified" in statuses or REPORT["errors"]:
        REPORT["status"] = "import_data_validation_incomplete"
    else:
        REPORT["status"] = "import_data_verified_pending_visual_runtime_validation"


try:
    validate()
except Exception:
    REPORT["status"] = "import_data_validation_failed"
    REPORT["fatal_error"] = traceback.format_exc()
finally:
    RECORD_PATH.parent.mkdir(parents=True, exist_ok=True)
    RECORD_PATH.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2, allow_nan=False), encoding="utf-8")
    unreal.log("NAIWA_IMPORT_VALIDATION: " + REPORT["status"])
if REPORT["status"] in ("import_data_checks_failed", "import_data_validation_failed"):
    raise RuntimeError("E2 import validation failed; inspect " + str(RECORD_PATH))
