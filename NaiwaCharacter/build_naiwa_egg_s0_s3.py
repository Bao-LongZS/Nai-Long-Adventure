"""Build and validate the frozen S0-S3 NaiwaEgg asset in Blender background mode."""
import json
import argparse
import math
import pathlib
import sys
import traceback

import bpy
from mathutils import Vector


ROOT = pathlib.Path(__file__).resolve().parents[1]
BLENDER_ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ARG_PARSER = argparse.ArgumentParser()
ARG_PARSER.add_argument("--params", type=pathlib.Path)
ARG_PARSER.add_argument("--output-dir", type=pathlib.Path)
ARGS, _ = ARG_PARSER.parse_known_args(BLENDER_ARGS)
PARAMS_FILE = ARGS.params or ROOT / "NaiwaCharacter/parameters/NaiwaEgg_Parameters_v1.json"
OUTPUT_DIR = ARGS.output_dir or ROOT / "NaiwaCharacter/deliveries/NaiwaEgg_S0-S3"
BLEND_FILE = OUTPUT_DIR / "NaiwaEgg_S0-S3.blend"
REPORT_FILE = OUTPUT_DIR / "NaiwaEgg_S0-S3_validation.json"
SEGMENTS = 28
BODY_RING_STEPS = 20


def linear_channel(channel):
    value = channel / 255.0
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def color_from_hex(value):
    value = value.lstrip("#")
    return tuple(linear_channel(int(value[index:index + 2], 16)) for index in (0, 2, 4)) + (1.0,)


def pchip_slopes(xs, ys):
    count = len(xs)
    hs = [xs[i + 1] - xs[i] for i in range(count - 1)]
    deltas = [(ys[i + 1] - ys[i]) / hs[i] for i in range(count - 1)]
    slopes = [0.0] * count
    if count == 2:
        return [deltas[0], deltas[0]]

    def endpoint(h0, h1, d0, d1):
        result = ((2.0 * h0 + h1) * d0 - h0 * d1) / (h0 + h1)
        if result * d0 <= 0.0:
            return 0.0
        if d0 * d1 < 0.0 and abs(result) > abs(3.0 * d0):
            return 3.0 * d0
        return result

    slopes[0] = endpoint(hs[0], hs[1], deltas[0], deltas[1])
    slopes[-1] = endpoint(hs[-1], hs[-2], deltas[-1], deltas[-2])
    for index in range(1, count - 1):
        left = deltas[index - 1]
        right = deltas[index]
        if left == 0.0 or right == 0.0 or left * right < 0.0:
            slopes[index] = 0.0
        else:
            w1 = 2.0 * hs[index] + hs[index - 1]
            w2 = hs[index] + 2.0 * hs[index - 1]
            slopes[index] = (w1 + w2) / (w1 / left + w2 / right)
    return slopes


def pchip_value(xs, ys, slopes, x):
    if x <= xs[0]:
        return ys[0]
    if x >= xs[-1]:
        return ys[-1]
    index = next(i for i in range(len(xs) - 1) if xs[i] <= x <= xs[i + 1])
    h = xs[index + 1] - xs[index]
    t = (x - xs[index]) / h
    h00 = 2.0 * t ** 3 - 3.0 * t ** 2 + 1.0
    h10 = t ** 3 - 2.0 * t ** 2 + t
    h01 = -2.0 * t ** 3 + 3.0 * t ** 2
    h11 = t ** 3 - t ** 2
    return (h00 * ys[index] + h10 * h * slopes[index] +
            h01 * ys[index + 1] + h11 * h * slopes[index + 1])


def make_material(name, color_hex, roughness, specular_ior_level=None):
    material = bpy.data.materials.new(name)
    material.diffuse_color = color_from_hex(color_hex)
    material.use_nodes = True
    shader = next((node for node in material.node_tree.nodes
                   if node.type == "BSDF_PRINCIPLED"), None)
    if shader is None:
        shader = material.node_tree.nodes.new("ShaderNodeBsdfPrincipled")
    shader.inputs["Base Color"].default_value = color_from_hex(color_hex)
    shader.inputs["Roughness"].default_value = roughness
    shader.inputs["Metallic"].default_value = 0.0
    if specular_ior_level is not None and "Specular IOR Level" in shader.inputs:
        shader.inputs["Specular IOR Level"].default_value = specular_ior_level
    return material


def append_component(all_vertices, all_faces, face_materials, smooth_flags,
                     component_vertices, component_faces, material_index,
                     smooth=True):
    start = len(all_vertices)
    all_vertices.extend(component_vertices)
    all_faces.extend(tuple(start + index for index in face) for face in component_faces)
    face_materials.extend([material_index] * len(component_faces))
    smooth_flags.extend([smooth] * len(component_faces))
    return list(range(start, start + len(component_vertices)))


def body_component(params):
    height = params["dimensions"]["height_m"]
    width_radius = params["dimensions"]["width_m"] / 2.0
    t_values = params["profile"]["t"]
    r_values = params["profile"]["radius_x_m"]
    slopes = pchip_slopes(t_values, r_values)
    cross_section = params["profile"]["cross_section_y_over_x"]

    ring_ts = params["profile"].get("ring_t")
    if ring_ts is None:
        ring_ts = [index / BODY_RING_STEPS for index in range(1, BODY_RING_STEPS + 1)]
        # Put the documented widest point exactly on a ring so it can be measured.
        ring_ts[6] = 0.34
    if len(ring_ts) != BODY_RING_STEPS:
        raise ValueError(f"profile.ring_t must have {BODY_RING_STEPS} values")
    ring_vertices = []
    for t in ring_ts:
        radius = pchip_value(t_values, r_values, slopes, t)
        radius_y = radius * cross_section
        z = (t - 0.5) * height
        for segment in range(SEGMENTS):
            angle = 2.0 * math.pi * segment / SEGMENTS
            ring_vertices.append((radius * math.cos(angle), radius_y * math.sin(angle), z))

    vertices = [(0.0, 0.0, -height / 2.0)] + ring_vertices
    faces = []
    smooth = []
    first_ring = 1
    for segment in range(SEGMENTS):
        next_segment = (segment + 1) % SEGMENTS
        faces.append((0, first_ring + next_segment, first_ring + segment))
        smooth.append(True)
    for ring in range(BODY_RING_STEPS - 1):
        lower = 1 + ring * SEGMENTS
        upper = lower + SEGMENTS
        for segment in range(SEGMENTS):
            next_segment = (segment + 1) % SEGMENTS
            faces.append((lower + segment, lower + next_segment,
                          upper + next_segment, upper + segment))
            smooth.append(True)
    top_ring = 1 + (BODY_RING_STEPS - 1) * SEGMENTS
    faces.append(tuple(top_ring + segment for segment in range(SEGMENTS)))
    smooth.append(False)
    return vertices, faces, smooth, ring_ts


def sphere_component(center, radius, segments, latitude_steps):
    vertices = []
    for latitude in range(latitude_steps + 1):
        theta = math.pi * latitude / latitude_steps
        radial = math.sin(theta) * radius
        z = math.cos(theta) * radius
        for segment in range(segments):
            phi = 2.0 * math.pi * segment / segments
            vertices.append((center[0] + radial * math.cos(phi),
                             center[1] + radial * math.sin(phi),
                             center[2] + z))
    faces = []
    for latitude in range(latitude_steps):
        upper = latitude * segments
        lower = upper + segments
        for segment in range(segments):
            next_segment = (segment + 1) % segments
            faces.append((upper + segment, lower + segment,
                          lower + next_segment, upper + next_segment))
    return vertices, faces


def hemisphere_component(center, radius, segments, latitude_steps):
    """Build an eye cap facing -Y; its equator is buried inside the egg body."""
    vertices = [(center[0], center[1] - radius, center[2])]
    for ring in range(1, latitude_steps + 1):
        theta = (math.pi * 0.5) * ring / latitude_steps
        radial = math.sin(theta) * radius
        y = center[1] - math.cos(theta) * radius
        for segment in range(segments):
            phi = 2.0 * math.pi * segment / segments
            vertices.append((center[0] + radial * math.cos(phi),
                             y,
                             center[2] + radial * math.sin(phi)))

    faces = []
    first_ring = 1
    for segment in range(segments):
        next_segment = (segment + 1) % segments
        faces.append((0, first_ring + segment, first_ring + next_segment))
    for ring in range(latitude_steps - 1):
        inner = 1 + ring * segments
        outer = inner + segments
        for segment in range(segments):
            next_segment = (segment + 1) % segments
            faces.append((inner + segment, outer + segment,
                          outer + next_segment, inner + next_segment))
    return vertices, faces


def body_surface_y(params, x, z):
    """Return the front (-Y) surface location for a point in the XZ plane."""
    height = params["dimensions"]["height_m"]
    t_values = params["profile"]["t"]
    r_values = params["profile"]["radius_x_m"]
    slopes = pchip_slopes(t_values, r_values)
    t = (z + height * 0.5) / height
    radius_x = pchip_value(t_values, r_values, slopes, t)
    if radius_x <= 1.0e-9:
        return 0.0
    radius_y = radius_x * params["profile"]["cross_section_y_over_x"]
    radial = math.sqrt(max(0.0, 1.0 - (x / radius_x) ** 2))
    return -radius_y * radial


def body_surface_normal(params, x, z):
    epsilon = 1.0e-4
    dx = (body_surface_y(params, x + epsilon, z) -
          body_surface_y(params, x - epsilon, z)) / (2.0 * epsilon)
    dz = (body_surface_y(params, x, z + epsilon) -
          body_surface_y(params, x, z - epsilon)) / (2.0 * epsilon)
    return Vector((dx, -1.0, dz)).normalized()


def surface_point(params, x, z, normal_offset):
    surface = Vector((x, body_surface_y(params, x, z), z))
    return tuple(surface + body_surface_normal(params, x, z) * normal_offset)


def surface_cap_component(params, center_x, center_z, radius, segments,
                          rim_offset, center_bump):
    """Build one circular eye layer projected onto the egg's curved front surface."""
    vertices = [surface_point(params, center_x, center_z, rim_offset + center_bump)]
    for segment in range(segments):
        angle = 2.0 * math.pi * segment / segments
        x = center_x + radius * math.cos(angle)
        z = center_z + radius * math.sin(angle)
        vertices.append(surface_point(params, x, z, rim_offset))
    faces = [(0, 1 + segment, 1 + ((segment + 1) % segments))
             for segment in range(segments)]
    return vertices, faces


def mouth_component(params):
    mouth = params["mouth"]
    width, height = mouth["closed_width_height_m"]
    center_x, _, center_z = mouth["center_m"]
    horizontal_steps = mouth.get("mesh_steps_x", 24)
    vertical_rows = mouth.get("mesh_steps_z", 8)
    smile_rise = mouth.get("smile_rise_m", 0.0)
    vertices = []
    uv = []
    for row in range(vertical_rows + 1):
        v = row / vertical_rows * 2.0 - 1.0
        for column in range(horizontal_steps + 1):
            u = column / horizontal_steps * 2.0 - 1.0
            x = center_x + u * width * 0.5
            z = center_z + smile_rise * abs(u) ** 2 + v * height * 0.5
            vertices.append(surface_point(params, x, z, mouth.get("surface_offset_m", 0.003)))
            uv.append((u, v))
    faces = []
    stride = horizontal_steps + 1
    for row in range(vertical_rows):
        for column in range(horizontal_steps):
            a = row * stride + column
            faces.append((a, a + 1, a + stride + 1, a + stride))
    return vertices, faces, uv


def signed_mesh_volume(vertices, faces):
    volume = 0.0
    for face in faces:
        v0 = Vector(vertices[face[0]])
        for index in range(1, len(face) - 1):
            v1 = Vector(vertices[face[index]])
            v2 = Vector(vertices[face[index + 1]])
            volume += v0.dot(v1.cross(v2)) / 6.0
    return volume


def point_bounds(points):
    return tuple((min(point[axis] for point in points), max(point[axis] for point in points))
                 for axis in range(3))


def configure_render_scene():
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 768
    scene.render.resolution_y = 768
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False
    scene.render.image_settings.color_mode = "RGBA"
    # Use the authored swatches directly in review renders; AgX desaturates this small warm palette.
    scene.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("NaiwaEgg_PreviewWorld") if not scene.world else scene.world
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.025, 0.04, 0.05, 1.0)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.55

    for name, location, power, size in (
        ("Key", (-3.5, -4.0, 4.0), 225.0, 4.0),
        ("Fill", (3.0, -2.0, 1.5), 117.0, 3.5),
        ("LowerFill", (0.0, -2.8, -2.4), 99.0, 3.5),
        ("Rim", (1.0, 2.5, 2.5), 180.0, 3.0),
    ):
        light_data = bpy.data.lights.new(name, "AREA")
        light_data.energy = power
        light_data.shape = "DISK"
        light_data.size = size
        light = bpy.data.objects.new("PreviewLight_" + name, light_data)
        scene.collection.objects.link(light)
        light.location = location
        direction = Vector((0.0, 0.0, 0.0)) - light.location
        light.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def render_view(name, location, target, ortho_scale, filename, resolution=(768, 768)):
    scene = bpy.context.scene
    scene.render.resolution_x, scene.render.resolution_y = resolution
    camera_data = bpy.data.cameras.new("PreviewCamera_" + name)
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = ortho_scale
    camera = bpy.data.objects.new("PreviewCamera_" + name, camera_data)
    scene.collection.objects.link(camera)
    camera.location = location
    direction = Vector(target) - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    scene.camera = camera
    scene.render.filepath = str(OUTPUT_DIR / filename)
    bpy.context.view_layer.update()
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(camera, do_unlink=True)


def main():
    global SEGMENTS, BODY_RING_STEPS
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    params = json.loads(PARAMS_FILE.read_text(encoding="utf-8"))
    SEGMENTS = params.get("mesh_budget", {}).get("radial_segments", SEGMENTS)
    BODY_RING_STEPS = len(params["profile"].get("ring_t", [])) or \
        params.get("mesh_budget", {}).get("body_rings", BODY_RING_STEPS)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for mesh in list(bpy.data.meshes):
        if mesh.users == 0:
            bpy.data.meshes.remove(mesh)
    for material in list(bpy.data.materials):
        if material.users == 0:
            bpy.data.materials.remove(material)

    material_specs = params.get("material_slots", {})
    materials = [
        make_material("M_NaiwaEgg_Body_Base",
            material_specs.get("Body", {}).get("color", params["variants"]["Base"]),
            material_specs.get("Body", {}).get("roughness", 0.62),
            material_specs.get("Body", {}).get("specular_ior_level")),
        make_material("M_NaiwaEgg_EyeWhite",
            material_specs.get("EyeWhite", {}).get("color", "#FBF7EA"),
            material_specs.get("EyeWhite", {}).get("roughness", 0.35),
            material_specs.get("EyeWhite", {}).get("specular_ior_level")),
        make_material("M_NaiwaEgg_Pupil",
            material_specs.get("Pupil", {}).get("color", "#14261A"),
            material_specs.get("Pupil", {}).get("roughness", 0.30),
            material_specs.get("Pupil", {}).get("specular_ior_level", 0.12)),
        make_material("M_NaiwaEgg_Mouth",
            material_specs.get("Mouth", {}).get("color", "#3A1410"),
            material_specs.get("Mouth", {}).get("roughness", 0.70),
            material_specs.get("Mouth", {}).get("specular_ior_level")),
    ]

    vertices, faces, smooth_flags, ring_ts = [], [], [], []
    component_groups = {}
    component_centers = {}

    body_vertices, body_faces, body_smooth, ring_ts = body_component(params)
    body_start = len(vertices)
    vertices.extend(body_vertices)
    faces.extend(body_faces)
    smooth_flags.extend(body_smooth)
    component_groups["body"] = list(range(body_start, len(vertices)))
    body_face_count = len(body_faces)
    face_materials = [0] * body_face_count

    eye_spec = params["eye"]
    eye_layers = eye_spec.get("surface_layers", [
        {"rim_offset_m": 0.003, "center_bump_m": 0.010},
        {"rim_offset_m": 0.009, "center_bump_m": 0.006},
        {"rim_offset_m": 0.013, "center_bump_m": 0.004},
    ])
    for eye_index, center in enumerate(eye_spec["centers_m"]):
        eye_label = "left" if eye_index == 0 else "right"
        center_x, _, center_z = center
        surface_anchor = (center_x, body_surface_y(params, center_x, center_z), center_z)
        layer_specs = [
            ("eye_white_" + eye_label, eye_spec["radius_m"], 1, eye_layers[0],
             center_x, center_z),
            ("pupil_" + eye_label, eye_spec["pupil_radius_m"], 2, eye_layers[1],
             center_x, center_z),
        ]
        glint_offset = eye_spec.get("glint_offset_m", [
            -eye_spec["pupil_radius_m"] * 0.45, 0.0, eye_spec["pupil_radius_m"] * 0.45])
        glint_x = center_x + glint_offset[0]
        glint_z = center_z + glint_offset[2]
        layer_specs.append(("glint_" + eye_label, eye_spec["glint_radius_m"], 1,
            eye_layers[2], glint_x, glint_z))

        for group, radius, material_index, layer, layer_x, layer_z in layer_specs:
            eye_vertices, eye_faces = surface_cap_component(params, layer_x, layer_z,
                radius, SEGMENTS, layer["rim_offset_m"], layer["center_bump_m"])
            component_centers[group] = surface_anchor
            component_groups[group] = append_component(vertices, faces, face_materials,
                smooth_flags, eye_vertices, eye_faces, material_index, True)

    mouth_vertices, mouth_faces, mouth_uv = mouth_component(params)
    mouth_start = len(vertices)
    vertices.extend(mouth_vertices)
    faces.extend(tuple(mouth_start + index for index in face) for face in mouth_faces)
    smooth_flags.extend([False] * len(mouth_faces))
    face_materials.extend([3] * len(mouth_faces))
    component_groups["mouth"] = list(range(mouth_start, len(vertices)))
    mouth_face_start = len(faces) - len(mouth_faces)

    mesh = bpy.data.meshes.new("SK_NaiwaEgg_S0S3_Mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.update(calc_edges=True)
    obj = bpy.data.objects.new("SK_NaiwaEgg", mesh)
    bpy.context.scene.collection.objects.link(obj)
    for component_name, indices in component_groups.items():
        group_name = "BODY" if component_name == "body" else "FACE_" + component_name.upper()
        group = obj.vertex_groups.new(name=group_name)
        group.add(indices, 1.0, "REPLACE")
    for material in materials:
        mesh.materials.append(material)
    for polygon, material_index, smooth in zip(mesh.polygons, face_materials, smooth_flags):
        polygon.material_index = material_index
        polygon.use_smooth = smooth
    obj.location = (0.0, 0.0, 0.0)
    obj.rotation_euler = (0.0, 0.0, 0.0)

    basis = obj.shape_key_add(name="Basis", from_mix=False)
    eye_wide = obj.shape_key_add(name="EyeWide", from_mix=False)
    scream = obj.shape_key_add(name="Scream", from_mix=False)
    squash = obj.shape_key_add(name="Squash", from_mix=False)
    base_coords = [tuple(point.co) for point in basis.data]
    for index, point in enumerate(base_coords):
        eye_wide.data[index].co = point
        scream.data[index].co = point
        squash.data[index].co = point

    eye_morph_spec = params.get("morph_targets", {}).get("EyeWide", {})
    eye_wide_scale = eye_spec.get("eye_wide_scale",
        eye_morph_spec.get("eye_wide_scale", [1.15, 1.0, 1.15]))
    pupil_relative_scale = eye_spec.get("pupil_relative_scale",
        eye_morph_spec.get("pupil_relative_scale", 0.85))
    for component_name in ("eye_white_left", "eye_white_right", "pupil_left", "pupil_right",
                           "glint_left", "glint_right"):
        center = Vector(component_centers[component_name])
        inner_scale = 1.0 if component_name.startswith("eye_white") else pupil_relative_scale
        scale = Vector((eye_wide_scale[0] * inner_scale,
                        eye_wide_scale[1],
                        eye_wide_scale[2] * inner_scale))
        for index in component_groups[component_name]:
            delta = Vector(base_coords[index]) - center
            eye_wide.data[index].co = center + Vector((delta.x * scale.x,
                delta.y * scale.y, delta.z * scale.z))

    mouth_center = Vector(params["mouth"]["center_m"])
    scream_dims = params["mouth"]["scream_width_height_m"]
    closed_dims = params["mouth"]["closed_width_height_m"]
    mouth_width_offset = params["mouth"].get("scream_center_z_offset_m", 0.0)
    mouth_surface_offset = params["mouth"].get("surface_offset_m", 0.003)
    for index, point in enumerate(base_coords):
        if index not in component_groups["mouth"]:
            scream.data[index].co = (point[0], point[1] * 1.04, point[2] * 0.94)
        else:
            mouth_index = index - mouth_start
            u, v = mouth_uv[mouth_index]
            aperture = math.sqrt(max(0.0, 1.0 - u * u))
            mouth_x = mouth_center.x + u * scream_dims[0] * 0.5
            mouth_z = mouth_center.z + mouth_width_offset + v * scream_dims[1] * 0.5 * aperture
            undeformed_body_z = mouth_z / 0.94
            front_y = body_surface_y(params, mouth_x, undeformed_body_z)
            scream.data[index].co = (mouth_x, front_y * 1.04 - mouth_surface_offset, mouth_z)
        squash.data[index].co = (point[0] * 1.12, point[1] * 1.12, point[2] * 0.80)

    obj.data.shape_keys.name = "SK_NaiwaEgg_MorphTargets"
    for block in obj.data.shape_keys.key_blocks:
        block.slider_min = 0.0
        block.slider_max = 1.0
        if block.name != "Basis":
            block.value = 0.0
    visual_review = params.get("visual_review", "VISUAL_REVIEW_PENDING")
    report_status = "PASS_METRICS_VISUAL_REVIEW_PENDING"
    asset_status = "S0-S3_v4_metrics_pass_visual_review_pending"
    obj["asset_status"] = asset_status
    obj["collision_proxy_radius_m"] = params["dimensions"]["collision_sphere_radius_m"]
    obj["front_axis"] = "-Y"
    obj["morph_targets"] = "EyeWide,Scream,Squash"

    mesh.calc_loop_triangles()
    triangle_count = len(mesh.loop_triangles)
    body_points = [vertices[index] for index in component_groups["body"]]
    bounds = point_bounds(body_points)
    body_dimensions = [bounds[axis][1] - bounds[axis][0] for axis in range(3)]
    widest_ring = max(ring_ts, key=lambda t: pchip_value(params["profile"]["t"],
        params["profile"]["radius_x_m"], pchip_slopes(params["profile"]["t"],
        params["profile"]["radius_x_m"]), t))
    widest_z = (widest_ring - 0.5) * params["dimensions"]["height_m"]
    top_width = params["profile"].get("top_width_m",
        params["profile"]["radius_x_m"][-1] * 2.0)

    squash_coords = [tuple(point.co) for point in squash.data]
    base_volume = abs(signed_mesh_volume(base_coords, faces))
    squash_volume = abs(signed_mesh_volume(squash_coords, faces))
    volume_ratio = squash_volume / base_volume if base_volume else 0.0
    volume_error_percent = abs(volume_ratio - 1.0) * 100.0

    expected_keys = ["Basis", "EyeWide", "Scream", "Squash"]
    actual_keys = [block.name for block in obj.data.shape_keys.key_blocks]
    mouth_closed_height = (max(value for _, value in mouth_uv) -
                           min(value for _, value in mouth_uv)) * closed_dims[1] * 0.5
    mouth_scream_points = [tuple(scream.data[index].co) for index in component_groups["mouth"]]
    mouth_scream_height = max(point[2] for point in mouth_scream_points) - min(
        point[2] for point in mouth_scream_points)
    mouth_scream_width = max(point[0] for point in mouth_scream_points) - min(
        point[0] for point in mouth_scream_points)
    mouth_heights = [mouth_closed_height, mouth_scream_height]
    checks = {
        "one_body_mesh_object": sum(item.type == "MESH" for item in bpy.context.scene.objects) == 1,
        "body_height": abs(body_dimensions[2] - 1.15) <= 0.005,
        "body_width": abs(body_dimensions[0] - 0.85) <= 0.005,
        "body_depth": abs(body_dimensions[1] - 0.82) <= 0.005,
        "pivot_at_center": all(abs(value) <= 0.002 for value in obj.location),
        "widest_point_z": abs(widest_z + 0.184) <= 0.01,
        "round_top_width": abs(top_width - 0.068) <= 0.005,
        "triangle_budget": triangle_count <= params["mesh_budget"]["maximum_triangles"],
        "four_material_slots": len(mesh.materials) == 4,
        "surface_cap_eyes": eye_spec.get("construction") == "surface_cap" and all(
            len(component_groups[name]) == SEGMENTS + 1 for name in (
                "eye_white_left", "eye_white_right", "pupil_left", "pupil_right",
                "glint_left", "glint_right")),
        "eye_line_t": abs((eye_spec["centers_m"][0][2] + 0.575) / 1.15 - 0.815) <= 0.005,
        "exact_morph_keys": actual_keys == expected_keys,
        "morph_defaults_are_neutral": all(abs(block.value) <= 1.0e-9
            for block in obj.data.shape_keys.key_blocks if block.name != "Basis"),
        "closed_mouth_thickness": abs(mouth_heights[0] - 0.014) <= 0.001,
        "closed_mouth_smile_rise": abs(params["mouth"].get("smile_rise_m", 0.0) - 0.020) <= 0.001,
        "scream_mouth_height": abs(mouth_heights[1] - 0.100) <= 0.002,
        "scream_mouth_width": abs(mouth_scream_width - 0.200) <= 0.002,
        "squash_volume": volume_error_percent < 1.0,
    }
    if not all(checks.values()):
        raise AssertionError("S0-S3 acceptance failed: " + json.dumps(checks, ensure_ascii=False))

    configure_render_scene()
    bpy.context.scene.render.image_settings.color_mode = "RGBA"
    body_dims = body_dimensions
    render_view("Front", (0.0, -3.8, 0.0), (0.0, 0.0, 0.0), 1.50, "NaiwaEgg_front.png")
    render_view("Side", (3.8, 0.0, 0.0), (0.0, 0.0, 0.0), 1.50, "NaiwaEgg_side.png")
    render_view("Top", (0.0, 0.0, 3.8), (0.0, 0.0, 0.0), 1.50, "NaiwaEgg_top.png")
    scream.value = 1.0
    render_view("Scream", (0.0, -3.8, 0.0), (0.0, 0.0, 0.0), 1.50, "NaiwaEgg_Scream.png")
    scream.value = 0.0
    eye_wide.value = 1.0
    render_view("EyeWide", (0.0, -3.8, 0.0), (0.0, 0.0, 0.0), 1.50, "NaiwaEgg_EyeWide.png")
    eye_wide.value = 0.0
    squash.value = 1.0
    render_view("Squash", (0.0, -3.8, 0.0), (0.0, 0.0, 0.0), 1.50, "NaiwaEgg_Squash.png")
    squash.value = 0.0
    render_view("Aim96", (0.0, -3.8, 0.0), (0.0, 0.0, 0.0), 1.5333333333,
                "NaiwaEgg_aim_96px.png", (128, 128))
    render_view("Flight177Closed", (0.0, -3.8, 0.0), (0.0, 0.0, 0.0), 1.5333333333,
                "NaiwaEgg_flight_closed_177px.png", (236, 236))
    scream.value = 1.0
    render_view("Flight177Scream", (0.0, -3.8, 0.0), (0.0, 0.0, 0.0), 1.5333333333,
                "NaiwaEgg_flight_scream_177px.png", (236, 236))
    scream.value = 0.0
    bpy.context.scene.render.resolution_x = 768
    bpy.context.scene.render.resolution_y = 768
    bpy.context.view_layer.update()
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_FILE))

    report = {
        "status": report_status,
        "visual_review": visual_review,
        "revision_notes": params.get("revision_notes", [
            "Rebuilt from book p.11 figure 7.4: rounded cap, upper eye line, green eye field, dark pupils, raised-corner mouth.",
            "Preserved the specified body envelope, pivot, collision proxy, and morph key names.",
            "This generated preview is awaiting user visual review; it is not final art sign-off."
        ]),
        "blender_version": bpy.app.version_string,
        "spec_version": params["spec_version"],
        "blend_file": str(BLEND_FILE),
        "preview_files": [
            str(OUTPUT_DIR / name) for name in (
                "NaiwaEgg_front.png", "NaiwaEgg_side.png", "NaiwaEgg_top.png",
                "NaiwaEgg_Scream.png", "NaiwaEgg_EyeWide.png", "NaiwaEgg_Squash.png",
                "NaiwaEgg_aim_96px.png", "NaiwaEgg_flight_closed_177px.png",
                "NaiwaEgg_flight_scream_177px.png")
        ],
        "body_dimensions_m": body_dims,
        "widest_point_z_m": widest_z,
        "top_width_m": top_width,
        "eye_construction": eye_spec.get("construction"),
        "eye_centers_m": eye_spec.get("centers_m"),
        "eye_surface_layers": eye_layers,
        "morph_keys": actual_keys,
        "mouth_closed_and_scream_height_m": mouth_heights,
        "triangle_count": triangle_count,
        "mesh_radial_segments": SEGMENTS,
        "body_ring_count": BODY_RING_STEPS,
        "material_colors_hex": {name: spec.get("color")
            for name, spec in material_specs.items()},
        "material_slots": [material.name for material in mesh.materials],
        "squash_volume_ratio": volume_ratio,
        "squash_volume_error_percent": volume_error_percent,
        "checks": checks,
    }
    REPORT_FILE.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print("NAIWA_EGG_S0_S3_PASS " + json.dumps(report, ensure_ascii=False))


try:
    main()
except Exception:
    failure = {
        "status": "FAIL_S0_S3",
        "error": traceback.format_exc(),
        "blender_version": bpy.app.version_string,
    }
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    REPORT_FILE.write_text(json.dumps(failure, indent=2, ensure_ascii=False), encoding="utf-8")
    print("NAIWA_EGG_S0_S3_FAIL " + failure["error"])
    sys.exit(1)
