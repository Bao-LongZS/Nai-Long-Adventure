"""Animate the recommended E2 Naiwa model, save a new project, and render it.

Run in Blender background mode with --preview, then --render.
The recommended source model is read only; all outputs live beside this script.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
import numpy as np
from mathutils import Vector

OUT = Path(__file__).resolve().parent.parent
PROJECT = OUT.parent
SOURCE = PROJECT / "NaiwaCharacter/versions/20261007_H2_E2_overall_refinement/candidate_E2/Naiwa_Character.blend"
EXPECTED_SHA = "317f62278d63217636bf393f831e2fb41ce21acfefbe21beed6ad6d4dc75f322"
BLEND = OUT / "Naiwa_E2_Greeting_10s.blend"
FPS = 24
COUNT = 240


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * x * (x * (x * 6.0 - 15.0) + 10.0)


def envelope(t, start, rise, fall, end):
    return smooth((t - start) / (rise - start)) * (1 - smooth((t - fall) / (end - fall)))


def material(name, color, roughness=0.7):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    shader = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    shader.inputs["Base Color"].default_value = (*color, 1)
    shader.inputs["Roughness"].default_value = roughness
    return mat


def area(scene, name, location, power, size, color):
    data = bpy.data.lights.new(name, "AREA")
    data.energy = power
    data.shape = "DISK"
    data.size = size
    data.color = color
    obj = bpy.data.objects.new(name, data)
    scene.collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler = (Vector((0, 0, 1.4)) - obj.location).to_track_quat("-Z", "Y").to_euler()


def setup_scene():
    scene = bpy.context.scene
    for engine in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            scene.render.engine = engine
            break
        except (TypeError, ValueError):
            pass
    else:
        raise RuntimeError("Eevee is required for this animation")
    for obj in scene.objects:
        if obj.type == "LIGHT" or obj.name in {"Preview floor", "Head and torso silhouette only"}:
            obj.hide_render = True
        if obj.type == "MESH" and any(m.type == "ARMATURE" for m in obj.modifiers):
            obj.hide_render = False
            obj.hide_viewport = False
            obj.hide_set(False)
    for layer in scene.view_layers:
        layer.material_override = None
    scene.render.use_compositing = False
    scene.render.use_sequencer = False
    scene.render.film_transparent = False
    scene.render.use_border = False
    scene.render.use_crop_to_border = False
    scene.render.resolution_x = 1920
    scene.render.resolution_y = 1080
    scene.render.resolution_percentage = 100
    scene.render.pixel_aspect_x = scene.render.pixel_aspect_y = 1
    scene.render.fps = FPS
    scene.render.fps_base = 1
    scene.frame_start = 1
    scene.frame_end = COUNT
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    scene.render.image_settings.color_depth = "8"
    if hasattr(scene.eevee, "taa_render_samples"):
        scene.eevee.taa_render_samples = 32
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.view_settings.exposure = -0.85
    scene.view_settings.gamma = 1
    scene.view_settings.use_curve_mapping = False
    scene.world.use_nodes = True
    world = next(n for n in scene.world.node_tree.nodes if n.type == "BACKGROUND")
    world.inputs["Color"].default_value = (0.46, 0.58, 0.57, 1)
    world.inputs["Strength"].default_value = 0.45

    bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, -0.145))
    floor = bpy.context.object
    floor.name = "Greeting mint studio floor"
    floor.data.materials.append(material("Greeting mint", (0.48, 0.70, 0.65)))
    bpy.ops.mesh.primitive_cylinder_add(vertices=128, radius=1.48, depth=0.13, location=(0, 0, -0.08))
    stage = bpy.context.object
    stage.name = "Greeting rounded cream stage"
    stage.data.materials.append(material("Greeting warm cream", (0.89, 0.84, 0.69)))
    bevel = stage.modifiers.new("Soft stage edge", "BEVEL")
    bevel.width = 0.045
    bevel.segments = 4
    stage.modifiers.new("Stage normals", "WEIGHTED_NORMAL")
    for face in stage.data.polygons:
        face.use_smooth = True
    area(scene, "Greeting large key", (3, -4, 6), 480, 5, (1, 0.96, 0.87))
    area(scene, "Greeting soft fill", (-4, -2, 3.8), 270, 4, (0.88, 0.96, 1))
    area(scene, "Greeting rim", (1, 3, 5), 350, 4, (1, 0.98, 0.86))
    area(scene, "Greeting face fill", (0, -5, 2.5), 60, 3, (1, 1, 1))
    cam = scene.camera
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 6.3
    cam.data.shift_x = cam.data.shift_y = 0
    cam.data.clip_start = 0.01
    cam.data.clip_end = 500
    cam.data.dof.use_dof = False
    target = Vector((0, -0.02, 1.43))
    cam.location = target + Vector((0.58, -8, 0.8))
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = str(OUT / "frames" / "")
    return scene


def geometry_fingerprints(objects):
    result = {}
    for obj in objects:
        points = np.empty(len(obj.data.vertices) * 3, dtype=np.float32)
        obj.data.vertices.foreach_get("co", points)
        result[obj.name] = hashlib.sha256(points.tobytes()).hexdigest()
    return result


def body_min_z(body):
    evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
    coords = np.empty(len(evaluated.data.vertices) * 3, dtype=np.float32)
    evaluated.data.vertices.foreach_get("co", coords)
    coords = coords.reshape((-1, 3))
    row = np.array(evaluated.matrix_world[2][:3], dtype=np.float32)
    return float((coords @ row).min() + evaluated.matrix_world.translation.z)


def build_animation(scene, rig, body):
    rig.animation_data_create()
    rig.animation_data.action = None
    rig.animation_data.use_nla = False
    action = bpy.data.actions.new("Naiwa_E2_Greeting_10s")
    action.use_fake_user = True
    rig.animation_data.action = action
    rig.rotation_mode = "XYZ"
    rig.data.pose_position = "POSE"
    records = []
    for frame in range(1, COUNT + 1):
        scene.frame_set(frame)
        t = (frame - 1) / FPS
        wave = envelope(t, 0.65, 1.35, 3.45, 4.15)
        cheer = envelope(t, 6.65, 7.25, 8.15, 8.8)
        bow = envelope(t, 8.65, 9.05, 9.2, 9.65)
        active = envelope(t, 0, 0.7, 9.15, 9.95)
        hop = 0
        prepare = 0
        land = 0
        for start, duration, height in ((4.65, 0.75, 0.29), (5.85, 0.9, 0.42)):
            p = (t - start) / duration
            if 0 < p < 1:
                hop += height * 4 * p * (1 - p)
            prepare += envelope(t, start - 0.38, start - 0.13, start - 0.06, start)
            land += envelope(t, start + duration, start + duration + 0.1,
                             start + duration + 0.12, start + duration + 0.35)
        for bone in rig.pose.bones:
            bone.rotation_mode = "XYZ"
            bone.location = (0, 0, 0)
            bone.rotation_euler = (0, 0, 0)
            bone.scale = (1, 1, 1)
        torso = rig.pose.bones["Torso"]
        torso.rotation_euler = (
            active * 0.007 * math.sin(t * 2.1) + prepare * 0.020 + land * 0.015 + bow * 0.025,
            active * 0.007 * math.sin(t * 1.7),
            wave * 0.008 * math.sin(t * 2.4) + cheer * 0.01 * math.sin(t * 3.8),
        )
        for side in ("L", "R"):
            rig.pose.bones["Thigh." + side].rotation_euler.x = prepare * 0.08 + land * 0.045
            rig.pose.bones["Shin." + side].rotation_euler.x = -prepare * 0.1 - land * 0.055
        rig.location = (0.035 * active * math.sin(t * 1.8), 0, 0)
        rig.rotation_euler = (
            bow * 0.075 + prepare * 0.025 + land * 0.015,
            wave * 0.025 * math.sin(t * 3.2) + cheer * 0.022 * math.sin(t * 3.8),
            wave * 0.13 * math.sin(t * 2.3) + cheer * 0.16 * math.sin(t * 3.0),
        )
        # This asset has broad automatic shoulder weights. Small forearm accents
        # preserve its shape; the main dance is carried by the whole character.
        for side, sign in (("L", -1), ("R", 1)):
            fore = rig.pose.bones["Forearm." + side]
            fore.rotation_euler.x = wave * 0.09 * math.sin(t * 5.5 + sign) + cheer * 0.06
            fore.rotation_euler.z = sign * (wave * 0.075 + cheer * 0.10)
        # The jump is a world-space lift; foot compensation avoids ground penetration.
        bpy.context.view_layer.update()
        floor_correction = 0.0 - body_min_z(body)
        rig.location.z = floor_correction + hop
        bpy.context.view_layer.update()
        for bone in rig.pose.bones:
            for channel in ("location", "rotation_euler", "scale"):
                bone.keyframe_insert(data_path=channel, frame=frame, group=bone.name)
        rig.keyframe_insert(data_path="location", frame=frame, group="World motion")
        rig.keyframe_insert(data_path="rotation_euler", frame=frame, group="World motion")
        if frame % 12 == 1:
            records.append({"frame": frame, "time": t, "hop_height": hop,
                            "feet_world_z": body_min_z(body), "wave": wave, "cheer": cheer})
    for frame, name in ((1, "Settle"), (17, "Happy sway"), (100, "Ready to hop"),
                        (113, "Hop one"), (141, "Hop two"), (164, "Celebrate"),
                        (208, "Little bow"), (233, "Settle again")):
        scene.timeline_markers.new(name, frame=frame)
    scene.frame_set(1)
    bpy.context.view_layer.update()
    return records


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", action="store_true")
    ap.add_argument("--render", action="store_true")
    args = ap.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "records").mkdir(exist_ok=True)
    source_hash = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    if source_hash != EXPECTED_SHA:
        raise RuntimeError("The recommended E2 source differs from its delivery manifest")
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
    rig = bpy.data.objects["Naiwa_Rig"]
    body = bpy.data.objects["Naiwa unified golden body"]
    character = [obj for obj in bpy.data.objects if obj.type == "MESH" and
                 any(m.type == "ARMATURE" and m.object == rig for m in obj.modifiers)]
    original_geometry = geometry_fingerprints(character)
    scene = setup_scene()
    samples = build_animation(scene, rig, body)
    assert original_geometry == geometry_fingerprints(character), "Model geometry changed"
    assert source_hash == hashlib.sha256(SOURCE.read_bytes()).hexdigest(), "Source file changed"
    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    # Saved project has the new baked action selected and the original actions retained.
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
    report = {
        "source": str(SOURCE), "source_version": "E2", "source_sha256": source_hash,
        "source_unchanged": True, "geometry_unchanged": True,
        "blender_version": bpy.app.version_string, "saved_project": str(BLEND),
        "action": "Naiwa_E2_Greeting_10s", "fps": FPS, "frames": COUNT,
        "duration_seconds": COUNT / FPS, "resolution": [1920, 1080],
        "engine": scene.render.engine, "bound_meshes": len(character),
        "bones": len(rig.data.bones), "character_geometry_sha256": original_geometry,
        "motion": "Happy sway with small forearm accents, two hops, playful twist, small bow",
        "pose_samples": samples,
    }
    (OUT / "records/animation_build.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    if args.preview or args.render:
        destination = OUT / ("preview" if args.preview else "frames")
        destination.mkdir(exist_ok=True)
        frames = (1, 49, 70, 120, 151, 182, 220, 240) if args.preview else range(1, COUNT + 1)
        for frame in frames:
            scene.frame_set(frame)
            scene.render.filepath = str(destination / f"{frame:04d}.png")
            bpy.ops.render.render(write_still=True)
            if args.preview or frame % FPS == 0:
                print("NAIWA_ANIMATION_PROGRESS", frame, COUNT, flush=True)
    assert source_hash == hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    print("NAIWA_ANIMATION_PREVIEW_OK" if args.preview else "NAIWA_ANIMATION_RENDER_OK", flush=True)


if __name__ == "__main__":
    main()
