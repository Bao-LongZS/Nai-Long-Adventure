"""Build the four P2 special-egg Skeletal Mesh variants from the reviewed base egg."""

import argparse
import hashlib
import json
import math
import pathlib
import sys
import traceback

import bpy
import bmesh
from mathutils import Vector


ROOT = pathlib.Path(__file__).resolve().parents[1]
BLENDER_ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
PARSER = argparse.ArgumentParser()
PARSER.add_argument("--source-blend", type=pathlib.Path)
PARSER.add_argument("--params", type=pathlib.Path)
PARSER.add_argument("--output-dir", type=pathlib.Path)
ARGS, _ = PARSER.parse_known_args(BLENDER_ARGS)
SOURCE_BLEND = (ARGS.source_blend or ROOT / "NaiwaCharacter/deliveries/NaiwaEgg_S4-S5_v3_candidate/SK_NaiwaEgg_S4-S5.blend").resolve()
PARAMS_FILE = (ARGS.params or ROOT / "NaiwaCharacter/parameters/NaiwaEgg_Parameters_v4.json").resolve()
OUTPUT_DIR = (ARGS.output_dir or ROOT / "NaiwaCharacter/deliveries/NaiwaEgg_Features_v1").resolve()
MORPH_NAMES = ("EyeWide", "Scream", "Squash")
BASE_EXPORT_REPORT = SOURCE_BLEND.parent / "NaiwaEgg_S4-S5_export_report.json"
VARIANT_COLORS = {
    "Chick": "#F5C542",
    "Mouse": "#E6D9B8",
    "Soy": "#CFE0A8",
    "Tea": "#C89A6A",
}
ACCENT_COLORS = {
    "Chick": "#E8663C",
    "Mouse": None,
    "Soy": "#C0392B",
    "Tea": "#F5EAD2",
}
FEATURE_GROUPS = {
    "Chick": "FEAT_CREST",
    "Mouse": "FEAT_EARS",
    "Soy": "FEAT_SCARF",
    "Tea": "FEAT_STRAW",
}


def sha256(path):
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
    return tuple(linear_channel(int(value[index:index + 2], 16)) for index in (0, 2, 4)) + (1.0,)


def set_material_color(material, hex_value):
    rgba = linear_color(hex_value)
    material.diffuse_color = rgba
    material.use_nodes = True
    shader = next((node for node in material.node_tree.nodes if node.type == "BSDF_PRINCIPLED"), None)
    if shader is None:
        raise RuntimeError(f"No Principled BSDF in {material.name}")
    shader.inputs["Base Color"].default_value = rgba


class MeshBuilder:
    def __init__(self):
        self.vertices = []
        self.faces = []

    def vertex(self, value):
        self.vertices.append(tuple(float(component) for component in value))
        return len(self.vertices) - 1

    def face(self, *indices):
        self.faces.append(tuple(indices))


def add_ellipsoid(builder, center, radii, segments, stacks, rotate_y=0.0):
    """Append a closed, triangulated ellipsoid with poles on its local Z axis."""
    center = Vector(center)
    rx, ry, rz = radii
    rotation = math.radians(rotate_y)

    def transform(x, y, z):
        rotated_x = x * math.cos(rotation) + z * math.sin(rotation)
        rotated_z = -x * math.sin(rotation) + z * math.cos(rotation)
        return center + Vector((rotated_x, y, rotated_z))

    top = builder.vertex(transform(0.0, 0.0, rz))
    rings = []
    for stack in range(1, stacks):
        latitude = math.pi * stack / stacks
        ring = []
        for segment in range(segments):
            longitude = 2.0 * math.pi * segment / segments
            ring.append(builder.vertex(transform(
                rx * math.sin(latitude) * math.cos(longitude),
                ry * math.sin(latitude) * math.sin(longitude),
                rz * math.cos(latitude))))
        rings.append(ring)
    bottom = builder.vertex(transform(0.0, 0.0, -rz))

    first = rings[0]
    for segment in range(segments):
        nxt = (segment + 1) % segments
        builder.face(top, first[segment], first[nxt])
    for upper, lower in zip(rings, rings[1:]):
        for segment in range(segments):
            nxt = (segment + 1) % segments
            builder.face(upper[segment], lower[segment], lower[nxt], upper[nxt])
    last = rings[-1]
    for segment in range(segments):
        nxt = (segment + 1) % segments
        builder.face(last[segment], bottom, last[nxt])


def add_octahedron(builder, center, radii):
    """Append a closed eight-triangle ellipsoid suitable for a low-poly lobe or knot."""
    center = Vector(center)
    rx, ry, rz = radii
    points = [
        center + Vector((0.0, 0.0, rz)),
        center + Vector((0.0, 0.0, -rz)),
        center + Vector((rx, 0.0, 0.0)),
        center + Vector((-rx, 0.0, 0.0)),
        center + Vector((0.0, ry, 0.0)),
        center + Vector((0.0, -ry, 0.0)),
    ]
    ids = [builder.vertex(point) for point in points]
    top, bottom, right, left, back, front = ids
    builder.face(top, right, front)
    builder.face(top, front, left)
    builder.face(top, left, back)
    builder.face(top, back, right)
    builder.face(bottom, front, right)
    builder.face(bottom, left, front)
    builder.face(bottom, back, left)
    builder.face(bottom, right, back)


def add_box(builder, center, size):
    center = Vector(center)
    hx, hy, hz = (value * 0.5 for value in size)
    corners = [
        (-hx, -hy, -hz), (hx, -hy, -hz), (hx, hy, -hz), (-hx, hy, -hz),
        (-hx, -hy, hz), (hx, -hy, hz), (hx, hy, hz), (-hx, hy, hz),
    ]
    ids = [builder.vertex(center + Vector(corner)) for corner in corners]
    for face in (
        (0, 3, 2, 1), (4, 5, 6, 7),
        (0, 1, 5, 4), (1, 2, 6, 5),
        (2, 3, 7, 6), (3, 0, 4, 7),
    ):
        builder.face(*(ids[index] for index in face))


def add_tapered_tail(builder, geometry):
    """Append the closed, slightly outward-leaning soy scarf tail prism."""
    top_center_x = float(geometry["knot_center_m"][0])
    bottom_center_x = float(geometry["tail_center_x_m"])
    top_z = float(geometry["tail_from_z_m"])
    bottom_z = float(geometry["tail_to_z_m"])
    center_y = float(geometry["knot_center_m"][1])
    depth = float(geometry["tail_section_m"][1])
    top_width = 0.09
    bottom_width = float(geometry["tail_section_m"][0])
    front_y = center_y - depth * 0.5
    back_y = center_y + depth * 0.5
    specs = [
        (top_center_x - top_width * 0.5, front_y, top_z),
        (top_center_x + top_width * 0.5, front_y, top_z),
        (top_center_x + top_width * 0.5, back_y, top_z),
        (top_center_x - top_width * 0.5, back_y, top_z),
        (bottom_center_x - bottom_width * 0.5, front_y, bottom_z),
        (bottom_center_x + bottom_width * 0.5, front_y, bottom_z),
        (bottom_center_x + bottom_width * 0.5, back_y, bottom_z),
        (bottom_center_x - bottom_width * 0.5, back_y, bottom_z),
    ]
    ids = [builder.vertex(point) for point in specs]
    builder.face(ids[0], ids[3], ids[2], ids[1])
    builder.face(ids[4], ids[5], ids[6], ids[7])
    builder.face(ids[0], ids[1], ids[5], ids[4])
    builder.face(ids[1], ids[2], ids[6], ids[5])
    builder.face(ids[2], ids[3], ids[7], ids[6])
    builder.face(ids[3], ids[0], ids[4], ids[7])


def add_closed_scarf_ring(builder, geometry, segments=12):
    center_z = float(geometry["band_z_m"])
    half_h = float(geometry["band_h_m"]) * 0.5
    outer_x = float(geometry["band_x_m"])
    outer_y = float(geometry["band_y_m"])
    inner_x = float(geometry["inner_x_m"])
    inner_y = float(geometry["inner_y_m"])
    loops = {name: [] for name in ("outer_lower", "outer_upper", "inner_lower", "inner_upper")}
    for index in range(segments):
        angle = 2.0 * math.pi * index / segments
        cosine, sine = math.cos(angle), math.sin(angle)
        for name, rx, ry, z in (
            ("outer_lower", outer_x, outer_y, center_z - half_h),
            ("outer_upper", outer_x, outer_y, center_z + half_h),
            ("inner_lower", inner_x, inner_y, center_z - half_h),
            ("inner_upper", inner_x, inner_y, center_z + half_h),
        ):
            loops[name].append(builder.vertex((rx * cosine, ry * sine, z)))
    for index in range(segments):
        nxt = (index + 1) % segments
        ol, ou = loops["outer_lower"], loops["outer_upper"]
        il, iu = loops["inner_lower"], loops["inner_upper"]
        builder.face(ol[index], ol[nxt], ou[nxt], ou[index])
        builder.face(il[nxt], il[index], iu[index], iu[nxt])
        builder.face(ou[index], ou[nxt], iu[nxt], iu[index])
        builder.face(ol[nxt], ol[index], il[index], il[nxt])


def add_cylinder_between(builder, start, end, radius, segments):
    start, end = Vector(start), Vector(end)
    direction = (end - start).normalized()
    tangent = direction.cross(Vector((0.0, 1.0, 0.0))).normalized()
    bitangent = direction.cross(tangent).normalized()
    rings = []
    for center in (start, end):
        ring = []
        for segment in range(segments):
            angle = 2.0 * math.pi * segment / segments
            point = center + radius * (math.cos(angle) * tangent + math.sin(angle) * bitangent)
            ring.append(builder.vertex(point))
        rings.append(ring)
    for segment in range(segments):
        nxt = (segment + 1) % segments
        builder.face(rings[0][segment], rings[0][nxt], rings[1][nxt], rings[1][segment])
    builder.face(*reversed(rings[0]))
    builder.face(*rings[1])


def make_feature_mesh(kind, spec):
    feature = spec["variant_features"][kind.lower()]
    geometry = feature["geometry"]
    builder = MeshBuilder()

    if kind == "Chick":
        offsets = geometry["offsets_x_m"]
        heights = geometry["lobe_h_m"]
        base_z = float(geometry["base_z_m"])
        center_y = float(geometry["base_center_y_m"])
        half_depth = float(geometry["thickness_m"]) * 0.5
        for x, height in zip(offsets, heights):
            add_octahedron(builder,
                (float(x), center_y, base_z + float(height) * 0.5),
                (0.05, half_depth, float(height) * 0.5))

    elif kind == "Mouse":
        radius = float(geometry["radius_m"])
        half_depth = float(geometry["thickness_m"]) * 0.5
        centers_x = geometry["centers_x_m"]
        center_z = float(geometry["z_m"])
        center_y = float(geometry["center_y_m"]) + float(geometry["inner_sink_m"])
        tilt = float(geometry["tilt_out_deg"])
        for side, center_x in ((-1.0, centers_x[0]), (1.0, centers_x[1])):
            add_ellipsoid(builder,
                (float(center_x), center_y, center_z),
                (radius, half_depth, radius),
                segments=11, stacks=3, rotate_y=side * tilt)

    elif kind == "Soy":
        add_closed_scarf_ring(builder, geometry, segments=12)
        add_tapered_tail(builder, geometry)
        add_octahedron(builder,
            geometry["knot_center_m"],
            (0.055, 0.035, 0.045))

    elif kind == "Tea":
        root = Vector(geometry["straw_root_m"])
        direction = Vector(geometry["straw_dir"]).normalized()
        embedded = float(geometry["straw_embedded_m"])
        exposed = float(geometry["straw_exposed_m"])
        start = root - direction * embedded
        end = root + direction * exposed
        add_cylinder_between(builder, start, end,
            float(geometry["straw_r_m"]), int(geometry["straw_segments"]))

    else:
        raise ValueError(f"Unsupported special egg: {kind}")

    return builder, feature


def render_preview(egg, kind, path, resolution, ortho_scale, squash=False):
    scene = bpy.context.scene
    camera_data = bpy.data.cameras.new(f"NaiwaEgg_{kind}_PreviewCamera")
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = ortho_scale
    camera = bpy.data.objects.new(f"NaiwaEgg_{kind}_PreviewCamera", camera_data)
    scene.collection.objects.link(camera)
    camera.location = (0.0, -3.8, 0.0)
    direction = Vector((0.0, 0.0, 0.0)) - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    old_camera = scene.camera
    scene.camera = camera
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = resolution
    scene.render.resolution_y = resolution
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.film_transparent = False
    scene.render.filepath = str(path)
    scene.view_settings.view_transform = "Standard"
    squash_key = egg.data.shape_keys.key_blocks.get("Squash")
    if squash_key:
        squash_key.value = 1.0 if squash else 0.0
    bpy.context.view_layer.update()
    bpy.ops.render.render(write_still=True)
    if squash_key:
        squash_key.value = 0.0
    scene.camera = old_camera
    bpy.data.objects.remove(camera, do_unlink=True)
    bpy.data.cameras.remove(camera_data)


def assert_closed_mesh(faces):
    edge_counts = {}
    for face in faces:
        for index in range(len(face)):
            left = face[index]
            right = face[(index + 1) % len(face)]
            key = (left, right) if left < right else (right, left)
            edge_counts[key] = edge_counts.get(key, 0) + 1
    boundary = [edge for edge, count in edge_counts.items() if count != 2]
    if boundary:
        raise RuntimeError(f"Feature geometry is not a closed solid: {len(boundary)} non-manifold edges")


def squash_point(point):
    return (point[0] * 1.12, point[1] * 1.12, point[2] * 0.80)


def convex_hull_2d(points):
    unique = sorted(set((float(point[0]), float(point[1])) for point in points))
    if len(unique) < 3:
        raise RuntimeError("Body silhouette hull needs at least three points")

    def cross(origin, a, b):
        return (a[0] - origin[0]) * (b[1] - origin[1]) - (a[1] - origin[1]) * (b[0] - origin[0])

    lower = []
    for point in unique:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], point) <= 0.0:
            lower.pop()
        lower.append(point)
    upper = []
    for point in reversed(unique):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], point) <= 0.0:
            upper.pop()
        upper.append(point)
    return lower[:-1] + upper[:-1]


def point_segment_distance(point, start, end):
    p, a, b = Vector(point), Vector(start), Vector(end)
    segment = b - a
    denominator = segment.length_squared
    if denominator <= 1.0e-12:
        return (p - a).length
    t = max(0.0, min(1.0, (p - a).dot(segment) / denominator))
    return (p - (a + segment * t)).length


def point_inside_convex_hull(point, hull):
    signs = []
    for index, start in enumerate(hull):
        end = hull[(index + 1) % len(hull)]
        value = (end[0] - start[0]) * (point[1] - start[1]) - (end[1] - start[1]) * (point[0] - start[0])
        if abs(value) > 1.0e-7:
            signs.append(value > 0.0)
    return not signs or all(sign == signs[0] for sign in signs)


def maximum_silhouette_break(body_points, feature_points):
    body_projected = [(point[0], point[2]) for point in body_points]
    hull = convex_hull_2d(body_projected)
    maximum = 0.0
    for point in feature_points:
        projected = (point[0], point[2])
        if point_inside_convex_hull(projected, hull):
            continue
        distance = min(point_segment_distance(projected, hull[index], hull[(index + 1) % len(hull)])
                       for index in range(len(hull)))
        maximum = max(maximum, distance)
    return maximum


def make_variant(kind, params):
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE_BLEND))
    egg = bpy.data.objects.get("SK_NaiwaEgg")
    rig = bpy.data.objects.get("SK_NaiwaEgg_Skeleton")
    if egg is None or egg.type != "MESH" or rig is None or rig.type != "ARMATURE":
        raise RuntimeError("Reviewed base blend must contain SK_NaiwaEgg and SK_NaiwaEgg_Skeleton")
    source_mesh = egg.data
    if source_mesh.shape_keys is None:
        raise RuntimeError("Reviewed base mesh is missing morph targets")
    source_keys = {key.name: key for key in source_mesh.shape_keys.key_blocks}
    if set(source_keys) != {"Basis", *MORPH_NAMES}:
        raise RuntimeError(f"Unexpected source morph set: {list(source_keys)}")
    if len(source_mesh.materials) != 4:
        raise RuntimeError(f"Source base must have four stable material slots; found {len(source_mesh.materials)}")

    builder, feature_spec = make_feature_mesh(kind, params)
    assert_closed_mesh(builder.faces)
    body_vertex_count = len(source_mesh.vertices)
    body_face_count = len(source_mesh.polygons)
    body_triangle_count = sum(len(poly.vertices) - 2 for poly in source_mesh.polygons)
    body_basis = [tuple(point.co) for point in source_keys["Basis"].data]
    body_morphs = {name: [tuple(point.co) for point in source_keys[name].data] for name in MORPH_NAMES}
    body_faces = [tuple(poly.vertices) for poly in source_mesh.polygons]
    body_material_indices = [poly.material_index for poly in source_mesh.polygons]
    body_smooth = [poly.use_smooth for poly in source_mesh.polygons]

    meshes, feature_faces = builder.vertices, builder.faces
    feature_vertex_count = len(meshes)
    merged_faces = body_faces + [tuple(body_vertex_count + index for index in face) for face in feature_faces]
    all_vertices_basis = body_basis + list(meshes)
    merged_mesh = bpy.data.meshes.new(f"SK_NaiwaEgg_{kind}_Mesh")
    merged_mesh.from_pydata(all_vertices_basis, [], merged_faces)
    merged_mesh.update(calc_edges=True)
    if len(merged_mesh.vertices) != body_vertex_count + feature_vertex_count:
        raise RuntimeError("Blender changed the expected vertex count while building the variant")

    body_material = source_mesh.materials[0]
    set_material_color(body_material, VARIANT_COLORS[kind])
    accent_material = bpy.data.materials.new("M_NaiwaEgg_Accent")
    accent_material.diffuse_color = linear_color(ACCENT_COLORS[kind] or VARIANT_COLORS[kind])
    accent_material.use_nodes = True
    accent_shader = next((node for node in accent_material.node_tree.nodes if node.type == "BSDF_PRINCIPLED"), None)
    if accent_shader is None:
        raise RuntimeError("Could not initialize accent material")
    accent_shader.inputs["Base Color"].default_value = accent_material.diffuse_color
    for material in source_mesh.materials:
        merged_mesh.materials.append(material)
    merged_mesh.materials.append(accent_material)

    for polygon_index, polygon in enumerate(merged_mesh.polygons):
        if polygon_index < body_face_count:
            polygon.material_index = body_material_indices[polygon_index]
            polygon.use_smooth = body_smooth[polygon_index]
        else:
            polygon.material_index = 0 if kind == "Mouse" else 4
            polygon.use_smooth = kind == "Mouse"

    # Recalculate all feature shell normals without changing vertex order or materials.
    bm = bmesh.new()
    bm.from_mesh(merged_mesh)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(merged_mesh)
    bm.free()
    merged_mesh.update(calc_edges=True)

    # Preserve the exact reviewed body topology and isolate each authored feature group.
    for group in list(egg.vertex_groups):
        egg.vertex_groups.remove(group)
    egg.data = merged_mesh
    root_group = egg.vertex_groups.new(name="Root")
    root_group.add(list(range(body_vertex_count + feature_vertex_count)), 1.0, "REPLACE")
    body_group = egg.vertex_groups.new(name="BODY")
    body_group.add(list(range(body_vertex_count)), 1.0, "REPLACE")
    feature_group_name = FEATURE_GROUPS[kind]
    feature_group = egg.vertex_groups.new(name=feature_group_name)
    feature_group.add(list(range(body_vertex_count, body_vertex_count + feature_vertex_count)), 1.0, "REPLACE")

    for modifier in egg.modifiers:
        if modifier.type == "ARMATURE":
            modifier.object = rig
    if not any(modifier.type == "ARMATURE" for modifier in egg.modifiers):
        armature_modifier = egg.modifiers.new("NaiwaEgg_RootSkin", "ARMATURE")
        armature_modifier.object = rig

    def feature_morph(name, point):
        x, y, z = point
        if name == "EyeWide":
            return (x, y, z)
        if name == "Scream":
            return (x, y * 1.04, z * 0.94)
        if name == "Squash":
            return squash_point(point)
        raise ValueError(name)

    for name, coordinates in (("Basis", all_vertices_basis),):
        key = egg.shape_key_add(name=name, from_mix=False)
        for index, coordinate in enumerate(coordinates):
            key.data[index].co = Vector(coordinate)
    for name in MORPH_NAMES:
        coordinates = body_morphs[name] + [feature_morph(name, point) for point in meshes]
        key = egg.shape_key_add(name=name, from_mix=False)
        for index, coordinate in enumerate(coordinates):
            key.data[index].co = Vector(coordinate)
        key.slider_min = 0.0
        key.slider_max = 1.0
        key.value = 0.0
    egg.data.shape_keys.name = f"SK_NaiwaEgg_{kind}_MorphTargets"

    bpy.context.view_layer.update()
    merged_mesh.calc_loop_triangles()
    triangle_count = len(merged_mesh.loop_triangles)
    if triangle_count != body_triangle_count + sum(len(face) - 2 for face in feature_faces):
        raise RuntimeError(f"Unexpected triangle count for {kind}: {triangle_count}")
    if triangle_count > int(params["mesh_budget"]["maximum_triangles"]):
        raise RuntimeError(f"{kind} exceeds the P2 mesh budget: {triangle_count}")

    body_basis_after = [tuple(egg.data.shape_keys.key_blocks["Basis"].data[index].co)
                        for index in range(body_vertex_count)]
    body_error = max((Vector(a) - Vector(b)).length for a, b in zip(body_basis, body_basis_after))
    if body_error > 1.0e-8:
        raise RuntimeError(f"{kind} changed the approved base body coordinates: {body_error}")

    morph_set = [key.name for key in egg.data.shape_keys.key_blocks]
    if morph_set != ["Basis", *MORPH_NAMES]:
        raise RuntimeError(f"{kind} has an unexpected morph order: {morph_set}")
    root_bones = [bone.name for bone in rig.data.bones]
    if root_bones != ["Root"]:
        raise RuntimeError(f"{kind} must share the one-bone Root skeleton: {root_bones}")

    body_squash = [squash_point(point) for point in body_basis]
    feature_squash = [squash_point(point) for point in meshes]
    silhouette_break_m = maximum_silhouette_break(body_squash, feature_squash)
    min_break_m = 8.0 / 83.5
    if silhouette_break_m + 1.0e-6 < min_break_m:
        raise RuntimeError(
            f"{kind} Squash silhouette break is below 8px at 96px body height: "
            f"{silhouette_break_m:.5f}m < {min_break_m:.5f}m")

    # Capture feature/body bounds from the final exported vertex sets, not from object bounds.
    def bounds(points):
        return {
            axis: [min(point[index] for point in points), max(point[index] for point in points)]
            for axis, index in (("x", 0), ("y", 1), ("z", 2))
        }

    feature_triangles = sum(len(face) - 2 for face in feature_faces)
    checks = {
        "reviewed_base_vertices_unchanged": body_error <= 1.0e-8,
        "feature_closed_solid": True,
        "one_root_bone": root_bones == ["Root"],
        "all_vertices_weighted_to_root": False,
        "three_named_morphs": morph_set == ["Basis", *MORPH_NAMES],
        "feature_group_isolated": feature_vertex_count > 0,
        "triangle_budget": triangle_count <= int(params["mesh_budget"]["maximum_triangles"]),
        "squash_silhouette_break_8px": silhouette_break_m + 1.0e-6 >= min_break_m,
        "body_vertex_count_unchanged": len(body_basis_after) == body_vertex_count,
    }
    # Confirm all Root weights are exactly one and feature vertices are excluded from BODY.
    root_ok = all(abs(root_group.weight(index) - 1.0) <= 1.0e-6
                  for index in range(body_vertex_count + feature_vertex_count))
    def group_weight(group, index):
        try:
            return group.weight(index)
        except RuntimeError:
            return None

    group_ok = all(abs(group_weight(body_group, index) - 1.0) <= 1.0e-6
                   for index in range(body_vertex_count)) and \
        all(group_weight(body_group, index) is None
            for index in range(body_vertex_count, body_vertex_count + feature_vertex_count)) and \
        all(group_weight(feature_group, index) is None for index in range(body_vertex_count)) and \
        all(abs(group_weight(feature_group, index) - 1.0) <= 1.0e-6
            for index in range(body_vertex_count, body_vertex_count + feature_vertex_count))
    checks["all_vertices_weighted_to_root"] = root_ok
    checks["feature_group_isolated"] = group_ok
    if not all(checks.values()):
        raise RuntimeError(f"{kind} validation failed: {checks}")

    render_preview(egg, kind, OUTPUT_DIR / f"NaiwaEgg_{kind}_177px.png",
        resolution=236, ortho_scale=1.5333333333, squash=False)
    render_preview(egg, kind, OUTPUT_DIR / f"NaiwaEgg_{kind}_Squash_96px.png",
        resolution=112, ortho_scale=1.07, squash=True)

    # Export the variant in the existing root skeleton and keep the in-place .blend source.
    variant_mesh_name = f"SK_NaiwaEgg_{kind}"
    egg.name = variant_mesh_name
    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 1.0
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.select_all(action="DESELECT")
    egg.select_set(True)
    rig.select_set(True)
    blend_path = OUTPUT_DIR / f"{variant_mesh_name}.blend"
    fbx_path = OUTPUT_DIR / f"{variant_mesh_name}.fbx"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))
    bpy.ops.export_scene.fbx(
        filepath=str(fbx_path),
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

    base_report = json.loads(BASE_EXPORT_REPORT.read_text(encoding="utf-8")) \
        if BASE_EXPORT_REPORT.is_file() else {}
    report = {
        "status": "PASS_BLENDER_FEATURE_VARIANT",
        "kind": kind,
        "source_blend": str(SOURCE_BLEND),
        "base_source_spec_version": base_report.get("source_spec_version"),
        "base_source_fbx_sha256": base_report.get("fbx_sha256"),
        "feature_spec_version": params.get("spec_version"),
        "output_blend": str(blend_path),
        "fbx": str(fbx_path),
        "fbx_sha256": sha256(fbx_path) if fbx_path.is_file() else None,
        "blender_version": bpy.app.version_string,
        "mesh": variant_mesh_name,
        "skeleton": rig.name,
        "root_bones": root_bones,
        "body_vertex_count": body_vertex_count,
        "feature_vertex_count": feature_vertex_count,
        "body_triangle_count": body_triangle_count,
        "feature_triangle_count": feature_triangles,
        "triangle_count": triangle_count,
        "morph_targets": morph_set[1:],
        "material_slots": [material.name if material else None for material in merged_mesh.materials],
        "body_bounds_m": bounds(body_basis),
        "feature_bounds_m": bounds(meshes),
        "squash_body_bounds_m": bounds(body_squash),
        "squash_feature_bounds_m": bounds(feature_squash),
        "squash_silhouette_break_m": silhouette_break_m,
        "squash_silhouette_break_px_at_96px_body": silhouette_break_m * 83.5,
        "feature_spec": feature_spec,
        "checks": checks,
    }
    report_path = OUTPUT_DIR / f"{variant_mesh_name}_build_report.json"
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"NAIWA_EGG_FEATURE_BUILD_PASS {json.dumps(report, ensure_ascii=False)}")
    return egg


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    stale_failure = OUTPUT_DIR / "NaiwaEgg_FeatureBuild_Failure.json"
    if stale_failure.exists():
        stale_failure.unlink()
    if not SOURCE_BLEND.is_file() or not PARAMS_FILE.is_file():
        raise FileNotFoundError("The reviewed base blend and feature parameters must exist")
    params = json.loads(PARAMS_FILE.read_text(encoding="utf-8"))
    if params.get("coordinate_system", {}).get("pivot") != "geometric_center":
        raise RuntimeError("The feature build requires the geometric-center pivot contract")
    variant_reports = []
    for kind in ("Chick", "Mouse", "Soy", "Tea"):
        make_variant(kind, params)
        report_path = OUTPUT_DIR / f"SK_NaiwaEgg_{kind}_build_report.json"
        variant_reports.append(json.loads(report_path.read_text(encoding="utf-8")))
    summary = {
        "status": "PASS_ALL_SPECIAL_EGGS",
        "source_blend": str(SOURCE_BLEND),
        "parameter_file": str(PARAMS_FILE),
        "variants": variant_reports,
    }
    summary_path = OUTPUT_DIR / "NaiwaEgg_FeatureBuild_Summary.json"
    summary_path.write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"NAIWA_EGG_FEATURES_COMPLETE {json.dumps(summary, ensure_ascii=False)}")


try:
    main()
except Exception:
    failure = {"status": "FAIL_SPECIAL_EGGS", "error": traceback.format_exc()}
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    (OUTPUT_DIR / "NaiwaEgg_FeatureBuild_Failure.json").write_text(
        json.dumps(failure, indent=2, ensure_ascii=False), encoding="utf-8")
    print("NAIWA_EGG_FEATURE_BUILD_FAIL " + failure["error"])
    sys.exit(1)
