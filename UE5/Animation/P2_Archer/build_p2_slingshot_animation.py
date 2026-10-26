"""Create the P2 interaction-driven Naiwa slingshot draw clip from pinned E2."""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import bpy
from mathutils import Quaternion, Vector

WORKSPACE = Path(__file__).resolve().parents[3]
SOURCE = WORKSPACE / "NaiwaCharacter/versions/20261007_H2_E2_overall_refinement/candidate_E2/Naiwa_Character.blend"
SOURCE_SHA = "317f62278d63217636bf393f831e2fb41ce21acfefbe21beed6ad6d4dc75f322"
OUT = Path(__file__).resolve().parent
ACTION_NAME = "Naiwa_P2_SlingshotDraw"
FPS = 24
FRAME_START = 1
FRAME_END = 31


def smooth(value: float) -> float:
    value = max(0.0, min(1.0, value))
    return value * value * value * (value * (value * 6.0 - 15.0) + 10.0)


def world_delta(bone, rotation: Quaternion) -> None:
    rest = bone.bone.matrix_local.to_quaternion()
    bone.rotation_euler = (rest.inverted() @ rotation @ rest).to_euler("XYZ")


def rotate_x(bone, degrees: float, abduct: float = 0.0) -> None:
    rotation = Quaternion(Vector((1, 0, 0)), math.radians(degrees))
    if abduct:
        rotation = Quaternion(Vector((0, 1, 0)), math.radians(abduct)) @ rotation
    world_delta(bone, rotation)


def rotate_y(bone, degrees: float) -> None:
    world_delta(bone, Quaternion(Vector((0, 1, 0)), math.radians(degrees)))


def build(rig):
    action = bpy.data.actions.new(ACTION_NAME)
    action.use_fake_user = True
    rig.animation_data_create()
    rig.animation_data.action = action
    rig.animation_data.use_nla = False
    rig.data.pose_position = "POSE"
    rig.rotation_mode = "XYZ"
    samples = []

    for frame in range(FRAME_START, FRAME_END + 1):
        t = (frame - FRAME_START) / (FRAME_END - FRAME_START)
        draw = smooth(t)
        prep = smooth(min(1.0, t / 0.18))
        settle = math.sin(t * math.pi)

        for bone in rig.pose.bones:
            bone.rotation_mode = "XYZ"
            bone.location = (0.0, 0.0, 0.0)
            bone.rotation_euler = (0.0, 0.0, 0.0)
            bone.scale = (1.0, 1.0, 1.0)

        # A small counter-lean keeps the round torso balanced as the arms reach.
        rotate_y(rig.pose.bones["Torso"], -5.0 * draw)
        torso = rig.pose.bones["Torso"]
        torso.rotation_euler.x += math.radians(1.2 * settle)

        # Left arm braces forward on the launcher; right arm folds back toward
        # the chest to pull the pouch. Small shoulder arcs respect the broad
        # weights on the original low-poly E2 mesh.
        rotate_x(rig.pose.bones["UpperArm.L"], 38.0 * draw, 3.0 * draw)
        rotate_x(rig.pose.bones["Forearm.L"], -70.0 * draw)
        rotate_x(rig.pose.bones["UpperArm.R"], -30.0 * draw, -4.0 * draw)
        rotate_x(rig.pose.bones["Forearm.R"], -82.0 * draw)

        # The pull hand settles into tension with a light breathing pulse.
        forearm = rig.pose.bones["Forearm.R"]
        forearm.rotation_euler.x += math.radians(1.1 * draw * math.sin(t * math.pi * 4.0))
        for bone in rig.pose.bones:
            for channel in ("location", "rotation_euler", "scale"):
                bone.keyframe_insert(data_path=channel, frame=frame, group=bone.name)
        samples.append({
            "frame": frame,
            "progress": round(t, 4),
            "draw_ease": round(draw, 5),
            "torso_counterlean_deg": round(-5.0 * draw, 3),
            "lead_upper_arm_deg": round(38.0 * draw, 3),
            "pull_upper_arm_deg": round(-30.0 * draw, 3),
        })

    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for key in curve.keyframe_points:
                        key.interpolation = "LINEAR"
    return action, samples


def export(scene, rig):
    rig.hide_set(False)
    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    out_fbx = OUT / "Naiwa_P2_SlingshotDraw.fbx"
    bpy.ops.export_scene.fbx(
        filepath=str(out_fbx), use_selection=True, object_types={"ARMATURE"},
        add_leaf_bones=False, apply_unit_scale=True, bake_anim=True,
        bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
        bake_anim_step=0.25, bake_anim_simplify_factor=0.0, path_mode="AUTO")
    return out_fbx


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    if hashlib.sha256(SOURCE.read_bytes()).hexdigest() != SOURCE_SHA:
        raise RuntimeError("Pinned E2 animation source hash changed")
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE), load_ui=False)
    rig = bpy.data.objects["Naiwa_Rig"]
    scene = bpy.context.scene
    scene.render.fps, scene.render.fps_base = FPS, 1.0
    scene.frame_start, scene.frame_end = FRAME_START, FRAME_END
    action, samples = build(rig)
    scene.frame_set(FRAME_START)
    blend = OUT / "Naiwa_P2_SlingshotDraw.blend"
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    fbx = export(scene, rig)
    report = {
        "source_blend": str(SOURCE), "source_sha256": SOURCE_SHA,
        "source_unchanged": hashlib.sha256(SOURCE.read_bytes()).hexdigest() == SOURCE_SHA,
        "blender_version": bpy.app.version_string,
        "action": ACTION_NAME, "fps": FPS, "frame_start": FRAME_START,
        "frame_end": FRAME_END, "duration_seconds": (FRAME_END - FRAME_START) / FPS,
        "sample_count": len(samples), "blend": str(blend), "fbx_animation_only": str(fbx),
        "design": "Smooth shoulder reach and pouch pull, slight counter-lean, subtle held-tension breath; keyed over a one-second normalized draw range for cursor-driven pose sampling.",
        "bones": ["Torso", "UpperArm.L", "Forearm.L", "UpperArm.R", "Forearm.R"],
        "maximum_joint_offsets_deg": {"lead_upper_arm": 38, "pull_upper_arm": -30,
                                       "forearm_L": -70, "forearm_R": -82,
                                       "torso_counterlean": -5},
        "samples": samples,
    }
    (OUT / "build_report.json").write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print("NAIWA_P2_ARCHER_FBX_EXPORTED", flush=True)


if __name__ == "__main__":
    main()
