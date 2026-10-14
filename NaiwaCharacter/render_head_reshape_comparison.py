"""Render fixed, identical before/after head inspection views in native Blender.

Usage:
    blender --background --python render_head_reshape_comparison.py -- \
        --before baseline.blend --after revised.blend --output comparisons

The input projects are opened and changed in memory only; this script never
saves a .blend file or exports geometry.
"""

import argparse
from contextlib import contextmanager
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector

SCRIPT_DIRECTORY = Path(__file__).resolve().parent
if str(SCRIPT_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIRECTORY))
from render_surface_diagnostics import configure_soft_lighting


VIEWS = (("front", 0), ("side", 90), ("three_quarter", 45))
HEAD_TARGET = (0.0, -0.12, 2.37)
FULL_TARGET = (0.0, -0.12, 1.31)
HEAD_SCALE = 0.85
FULL_SCALE = 3.45
DISTANCE = 7.0
RESOLUTION = (900, 1000)
GRAY_COLOR = (0.38, 0.38, 0.38, 1.0)


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", required=True, type=Path)
    parser.add_argument("--after", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--head-only", action="store_true")
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    return parser.parse_args(arguments)


def set_render_engine(scene, requested=None):
    """Use one engine for both files, with native-version Eevee compatibility."""
    candidates = (requested,) if requested else (
        "BLENDER_EEVEE_NEXT", "BLENDER_EEVEE", "CYCLES"
    )
    for candidate in candidates:
        try:
            scene.render.engine = candidate
            return candidate
        except (TypeError, ValueError):
            continue
    raise RuntimeError("No supported common render engine: " + repr(candidates))


def prepare_scene(render_engine=None):
    scene = bpy.context.scene
    engine = set_render_engine(scene, render_engine)
    rig = bpy.data.objects.get("Naiwa_Rig")
    idle = bpy.data.actions.get("Naiwa_Idle")
    if rig is None or idle is None:
        raise RuntimeError("Project must contain Naiwa_Rig and Naiwa_Idle")
    animation = rig.animation_data_create()
    animation.action = idle
    animation.use_nla = False
    rig.data.pose_position = "POSE"

    for obj in scene.objects:
        if obj.type == "MESH" and any(
            modifier.type == "ARMATURE" for modifier in obj.modifiers
        ):
            obj.hide_render = False
            obj.hide_viewport = False
            obj.hide_set(False)
    shell = bpy.data.objects.get("Head and torso silhouette only")
    if shell is not None:
        shell.hide_render = True
        shell.hide_set(True)
    floor = bpy.data.objects.get("Preview floor")
    if floor is not None:
        floor.hide_render = False

    scene.frame_set(1)
    scene.render.resolution_x, scene.render.resolution_y = RESOLUTION
    scene.render.resolution_percentage = 100
    scene.render.pixel_aspect_x = scene.render.pixel_aspect_y = 1.0
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    scene.render.image_settings.color_depth = "8"
    scene.render.film_transparent = False
    scene.render.use_border = False
    scene.render.use_crop_to_border = False
    scene.render.use_compositing = False
    scene.render.use_sequencer = False
    if hasattr(scene.render, "use_motion_blur"):
        scene.render.use_motion_blur = False
    if hasattr(scene, "eevee") and hasattr(scene.eevee, "taa_render_samples"):
        scene.eevee.taa_render_samples = 64
    if engine == "CYCLES":
        scene.cycles.samples = 64
        scene.cycles.seed = 0
        scene.cycles.use_animated_seed = False
    for layer in scene.view_layers:
        layer.material_override = None

    if scene.world is None:
        scene.world = bpy.data.worlds.new("Head comparison world")
    scene.world.use_nodes = True
    configure_soft_lighting(scene)
    for name in ("Surface soft key", "Surface soft fill"):
        light = bpy.data.objects[name]
        light.hide_viewport = False
        light.hide_set(False)
        light.data.shape = "SQUARE"
    scene.view_settings.gamma = 1.0
    scene.view_settings.use_curve_mapping = False
    # Standard's neutral look is named differently across Blender versions.
    for look in ("None", "Medium High Contrast"):
        try:
            scene.view_settings.look = look
            break
        except (TypeError, ValueError):
            continue

    if scene.camera is None:
        data = bpy.data.cameras.new("Head comparison camera")
        scene.camera = bpy.data.objects.new("Head comparison camera", data)
        scene.collection.objects.link(scene.camera)
    camera = scene.camera
    camera.hide_render = False
    camera.data.type = "ORTHO"
    camera.data.lens = 50.0
    camera.data.shift_x = camera.data.shift_y = 0.0
    camera.data.clip_start = 0.01
    camera.data.clip_end = 1000.0
    camera.data.dof.use_dof = False
    bpy.context.view_layer.update()
    return scene, engine


def render_view(scene, output, filename, target, scale, degrees):
    angle = math.radians(degrees)
    camera = scene.camera
    camera.data.ortho_scale = scale
    camera.location = (
        DISTANCE * math.sin(angle),
        target[1] - DISTANCE * math.cos(angle),
        target[2],
    )
    camera.rotation_euler = (
        Vector(target) - camera.location
    ).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = str(output / filename)
    bpy.context.view_layer.update()
    bpy.ops.render.render(write_still=True, scene=scene.name)
    return filename


def make_gray_material():
    gray = bpy.data.materials.new("Head reshape neutral gray override")
    gray.use_nodes = True
    nodes = gray.node_tree.nodes
    nodes.clear()
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    shader.inputs["Base Color"].default_value = GRAY_COLOR
    shader.inputs["Roughness"].default_value = 1.0
    specular = shader.inputs.get("Specular IOR Level") or shader.inputs.get("Specular")
    if specular is not None:
        specular.default_value = 0.0
    output = nodes.new("ShaderNodeOutputMaterial")
    gray.node_tree.links.new(shader.outputs["BSDF"], output.inputs["Surface"])
    return gray


@contextmanager
def gray_override(scene):
    """Enforce plain gray even in engines that ignore view-layer overrides.

    The gray material has no attribute/color nodes. Temporary material-slot
    replacement ensures original vertex-color shaders cannot remain active.
    Every original slot and override is restored after the gray renders.
    """
    gray = make_gray_material()
    layers = [(layer, layer.material_override) for layer in scene.view_layers]
    objects = [obj for obj in scene.objects
               if obj.type in {"MESH", "CURVE", "SURFACE", "FONT", "META"}
               and not obj.hide_render]
    data_materials = {}
    object_slots = []
    # Snapshot every slot before changing shared material datablocks.
    for obj in objects:
        data_materials.setdefault(obj.data, tuple(obj.data.materials))
        object_slots.append((obj, tuple(
            (slot.link, slot.material) for slot in obj.material_slots
        )))
    try:
        for layer, _ in layers:
            layer.material_override = gray
        for data, original in data_materials.items():
            if original:
                for index in range(len(original)):
                    data.materials[index] = gray
            else:
                data.materials.append(gray)
        for obj, _ in object_slots:
            for slot in obj.material_slots:
                slot.material = gray
        bpy.context.view_layer.update()
        yield
    finally:
        for data, original in data_materials.items():
            if original:
                for index, material in enumerate(original):
                    data.materials[index] = material
            else:
                data.materials.clear()
        for obj, original in object_slots:
            for slot, (link, material) in zip(obj.material_slots, original):
                slot.link = link
                slot.material = material
        for layer, original in layers:
            layer.material_override = original
        bpy.context.view_layer.update()


def render_project(source, output, prefix, render_engine=None, head_only=False):
    bpy.ops.wm.open_mainfile(filepath=str(source), load_ui=False)
    scene, engine = prepare_scene(render_engine)
    filenames = []
    for view, degrees in VIEWS:
        filenames.append(render_view(
            scene, output, f"{prefix}_head_{view}.png",
            HEAD_TARGET, HEAD_SCALE, degrees
        ))
    if not head_only:
        filenames.append(render_view(
            scene, output, f"{prefix}_full_front.png", FULL_TARGET, FULL_SCALE, 0
        ))
    with gray_override(scene):
        for view, degrees in VIEWS:
            filenames.append(render_view(
                scene, output, f"{prefix}_head_gray_{view}.png",
                HEAD_TARGET, HEAD_SCALE, degrees
            ))
    return engine, filenames


def main():
    args = parse_arguments()
    before, after, output = (path.expanduser().resolve() for path in (
        args.before, args.after, args.output
    ))
    for source in (before, after):
        if not source.is_file() or source.suffix.lower() != ".blend":
            raise FileNotFoundError("Expected an existing .blend project: " + str(source))
    output.mkdir(parents=True, exist_ok=True)
    engine, before_files = render_project(before, output, "before", head_only=args.head_only)
    _, after_files = render_project(after, output, "after", engine, head_only=args.head_only)
    filenames = before_files + after_files
    for filename in filenames:
        path = output / filename
        if not path.is_file() or path.stat().st_size == 0:
            raise RuntimeError("Missing render: " + str(path))
    protocol = {
        "before": str(before), "after": str(after),
        "projection": "ORTHO", "lens_mm": 50, "distance": DISTANCE,
        "head_scale": HEAD_SCALE, "full_scale": FULL_SCALE,
        "head_target": HEAD_TARGET, "full_target": FULL_TARGET,
        "views_degrees": dict(VIEWS), "resolution": RESOLUTION,
        "animation": "Naiwa_Idle", "frame": 1,
        "render_engine": engine, "view_transform": "Standard",
        "exposure": -0.4, "gamma": 1.0,
        "lighting_function": "render_surface_diagnostics.configure_soft_lighting",
        "key": 550, "fill": 250, "area_size": 4.5,
        "key_location": [3, -4, 5], "fill_location": [-3, -1, 3.5],
        "light_target": [0, 0, 2.2], "area_shape": "SQUARE",
        "world_color": [0.18, 0.18, 0.18, 1], "world_strength": 0.5,
        "gray_rgba": GRAY_COLOR,
        "gray_method": "view-layer material override and temporary material slots",
        "before_after_identical_camera_framing_lighting_exposure": True,
        "source_projects_saved": False, "renders": filenames,
    }
    (output / "camera_protocol.json").write_text(
        json.dumps(protocol, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print("HEAD_RESHAPE_COMPARISON_OK", len(filenames), "renders", str(output), flush=True)


if __name__ == "__main__":
    main()
