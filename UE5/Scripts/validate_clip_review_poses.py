"""Inspect saved held-phase UE transforms without launching Unreal or altering assets."""
from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import math
from datetime import datetime, timezone
from pathlib import Path


def vector(value):
    return tuple(float(value[key]) for key in ("x", "y", "z"))


def distance(first, second):
    return math.sqrt(sum((a - b) ** 2 for a, b in zip(first, second)))


def quaternion_angle_degrees(first, second):
    first_norm = math.sqrt(sum(value * value for value in first))
    second_norm = math.sqrt(sum(value * value for value in second))
    if first_norm < 1e-12 or second_norm < 1e-12:
        raise ValueError("A sampled rotation quaternion has zero magnitude")
    # q and -q represent the same rotation; normalize before computing the dot.
    dot = abs(sum(a * b for a, b in zip(first, second)) / (first_norm * second_norm))
    return math.degrees(2.0 * math.acos(min(1.0, max(0.0, dot))))


def bone_map(entry):
    return {bone["name"]: bone for bone in entry["component_space_bones"]}


def pose_difference(first, second):
    first_bones, second_bones = bone_map(first), bone_map(second)
    if set(first_bones) != set(second_bones):
        raise ValueError("Pose comparison found inconsistent bone names")
    rows = []
    for name, bone in first_bones.items():
        other = second_bones[name]
        rows.append({
            "bone": name,
            "position_difference_component_cm": distance(vector(bone["position_cm"]), vector(other["position_cm"])),
            "rotation_difference_degrees": quaternion_angle_degrees(bone["quaternion_xyzw"], other["quaternion_xyzw"]),
        })
    return {
        "from_phase": first["phase_index"],
        "to_phase": second["phase_index"],
        "from_time_s": first["sample_time_s"],
        "to_time_s": second["sample_time_s"],
        "maximum_bone_position_difference_component_cm": max(row["position_difference_component_cm"] for row in rows),
        "maximum_bone_rotation_difference_degrees": max(row["rotation_difference_degrees"] for row in rows),
        "bone_differences": rows,
        "surface_min_world_z_difference_cm": abs(first["surface_min_world_cm"]["z"] - second["surface_min_world_cm"]["z"]),
        "front_extent_difference_cm": abs(first["front_extent_from_actor_cm"] - second["front_extent_from_actor_cm"]),
        "rear_extent_difference_cm": abs(first["rear_extent_from_actor_cm"] - second["rear_extent_from_actor_cm"]),
        "morph_weight_maximum_difference": max((abs(value - second["final_component_morph_weights"][name])
            for name, value in first["final_component_morph_weights"].items()), default=0.0),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source_bytes = args.manifest.read_bytes()
    manifest = json.loads(source_bytes.decode("utf-8-sig"))
    entries = manifest["entries"]
    if not manifest["capture_complete"] or len(entries) != 80:
        raise ValueError("Expected the completed five-clip, 80-entry phase capture")
    clips = list(dict.fromkeys(row["clip"] for row in entries))
    if len(clips) != 5:
        raise ValueError("Expected five distinct reviewed clip assets")
    report = {
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "source_manifest": str(args.manifest.resolve()),
        "source_manifest_sha256": hashlib.sha256(source_bytes).hexdigest(),
        "method": "Read stored UE component-space positions/quaternions and world surface extrema; 12 held phases plus two near-end/start pairs per clip.",
        "scope": "Sampled transform data only. No Unreal launch, model change, input validation, continuous playback, new skinning evaluation, or P0 completion claim.",
        "limits": [
            "Positions are stored component-space centimeters before gameplay component scaling; they are not world-space foot positions.",
            "Only position and quaternion were recorded per bone; scale validity cannot be established from this manifest.",
            "Seam samples are at clip_length-0.00001 seconds versus time zero, rather than the exact terminal sample.",
            "Surface minima span all CPU-skinned vertices. A foot region attribution requires image review; no per-foot support or slip measurement is available.",
            "Twelve held phases cannot establish inter-frame acceleration, temporal smoothness, or skin self-intersection absence.",
            "Jump/Laugh are original demonstration sequences, not automatically certified as intended seamless locomotion loops.",
        ],
        "entry_count": len(entries),
        "clips": [],
    }
    for clip in clips:
        rows = sorted((entry for entry in entries if entry["clip"] == clip), key=lambda entry: entry["phase_index"])
        if [entry["phase_index"] for entry in rows] != list(range(16)):
            raise ValueError(f"{clip}: expected distinct phase indices 0..15")
        names = list(bone_map(rows[0]))
        if len(names) != 11 or any(list(bone_map(row)) != names for row in rows):
            raise ValueError(f"{clip}: inconsistent 11-bone hierarchy sample")
        all_values = [value for row in rows for bone in row["component_space_bones"]
                      for value in (*vector(bone["position_cm"]), *bone["quaternion_xyzw"])]
        if not all(math.isfinite(value) for value in all_values):
            raise ValueError(f"{clip}: non-finite recorded transform")
        primary = rows[:12]
        movement = []
        for name in names:
            poses = [bone_map(row)[name] for row in primary]
            positions = [vector(bone["position_cm"]) for bone in poses]
            rotations = [bone["quaternion_xyzw"] for bone in poses]
            max_position = max(distance(a, b) for a, b in itertools.combinations(positions, 2))
            max_rotation = max(quaternion_angle_degrees(a, b) for a, b in itertools.combinations(rotations, 2))
            movement.append({
                "bone": name,
                "maximum_pairwise_position_change_component_cm": max_position,
                "maximum_pairwise_rotation_change_degrees": max_rotation,
                "position_bounds_component_cm": {axis: [min(pos[index] for pos in positions), max(pos[index] for pos in positions)]
                                                  for index, axis in enumerate(("x", "y", "z"))},
                "nonstatic_transform_at_0_05_deg_or_cm": max_position > 0.05 or max_rotation > 0.05,
            })
        seam_pairs = [pose_difference(rows[12], rows[13]), pose_difference(rows[14], rows[15])]
        repeat_pairs = [pose_difference(rows[12], rows[14]), pose_difference(rows[13], rows[15])]
        quaternion_norm_errors = [abs(math.sqrt(sum(value * value for value in bone["quaternion_xyzw"])) - 1.0)
                                  for row in rows for bone in row["component_space_bones"]]
        floor_minima = [{"phase": row["phase_index"], "time_s": row["sample_time_s"], "minimum_world_z_cm": row["surface_min_world_cm"]["z"]}
                        for row in primary]
        screenshot_files = [Path(row["file"]) for row in rows]
        clip_report = {
            "clip": clip,
            "asset": rows[0]["asset"],
            "clip_length_s": rows[0]["clip_length_s"],
            "bone_count": len(names),
            "bone_names": names,
            "all_recorded_position_quaternion_values_finite": True,
            "maximum_quaternion_norm_error": max(quaternion_norm_errors),
            "maximum_actual_vs_requested_sample_time_error_s": max(abs(row["sample_time_s"] - row["actual_single_node_time_s"]) for row in rows),
            "actor_maximum_pairwise_location_change_cm": max(distance(vector(a["actor_location_cm"]), vector(b["actor_location_cm"])) for a, b in itertools.combinations(rows, 2)),
            "camera_maximum_pairwise_location_change_cm": max(distance(vector(a["camera_world_location_cm"]), vector(b["camera_world_location_cm"])) for a, b in itertools.combinations(rows, 2)),
            "sampled_cpu_vertex_counts": sorted(set(row["actual_cpu_skinned_vertex_count"] for row in rows)),
            "sampled_bone_motion": movement,
            "maximum_pairwise_bone_rotation_change_degrees": max(bone["maximum_pairwise_rotation_change_degrees"] for bone in movement),
            "maximum_pairwise_bone_position_change_component_cm": max(bone["maximum_pairwise_position_change_component_cm"] for bone in movement),
            "near_end_start_pairs": seam_pairs,
            "repeated_end_and_start_samples": repeat_pairs,
            "near_end_start_maximum_rotation_difference_degrees": max(pair["maximum_bone_rotation_difference_degrees"] for pair in seam_pairs),
            "near_end_start_maximum_position_difference_component_cm": max(pair["maximum_bone_position_difference_component_cm"] for pair in seam_pairs),
            "near_end_start_within_selected_numeric_0_01deg_and_0_01cm_tolerance": all(
                pair["maximum_bone_rotation_difference_degrees"] <= 0.01 and pair["maximum_bone_position_difference_component_cm"] <= 0.01
                for pair in seam_pairs),
            "surface_minimum_world_z_range_cm": [min(row["minimum_world_z_cm"] for row in floor_minima), max(row["minimum_world_z_cm"] for row in floor_minima)],
            "surface_minimum_world_z_per_phase": floor_minima,
            "front_extent_from_actor_range_cm": [min(row["front_extent_from_actor_cm"] for row in primary), max(row["front_extent_from_actor_cm"] for row in primary)],
            "rear_extent_from_actor_range_cm": [min(row["rear_extent_from_actor_cm"] for row in primary), max(row["rear_extent_from_actor_cm"] for row in primary)],
            "recorded_morph_weight_ranges": {name: [min(row["final_component_morph_weights"][name] for row in primary), max(row["final_component_morph_weights"][name] for row in primary)]
                                           for name in rows[0]["final_component_morph_weights"]},
            "screenshots_saved_in_manifest_and_exist": all(row["saved"] and path.is_file() and path.stat().st_size > 0 for row, path in zip(rows, screenshot_files)),
            "screenshot_paths": [str(path) for path in screenshot_files],
        }
        report["clips"].append(clip_report)
    report["status"] = "held_phase_pose_data_verified_visual_and_runtime_limits_preserved"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"output": str(args.output), "status": report["status"], "clips": [{
        "clip": clip["clip"], "motion_deg": clip["maximum_pairwise_bone_rotation_change_degrees"],
        "motion_component_cm": clip["maximum_pairwise_bone_position_change_component_cm"],
        "seam_deg": clip["near_end_start_maximum_rotation_difference_degrees"],
        "seam_component_cm": clip["near_end_start_maximum_position_difference_component_cm"],
        "surface_z_cm": clip["surface_minimum_world_z_range_cm"],
    } for clip in report["clips"]]}, ensure_ascii=False))


if __name__ == "__main__":
    main()
