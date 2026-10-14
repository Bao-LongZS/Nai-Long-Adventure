"""Independently compare the saved broad-face refinement with its baseline.

Run in Blender with --python and optionally -- --draft. This script reads the
two meshes and does not import or reproduce the sculpt's target curves.
"""
import bpy
import sys
import json
import math
import hashlib
from pathlib import Path

import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree


OUT = Path(__file__).resolve().parent
BASELINE = OUT / 'iterations/before_broad_face_arc_refinement/Naiwa_Character.blend'
TARGET = OUT / 'Naiwa_Character.blend'
if '--draft' in sys.argv:
    TARGET = OUT / 'iterations/broad_face_arc_draft/Naiwa_Character.blend'

NAMES = [
    'Naiwa unified golden body', 'Head and torso silhouette only',
    'L concentric iris and pupil', 'R concentric iris and pupil',
    'L eye glint', 'R eye glint', 'Short closed oral seam', 'Naiwa_Rig',
]


def attribute(collection, prop, size, dtype):
    result = np.empty(len(collection) * size, dtype=dtype)
    collection.foreach_get(prop, result)
    return result


def coordinates(obj):
    return attribute(obj.data.vertices, 'co', 3, np.float32).reshape((-1, 3))


def section(obj, z):
    front_hit, front, _, _ = obj.ray_cast(Vector((0, -2, z)), Vector((0, 1, 0)))
    rear_hit, rear, _, _ = obj.ray_cast(Vector((0, 2, z)), Vector((0, -1, 0)))
    assert front_hit and rear_hit, ('Sagittal ray missed mesh', obj.name, z)
    side_hit, side, _, _ = obj.ray_cast(
        Vector((2, (front.y + rear.y) / 2, z)), Vector((-1, 0, 0)))
    assert side_hit, ('Side section ray missed mesh', obj.name, z)
    return np.array((side.x, front.y, rear.y), dtype=float)


def topology(a, b):
    assert len(a.data.vertices) == len(b.data.vertices), ('Vertex count changed', b.name)
    for collection, prop, size in (
        ('edges', 'vertices', 2), ('loops', 'vertex_index', 1),
        ('polygons', 'loop_start', 1), ('polygons', 'loop_total', 1),
        ('polygons', 'material_index', 1),
    ):
        p = attribute(getattr(a.data, collection), prop, size, np.int32)
        q = attribute(getattr(b.data, collection), prop, size, np.int32)
        assert np.array_equal(p, q), ('Mesh topology/material assignment changed', b.name, collection)


def weight_hash(obj, bone_names):
    groups = {group.index: group.name for group in obj.vertex_groups if group.name in bone_names}
    digest = hashlib.sha256()
    for vertex in obj.data.vertices:
        pairs = sorted((groups[group.group], group.weight)
                       for group in vertex.groups if group.group in groups)
        for name, weight in pairs:
            digest.update(f'{vertex.index}:{name}:{weight}'.encode())
    return digest.hexdigest()


def mouth_gaps(body, points):
    gaps = []
    for point in points:
        hit, surface, _, _ = body.ray_cast(
            Vector((float(point[0]), -2, float(point[2]))), Vector((0, 1, 0)))
        assert hit, 'Mouth surface ray missed mesh'
        gaps.append(float(surface.y) - float(point[1]))
    return np.array(gaps)


def surface_integrity(obj):
    xyz = coordinates(obj)
    affected = (xyz[:, 2] >= 1.98) & (xyz[:, 2] <= 2.54)
    points = xyz[affected]
    duplicates = len(points) - len(np.unique(np.round(points, 7), axis=0))
    # Include faces crossing the boundary as well as those wholly inside it.
    faces = [polygon for polygon in obj.data.polygons
             if min(xyz[i, 2] for i in polygon.vertices) <= 2.54
             and max(xyz[i, 2] for i in polygon.vertices) >= 1.98]
    polygons = [tuple(polygon.vertices) for polygon in faces]
    signatures = [tuple(sorted(vertices)) for vertices in polygons]
    duplicates_faces = len(signatures) - len(set(signatures))
    bvh = BVHTree.FromPolygons([Vector(point) for point in xyz], polygons,
                              all_triangles=False, epsilon=0.0)
    crossings = []
    for a, b in bvh.overlap(bvh):
        if a >= b or set(polygons[a]).intersection(polygons[b]):
            continue
        crossings.append((a, b))
    normals = attribute(obj.data.vertices, 'normal', 3, np.float32).reshape((-1, 3))
    outward = affected & (np.abs(xyz[:, 0]) > 0.1)
    result = {
        'duplicate_vertices_at_1e_7': int(duplicates),
        'duplicate_faces': int(duplicates_faces),
        'degenerate_faces': int(sum(polygon.area < 1e-12 for polygon in faces)),
        'nonadjacent_intersections': len(crossings),
        'normal_unit_error': float(np.max(np.abs(np.linalg.norm(normals[affected], axis=1) - 1))),
        'minimum_outward_x': float(np.min(normals[outward, 0] * np.sign(xyz[outward, 0]))),
        'custom_normals': bool(obj.data.has_custom_normals),
        'sharp_edges': int(sum(edge.use_edge_sharp for edge in obj.data.edges)),
        'all_faces_smooth': bool(all(polygon.use_smooth for polygon in obj.data.polygons)),
    }
    assert duplicates == duplicates_faces == result['degenerate_faces'] == len(crossings) == 0, result
    assert result['normal_unit_error'] < 1e-5 and result['minimum_outward_x'] > 0, result
    assert not result['custom_normals'] and result['sharp_edges'] == 0 and result['all_faces_smooth'], result
    return result


def profile_derivatives(zs, values):
    # Fit a physical 0.048-long neighborhood to suppress polygon/ray sampling
    # noise. This measures the actual saved mesh, not the sculpt's curve.
    radius = 0.024
    derivatives = np.empty((len(zs), 3), dtype=float)
    for i, z in enumerate(zs):
        local = np.abs(zs - z) <= radius + 1e-10
        t = (zs[local] - z) / radius
        coefficients = np.polynomial.polynomial.polyfit(t, values[local], 3)
        derivatives[i] = (coefficients[1] / radius,
                          2 * coefficients[2] / radius**2,
                          6 * coefficients[3] / radius**3)
    return derivatives


def monotonic_stats(values):
    steps = np.diff(values)
    return {
        'maximum_upward_step': float(np.max(steps)),
        'total_positive_excursion': float(np.maximum(steps, 0).sum()),
    }


def angular_diagnostics(old_body, new_body):
    # Rays share the baseline center and direction. Record anterior vs posterior
    # changes; do not impose a particular mask or prohibit natural neck widening.
    result = []
    for z in (2.12, 2.16, 2.19, 2.22, 2.26, 2.30, 2.34, 2.38):
        old_section = section(old_body, z)
        center = Vector((0, float((old_section[1] + old_section[2]) / 2), z))
        samples = []
        for angle in range(0, 360, 30):
            radians = math.radians(angle)
            direction = Vector((math.cos(radians), math.sin(radians), 0))
            origin = center + direction * 2
            a_hit, a, _, _ = old_body.ray_cast(origin, -direction)
            b_hit, b, _, _ = new_body.ray_cast(origin, -direction)
            if not (a_hit and b_hit):
                samples.append({'angle_degrees': angle, 'ray_hit': False})
                continue
            delta = b - a
            samples.append({
                'angle_degrees': angle, 'ray_hit': True,
                'delta_y': float(delta.y), 'radial_change': float(delta.dot(direction)),
                'half': 'front' if direction.y < -0.1 else ('rear' if direction.y > 0.1 else 'side'),
            })
        front = [abs(sample['delta_y']) for sample in samples
                 if sample.get('half') == 'front']
        rear = [abs(sample['delta_y']) for sample in samples
                if sample.get('half') == 'rear']
        front_max, rear_max = max(front, default=0), max(rear, default=0)
        result.append({
            'z': z, 'front_half_max_abs_y_change': front_max,
            'rear_half_max_abs_y_change': rear_max,
            'rear_to_front_y_change_ratio': rear_max / front_max if front_max > 1e-8 else None,
            'samples': samples,
        })
    return result


bpy.ops.wm.open_mainfile(filepath=str(TARGET))
current = [bpy.data.objects[name] for name in NAMES]
with bpy.data.libraries.load(str(BASELINE), link=False) as (available, requested):
    assert all(name in available.objects for name in NAMES), 'Baseline objects missing'
    requested.objects = NAMES
previous = requested.objects
assert all(obj is not None for obj in previous), 'Baseline objects failed to load'
comparison = bpy.data.collections.new('Temporary broad-face independent comparison')
bpy.context.scene.collection.children.link(comparison)
for obj in previous:
    comparison.objects.link(obj)
    obj.hide_render = True
bpy.context.scene.frame_set(1)
bpy.context.view_layer.update()

body, old_body = current[0], previous[0]
old, new = coordinates(old_body), coordinates(body)
for a, b in zip(previous[:2], current[:2]):
    before, after = coordinates(a), coordinates(b)
    assert before.shape == after.shape and np.isfinite(after).all()
    assert a.matrix_world == b.matrix_world, ('Body transform changed', b.name)
    assert np.array_equal(before[:, 2], after[:, 2]), ('Height or neck length changed', b.name)
    protected = ((before[:, 2] <= 1.98) | (before[:, 2] >= 2.54)
                 | ((before[:, 2] <= 2.09) & (np.abs(before[:, 0]) >= 0.395)
                    & (before[:, 1] >= -0.12) & (before[:, 1] <= 0.19)))
    assert np.array_equal(before[protected], after[protected]), ('Chest root, shoulders, limbs or crown moved', b.name)
    upper = before[:, 2] >= 2.425
    assert np.array_equal(before[upper, 0], after[upper, 0]), ('Basic upper-head width changed', b.name)

for a, b in zip(previous[:7], current[:7]):
    topology(a, b)
for a, b, name in ((previous[0], current[0], 'NaiwaColor'),
                    (previous[2], current[2], 'EyeColor'),
                    (previous[3], current[3], 'EyeColor')):
    p = attribute(a.data.color_attributes[name].data, 'color', 4, np.float32)
    q = attribute(b.data.color_attributes[name].data, 'color', 4, np.float32)
    assert np.array_equal(p, q), ('Vertex colors changed', b.name)

rig, old_rig = current[-1], previous[-1]
bone_names = set(rig.data.bones.keys())
assert bone_names == set(old_rig.data.bones.keys()) and len(bone_names) == 10, 'Bone set changed'
assert rig.matrix_world == old_rig.matrix_world, 'Rig transform changed'
for name in bone_names:
    assert rig.data.bones[name].matrix_local == old_rig.data.bones[name].matrix_local, ('Rest bone changed', name)
    assert rig.data.bones[name].parent == old_rig.data.bones[name].parent or (
        (rig.data.bones[name].parent.name if rig.data.bones[name].parent else None)
        == (old_rig.data.bones[name].parent.name if old_rig.data.bones[name].parent else None)), ('Bone parent changed', name)
for a, b in zip(previous[:7], current[:7]):
    assert weight_hash(a, bone_names) == weight_hash(b, bone_names), ('Skin weights changed', b.name)
assert {'Naiwa_Idle', 'Naiwa_Walk', 'Naiwa_Laugh', 'Naiwa_Jump'} <= set(bpy.data.actions.keys()), 'Animation action missing'

eyes = {}
for side, a, b in (('L', previous[2], current[2]), ('R', previous[3], current[3])):
    p, q = coordinates(a).astype(float), coordinates(b).astype(float)
    pa, qa = p.mean(axis=0), q.mean(axis=0)
    u, _, vt = np.linalg.svd((p - pa).T @ (q - qa))
    correction = np.eye(3)
    correction[-1, -1] = np.linalg.det(u @ vt)
    rotation = u @ correction @ vt
    error = float(np.max(np.linalg.norm((p - pa) @ rotation + qa - q, axis=1)))
    assert error < 1e-6, ('Eye is not a rigid refit', side, error)
    pupil_before = np.array(a['frontal_pupil_center'])
    pupil_after = np.array(b['frontal_pupil_center'])
    assert np.array_equal(pupil_before[[0, 2]], pupil_after[[0, 2]]), ('Pupil X/Z layout changed', side)
    eyes[side] = {'rigid_fit_error': error, 'pupil_x_z': pupil_after[[0, 2]].tolist()}

old_mouth, mouth = coordinates(previous[6]), coordinates(current[6])
assert np.array_equal(old_mouth[:, [0, 2]], mouth[:, [0, 2]]), 'Mouth X/Z dimensions or placement changed'
old_gap, gap = mouth_gaps(old_body, old_mouth), mouth_gaps(body, mouth)
assert np.max(np.abs(gap - old_gap)) < 1e-6, 'Mouth surface offset changed'
assert np.min(gap) > 0 and np.max(gap) < 0.002, 'Mouth detached from face or embedded'

zs = np.arange(2.0, 2.5301, 0.002)
before_sections = np.array([section(old_body, float(z)) for z in zs])
after_sections = np.array([section(body, float(z)) for z in zs])
monotonic_range = (zs >= 2.01 - 1e-8) & (zs <= 2.52 + 1e-8)
monotonic = {}
for name, column in (('width', 0), ('nape', 2)):
    before_stats = monotonic_stats(before_sections[monotonic_range, column])
    after_stats = monotonic_stats(after_sections[monotonic_range, column])
    monotonic[name] = {'before': before_stats, 'after': after_stats}
    # Submillimeter tolerances absorb mesh/ray interpolation, but prevent an
    # actual waist or a sequence of nape bulges from hiding in small steps.
    assert after_stats['maximum_upward_step'] < 0.00020, ('Section direction reverses', name, after_stats)
    assert after_stats['total_positive_excursion'] < 0.00075, ('Repeated section reversals', name, after_stats)

before_derivatives = profile_derivatives(zs, before_sections[:, 1])
after_derivatives = profile_derivatives(zs, after_sections[:, 1])
lower_face = (zs >= 2.19 - 1e-8) & (zs <= 2.33 + 1e-8)
slope_before = float(np.max(np.abs(before_derivatives[lower_face, 0])))
slope_after = float(np.max(np.abs(after_derivatives[lower_face, 0])))
variation_before = float(np.sqrt(np.mean(before_derivatives[lower_face, 2] ** 2)))
variation_after = float(np.sqrt(np.mean(after_derivatives[lower_face, 2] ** 2)))
# Compare actual geometric smoothness with the preceding mesh. These bounds
# allow sampling uncertainty and do not encode the proposed face dimensions.
assert slope_after <= slope_before * 1.05 + 0.015, ('Lower-face slope became steeper', slope_before, slope_after)
assert variation_after <= variation_before * 1.20 + 5.0, ('Lower-face curvature variation increased', variation_before, variation_after)

integrity = surface_integrity(body)
face_range = (zs >= 2.29) & (zs <= 2.42)
throat_range = (zs >= 2.10) & (zs <= 2.25)
old_face_index = np.flatnonzero(face_range)[np.argmin(before_sections[face_range, 1])]
new_face_index = np.flatnonzero(face_range)[np.argmin(after_sections[face_range, 1])]
old_throat_index = np.flatnonzero(throat_range)[np.argmax(before_sections[throat_range, 1])]
new_throat_index = np.flatnonzero(throat_range)[np.argmax(after_sections[throat_range, 1])]
front_delta = after_sections[:, 1] - before_sections[:, 1]
report = {
    'independent_validation': 'passed',
    'baseline': str(BASELINE), 'target': str(TARGET),
    'all_body_and_shell_heights_exact': True,
    'crown_root_shoulders_limbs_exact': True, 'upper_head_width_exact': True,
    'topology_material_assignments_colors_bones_weights_preserved': True,
    'height_before': float(old[:, 2].max()), 'height_after': float(new[:, 2].max()),
    'changed_body_vertex_count': int(np.count_nonzero(np.any(old != new, axis=1))),
    'maximum_body_displacement': float(np.max(np.linalg.norm(new - old, axis=1))),
    'eye_fitting': eyes,
    'mouth_seam_width_before': float(np.ptp(old_mouth[:, 0])),
    'mouth_seam_width_after': float(np.ptp(mouth[:, 0])),
    'mouth_seam_height_before': float(np.ptp(old_mouth[:, 2])),
    'mouth_seam_height_after': float(np.ptp(mouth[:, 2])),
    'mouth_surface_gap_mean': float(gap.mean()),
    'mouth_surface_gap_max_change': float(np.max(np.abs(gap - old_gap))),
    'sagittal_lower_face': {
        'range_z': [2.19, 2.33], 'sample_spacing': 0.002,
        'local_cubic_window_length': 0.048,
        'maximum_abs_slope_before': slope_before, 'maximum_abs_slope_after': slope_after,
        'maximum_abs_slope_reduction_percent': 100 * (1 - slope_after / slope_before) if slope_before else None,
        'curvature_variation_rms_before': variation_before, 'curvature_variation_rms_after': variation_after,
        'curvature_variation_reduction_percent': 100 * (1 - variation_after / variation_before) if variation_before else None,
        'curvature_peak_to_peak_before': float(np.ptp(before_derivatives[lower_face, 1])),
        'curvature_peak_to_peak_after': float(np.ptp(after_derivatives[lower_face, 1])),
    },
    'volume_redistribution': {
        'maximum_front_retraction': float(np.max(front_delta)),
        'maximum_front_advance': float(-np.min(front_delta)),
        'face_apex_before': {'z': float(zs[old_face_index]), 'y': float(before_sections[old_face_index, 1])},
        'face_apex_after': {'z': float(zs[new_face_index]), 'y': float(after_sections[new_face_index, 1])},
        'apex_retraction': float(after_sections[new_face_index, 1] - before_sections[old_face_index, 1]),
        'throat_before': {'z': float(zs[old_throat_index]), 'y': float(before_sections[old_throat_index, 1])},
        'throat_after': {'z': float(zs[new_throat_index]), 'y': float(after_sections[new_throat_index, 1])},
        'throat_extremum_forward_change': float(before_sections[old_throat_index, 1] - after_sections[new_throat_index, 1]),
        'throat_forward_change_at_previous_height': float(-front_delta[old_throat_index]),
    },
    'section_monotonicity': monotonic,
    'angular_change_diagnostics': angular_diagnostics(old_body, body),
    'surface_integrity': integrity,
    'sections': [{'z': float(z), 'before': a.tolist(), 'after': b.tolist()}
                 for z, a, b in zip(zs[::10], before_sections[::10], after_sections[::10])],
}
(TARGET.parent / 'broad_face_arc_validation.json').write_text(
    json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
summary = {key: value for key, value in report.items()
           if key not in ('sections', 'angular_change_diagnostics')}
print('BROAD_FACE_ARC_INDEPENDENT_VALIDATION_OK', json.dumps(summary, ensure_ascii=False))
