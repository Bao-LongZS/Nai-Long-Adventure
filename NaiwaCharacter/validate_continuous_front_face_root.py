"""Independently validate the saved lip/eye refinement against its baseline.

Run with Blender --python, optionally followed by -- --draft.  All measurements
come from the two saved meshes; no sculpt target functions are imported.
"""
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree


OUT = Path(__file__).resolve().parent
sys.path.insert(0, str(OUT))
from eye_projection_measure import visible_metrics

BASELINE = OUT / 'iterations/before_continuous_front_face/Naiwa_Character.blend'
TARGET = OUT / 'Naiwa_Character.blend'
if '--draft' in sys.argv:
    TARGET = OUT / 'iterations/continuous_front_face_draft/Naiwa_Character.blend'
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
        before = attribute(getattr(a.data, collection), prop, size, np.int32)
        after = attribute(getattr(b.data, collection), prop, size, np.int32)
        assert np.array_equal(before, after), ('Topology/material assignment changed', b.name, collection)
    assert len(a.data.materials) == len(b.data.materials), ('Material slot count changed', b.name)
    assert [modifier.type for modifier in a.modifiers] == [modifier.type for modifier in b.modifiers], (
        'Modifier types changed', b.name)


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
        hit, skin, _, _ = body.ray_cast(
            Vector((float(point[0]), -2, float(point[2]))), Vector((0, 1, 0)))
        assert hit, 'Mouth surface ray missed mesh'
        gaps.append(float(skin.y) - float(point[1]))
    return np.array(gaps)


def surface_integrity(obj, z_range=None, require_outward_x=False):
    xyz = coordinates(obj)
    affected = np.ones(len(xyz), dtype=bool)
    if z_range is not None:
        affected = (xyz[:, 2] >= z_range[0]) & (xyz[:, 2] <= z_range[1])
    points = xyz[affected]
    assert len(points), ('No points in surface-check region', obj.name)
    duplicate_vertices = len(points) - len(np.unique(np.round(points, 7), axis=0))
    faces = [polygon for polygon in obj.data.polygons if z_range is None
             or (min(xyz[i, 2] for i in polygon.vertices) <= z_range[1]
                 and max(xyz[i, 2] for i in polygon.vertices) >= z_range[0])]
    polygons = [tuple(polygon.vertices) for polygon in faces]
    signatures = [tuple(sorted(indices)) for indices in polygons]
    duplicate_faces = len(signatures) - len(set(signatures))
    bvh = BVHTree.FromPolygons([Vector(point) for point in xyz], polygons,
                              all_triangles=False, epsilon=0.0)
    vertex_sets = [set(indices) for indices in polygons]
    crossings = [(a, b) for a, b in bvh.overlap(bvh)
                 if a < b and not vertex_sets[a].intersection(vertex_sets[b])]
    normals = attribute(obj.data.vertices, 'normal', 3, np.float32).reshape((-1, 3))
    result = {
        'duplicate_vertices_at_1e_7': int(duplicate_vertices),
        'duplicate_faces': int(duplicate_faces),
        'degenerate_faces': int(sum(polygon.area < 1e-12 for polygon in faces)),
        'nonadjacent_intersections': len(crossings),
        'normal_unit_error': float(np.max(np.abs(np.linalg.norm(normals[affected], axis=1) - 1))),
        'custom_normals': bool(obj.data.has_custom_normals),
        'sharp_edges': int(sum(edge.use_edge_sharp for edge in obj.data.edges)),
        'all_faces_smooth': bool(all(polygon.use_smooth for polygon in obj.data.polygons)),
    }
    assert duplicate_vertices == duplicate_faces == result['degenerate_faces'] == len(crossings) == 0, (
        'Surface integrity failed', obj.name, result)
    assert result['normal_unit_error'] < 1e-5, ('Nonunit surface normal', obj.name, result)
    assert not result['custom_normals'] and result['sharp_edges'] == 0 and result['all_faces_smooth'], (
        'Surface shading discontinuity', obj.name, result)
    if require_outward_x:
        outward = affected & (np.abs(xyz[:, 0]) > 0.1)
        result['minimum_outward_x'] = float(np.min(normals[outward, 0] * np.sign(xyz[outward, 0])))
        assert result['minimum_outward_x'] > 0, ('Inward body normal', obj.name, result)
    return result


def _center_delta(before, after):
    return float(np.linalg.norm(np.array(after['center']) - np.array(before['center'])))


def validate():
    bpy.ops.wm.open_mainfile(filepath=str(TARGET))
    current = [bpy.data.objects[name] for name in NAMES]
    current_action_names = set(bpy.data.actions.keys())
    with bpy.data.libraries.load(str(BASELINE), link=False) as (available, requested):
        assert all(name in available.objects for name in NAMES), 'Baseline objects missing'
        requested.objects = NAMES
    previous = requested.objects
    assert all(obj is not None for obj in previous), 'Baseline objects failed to load'
    comparison = bpy.data.collections.new('Temporary lip-eye independent comparison')
    bpy.context.scene.collection.children.link(comparison)
    for obj in previous:
        comparison.objects.link(obj)
        obj.hide_render = True
    bpy.context.scene.frame_set(1)
    bpy.context.view_layer.update()
    body, old_body = current[0], previous[0]
    old, new = coordinates(old_body), coordinates(body)
    preservation = {}
    for a, b in zip(previous[:2], current[:2]):
        before, after = coordinates(a), coordinates(b)
        assert before.shape == after.shape and np.isfinite(after).all(), ('Invalid coordinates', b.name)
        assert a.matrix_world == b.matrix_world, ('Body transform changed', b.name)
        assert np.array_equal(before[:, [0, 2]], after[:, [0, 2]]), ('Body X/Z or neck length changed', b.name)
        protected = (before[:, 2] <= 2.20) | (before[:, 2] >= 2.46) | (np.abs(before[:, 0]) >= .19)
        assert np.array_equal(before[protected], after[protected]), ('Head crown, throat, chest or limb moved', b.name)
        heights = np.arange(2.20, 2.46 + 0.00025, 0.0005)
        sections = np.array([section(a, float(z)) for z in heights])
        middle = np.interp(before[:, 2], heights, (sections[:, 1] + sections[:, 2]) / 2)
        posterior = before[:, 1] >= middle
        assert np.array_equal(before[posterior], after[posterior]), ('Posterior half/nape moved', b.name)
        preservation[b.name] = {
            'all_x_z_exact': True, 'fixed_height_and_lateral_bounds_exact': True,
            'posterior_half_exact': True,
            'protected_vertex_count': int(protected.sum()),
            'posterior_vertex_count': int(posterior.sum()),
        }
    for a, b in zip(previous[:7], current[:7]):
        topology(a, b)
        assert np.isfinite(coordinates(b)).all(), ('Nonfinite mesh vertex', b.name)
    for a, b, color_name in ((previous[0], current[0], 'NaiwaColor'),
                              (previous[2], current[2], 'EyeColor'),
                              (previous[3], current[3], 'EyeColor')):
        before = attribute(a.data.color_attributes[color_name].data, 'color', 4, np.float32)
        after = attribute(b.data.color_attributes[color_name].data, 'color', 4, np.float32)
        assert np.array_equal(before, after), ('Per-corner colors changed', b.name)
    rig, old_rig = current[-1], previous[-1]
    bone_names = set(rig.data.bones.keys())
    assert bone_names == set(old_rig.data.bones.keys()) and len(bone_names) == 10, 'Bone set changed'
    assert rig.matrix_world == old_rig.matrix_world, 'Rig transform changed'
    for name in bone_names:
        a, b = old_rig.data.bones[name], rig.data.bones[name]
        assert a.matrix_local == b.matrix_local, ('Rest bone changed', name)
        assert (a.parent.name if a.parent else None) == (b.parent.name if b.parent else None), ('Bone parent changed', name)
    for a, b in zip(previous[:7], current[:7]):
        assert weight_hash(a, bone_names) == weight_hash(b, bone_names), ('Skin weights changed', b.name)
    expected_actions = {'Naiwa_Idle', 'Naiwa_Walk', 'Naiwa_Laugh', 'Naiwa_Jump'}
    assert expected_actions <= current_action_names, 'Animation action missing from saved target'

    eye_report = {}
    for side, a, b in (('L', previous[2], current[2]), ('R', previous[3], current[3])):
        ac,bc=coordinates(a),coordinates(b)
        assert np.array_equal(ac[:,[0,2]],bc[:,[0,2]]),('Eye frontal geometry changed',side)
        assert np.max(np.abs(ac[:,1]-bc[:,1])) < .016,('Eye reattachment too large',side)
        assert np.max(np.abs(ac[ac[:,2]>2.463]-bc[ac[:,2]>2.463])) < 1e-6,('Upper eye moved',side)
        for prop in ('frontal_aperture_center','frontal_eye_axes','frontal_pupil_radius','visible_pupil_to_aperture_area_ratio'):
            assert np.array_equal(np.array(a[prop]),np.array(b[prop])),('Eye property changed',side,prop)
        pupil_before, pupil_after = np.array(a['frontal_pupil_center']), np.array(b['frontal_pupil_center'])
        assert np.array_equal(pupil_before[[0, 2]], pupil_after[[0, 2]]), ('Pupil X/Z layout changed', side)
        before_metrics = visible_metrics(a, old_body, step=0.0006)
        after_metrics = visible_metrics(b, body, step=0.0006)
        changes = {}
        for region in ('aperture', 'green', 'black'):
            p, q = before_metrics[region], after_metrics[region]
            fraction = q['area'] / p['area'] - 1
            center_change = _center_delta(p, q)
            limit = 0.01
            assert abs(fraction) <= limit, ('Projected eye region area changed', side, region, fraction)
            if region in ('aperture', 'green', 'black'):
                assert center_change <= 0.0003, ('Projected eye aperture center moved', side, center_change)
            changes[region] = {'area_fraction_change': float(fraction),
                               'center_displacement': center_change}
        aperture, black = after_metrics['aperture'], after_metrics['black']
        black_anchor_error = float(np.linalg.norm(np.array(black['center']) - pupil_after[[0, 2]]))
        assert black_anchor_error <= 0.001, ('Round pupil no longer matches its layout anchor', side, black_anchor_error)
        assert 0.94 <= aperture['width_height_ratio'] <= 1.02, ('Eye aperture is not round/vertically oval', side, aperture)
        assert aperture['axis_ratio'] <= 1.08, ('Eye aperture still has oblique elongation', side, aperture)
        assert black['axis_ratio'] <= 1.03, ('Pupil is not round', side, black)
        assert 0.97 <= black['width_height_ratio'] <= 1.03, ('Pupil projected bounds are not round', side, black)
        # With a near-circle the PCA angle is unstable. An actual vertical
        # elongation must nevertheless stay upright rather than retain a slant.
        if aperture['axis_ratio'] >= 1.025:
            assert abs(abs(aperture['major_axis_degrees']) - 90) <= 8, ('Vertical eye oval is tilted', side, aperture)
        assert after_metrics['visible_surface_gap']['maximum'] <= 0.025, ('Eye became a raised thick slab', side)
        eye_report[side] = {
            'before': before_metrics, 'after': after_metrics, 'changes': changes,
            'pupil_property_x_z_exact': pupil_after[[0, 2]].tolist(),
            'pupil_visible_center_to_layout_anchor': black_anchor_error,
            'green_and_black_centroid_changes_recorded_without_lock': True,
            'surface_integrity': surface_integrity(b),
        }

    old_mouth, mouth = coordinates(previous[6]), coordinates(current[6])
    assert np.array_equal(old_mouth[:,[0,2]],mouth[:,[0,2]]), 'Mouth frontal geometry changed'
    assert len(mouth) % 2 == 0, 'Expected paired mouth-seam vertices'
    rows = mouth.reshape((-1, 2, 3))
    assert np.all(np.abs(rows[:, 0, 0] - rows[:, 1, 0]) < 1e-7), 'Mouth seam rows are misaligned'
    widths = np.abs(rows[:, 1, 2] - rows[:, 0, 2])
    middle_index = len(rows) // 2
    center_height = float(widths[middle_index])
    assert 0.0001 < center_height <= 0.0022, ('Mouth seam is too thick or vanishes', center_height)
    assert max(float(widths[0]), float(widths[-1])) < center_height * 0.70, 'Mouth endpoints do not taper'
    assert np.max(widths) <= 0.0022, 'Mouth strip has a thick lateral segment'
    centerline = rows.mean(axis=1)
    arc = float((centerline[0, 2] + centerline[-1, 2]) / 2 - centerline[middle_index, 2])
    assert 0.00001 <= arc < 0.004, ('Mouth is not a shallow arc', arc)
    old_width, new_width = float(np.ptp(old_mouth[:, 0])), float(np.ptp(mouth[:, 0]))
    assert old_width * 0.60 <= new_width <= old_width + 0.001, ('Mouth width changed excessively', old_width, new_width)
    assert np.max(np.abs(mouth.mean(axis=0)[[0, 2]] - old_mouth.mean(axis=0)[[0, 2]])) <= 0.002, 'Mouth layout moved'
    gap = mouth_gaps(body, mouth)
    assert float(gap.min()) > 0 and float(gap.max()) < 0.002, 'Mouth is embedded or detached from face'
    assert float(np.ptp(gap)) < 0.0002, 'Mouth offset varies around the seam'

    zs = np.arange(2.10, 2.4901, 0.002)
    before_sections = np.array([section(old_body, float(z)) for z in zs])
    after_sections = np.array([section(body, float(z)) for z in zs])
    front_delta = after_sections[:, 1] - before_sections[:, 1]
    lower_range = (zs >= 2.315 - 1e-8) & (zs <= 2.350 + 1e-8)
    lower_mean = float(front_delta[lower_range].mean())
    lower_max = float(front_delta[lower_range].max())
    assert lower_mean > 0 and lower_max > 0.001, ('Large lower-mouth bulge has not receded', lower_mean, lower_max)
    assert float(front_delta[lower_range].min()) > -0.001, 'Lower chin acquired a new protrusion'
    mouth_z = float(centerline[middle_index, 2])
    old_at_mouth, new_at_mouth = section(old_body, mouth_z), section(body, mouth_z)
    mouth_front_change = float(new_at_mouth[1] - old_at_mouth[1])
    assert -0.005 <= mouth_front_change <= 0.030, ('Mouth front point changed excessively', mouth_front_change)
    near_below = section(body, mouth_z - 0.055)
    lip_projection = float(near_below[1] - new_at_mouth[1])
    assert 0.004 < lip_projection < 0.035, ('Lip/body relation is too flat or too deep', lip_projection)
    throat_z = 2.16
    throat = section(body, throat_z)
    assert np.max(np.abs(throat - section(old_body, throat_z))) < 1e-6, 'Existing throat arc moved'
    assert float(throat[1] - new_at_mouth[1]) > 0.025, 'Mouth no longer leads into receding throat'
    upper = (zs >= mouth_z-.015) & (zs <= 2.435)
    peak_z=float(zs[upper][np.argmin(after_sections[upper,1])])
    assert abs(peak_z-mouth_z)<=.012,('Frontmost point is not near the mouth',peak_z,mouth_z)
    nose=(zs>=2.385)&(zs<=2.42)
    assert np.min(after_sections[nose,1]) > np.min(after_sections[upper,1])+.0001,'Central upper nose remains'
    posterior_error = float(np.max(np.abs(after_sections[:, 2] - before_sections[:, 2])))
    assert posterior_error < 1e-6, ('Nape/back sagittal silhouette moved', posterior_error)
    profile_range = (zs >= 2.182) & (zs <= mouth_z - 0.008)
    anterior_steps = np.diff(after_sections[profile_range, 1])
    assert float(anterior_steps.max()) < 0.0003, 'Lower face has a local reversed section'
    assert float(np.maximum(anterior_steps, 0).sum()) < 0.0008, 'Lower face repeats small bulges'
    integrity = surface_integrity(body, z_range=(1.98, 2.54), require_outward_x=True)
    report = {
        'independent_validation': 'passed', 'baseline': str(BASELINE), 'target': str(TARGET),
        'body_and_shell_preservation': preservation,
        'topology_material_assignments_colors_bones_weights_preserved': True,
        'bone_count': len(bone_names), 'animation_actions_present': sorted(expected_actions),
        'height_before': float(old[:, 2].max()), 'height_after': float(new[:, 2].max()),
        'changed_body_vertex_count': int(np.count_nonzero(np.any(old != new, axis=1))),
        'maximum_body_displacement': float(np.max(np.linalg.norm(new - old, axis=1))),
        'eyes': eye_report,
        'mouth_seam': {
            'width_before': old_width, 'width_after': new_width,
            'central_height': center_height, 'endpoint_heights': [float(widths[0]), float(widths[-1])],
            'shallow_arc_height': arc, 'center_x_z': centerline[middle_index, [0, 2]].tolist(),
            'skin_gap_mean': float(gap.mean()), 'skin_gap_range': [float(gap.min()), float(gap.max())],
            'mouth_projection_change': float(new_at_mouth[1] - old_at_mouth[1]),
            'lip_projection_above_lower_face': lip_projection,
        },
        'lower_face_retraction': {'range_z': [2.315, 2.350],
                                  'mean_retraction': lower_mean, 'maximum_retraction': lower_max},
        'nape_sagittal_max_error': posterior_error,
        'existing_throat_section_exact': True,
        'surface_integrity': integrity,
        'sections': [{'z': float(z), 'before': a.tolist(), 'after': b.tolist()}
                     for z, a, b in zip(zs[::5], before_sections[::5], after_sections[::5])],
    }
    (TARGET.parent / 'continuous_front_face_root_validation.json').write_text(
        json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
    summary = {key: value for key, value in report.items() if key not in ('sections', 'eyes')}
    print('CONTINUOUS_FRONT_FACE_ROOT_VALIDATION_OK', json.dumps(summary, ensure_ascii=False))
    return report


if __name__ == '__main__':
    validate()
