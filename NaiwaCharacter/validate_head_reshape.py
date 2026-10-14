"""Independently validate the saved head revision against its pinned delivery.

Run with Blender --python, optionally followed by -- --target path/to/file.blend.
Only saved geometry supplies the measurements.  No sculpt target or deformation
functions are imported, and the temporary comparison objects are never saved.
"""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector


ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from eye_projection_measure import visible_metrics
from validate_lip_eye_refinement import (
    attribute, coordinates, topology, weight_hash, surface_integrity,
)

BASE = ROOT / 'versions/20261005_eye_muzzle_body_delivery/Naiwa_Character.blend'
DEFAULT_TARGET = ROOT / 'versions/20261005_head_reshape/Naiwa_Character.blend'
NAMES = (
    'Naiwa unified golden body', 'Head and torso silhouette only',
    'L concentric iris and pupil', 'R concentric iris and pupil',
    'L eye glint', 'R eye glint', 'Short closed oral seam', 'Naiwa_Rig',
)
EXPECTED_ACTIONS = {'Naiwa_Idle', 'Naiwa_Walk', 'Naiwa_Laugh', 'Naiwa_Jump'}
SAMPLE_STEP = 0.0003
DIAMETER_FRACTION_TOLERANCE = 0.01
WIDTH_TOLERANCE = 0.00003
GAP_TOLERANCE = 0.0004
COORDINATE_TOLERANCE = 0.000001


def plane_width(obj, z):
    """Intersect actual mesh edges with a horizontal plane, including flat edges."""
    points = coordinates(obj).astype(np.float64)
    edges = attribute(obj.data.edges, 'vertices', 2, np.int32).reshape((-1, 2))
    a, b = points[edges[:, 0]], points[edges[:, 1]]
    dz = b[:, 2] - a[:, 2]
    crossed = ((a[:, 2] <= z) & (b[:, 2] >= z)) | ((b[:, 2] <= z) & (a[:, 2] >= z))
    nonflat = crossed & (np.abs(dz) > 1e-12)
    t = (z - a[nonflat, 2]) / dz[nonflat]
    xs = a[nonflat, 0] + t * (b[nonflat, 0] - a[nonflat, 0])
    flat = (np.abs(dz) <= 1e-12) & (np.abs(a[:, 2] - z) <= 1e-12)
    if np.any(flat):
        xs = np.concatenate((xs, a[flat, 0], b[flat, 0]))
    assert len(xs) >= 2, ('No usable edge-plane intersection', obj.name, z)
    low, high = float(xs.min()), float(xs.max())
    return {'z': float(z), 'width': high - low, 'minimum_x': low,
            'maximum_x': high, 'midline_x': (high + low) / 2,
            'edge_intersection_count': int(len(xs))}


def sagittal_section(obj, z, x=0.0):
    """Measure central front/rear Y from two independent mesh rays."""
    front_hit, front, _, _ = obj.ray_cast(Vector((float(x), -2, float(z))), Vector((0, 1, 0)))
    rear_hit, rear, _, _ = obj.ray_cast(Vector((float(x), 2, float(z))), Vector((0, -1, 0)))
    assert front_hit and rear_hit, ('Sagittal ray missed body', obj.name, z)
    return np.array((front.y, rear.y), dtype=np.float64)


def equivalent_diameter(region):
    return math.sqrt(4 * region['area'] / math.pi)


def geometry_center_xz(obj):
    """Measure the complete symmetric eye volume's center from its X/Z bounds."""
    points = coordinates(obj).astype(np.float64)
    world = np.asarray([tuple(obj.matrix_world @ Vector(point)) for point in points])
    return (world[:, [0, 2]].min(axis=0) + world[:, [0, 2]].max(axis=0)) / 2


def mouth_dimensions(obj):
    points = coordinates(obj).astype(np.float64)
    low, high = float(points[:, 0].min()), float(points[:, 0].max())
    middle_x = (low + high) / 2
    central = points[np.abs(points[:, 0] - middle_x) <= 1e-7]
    assert len(central) >= 2, ('No central mouth section vertices', obj.name)
    bottom, top = float(central[:, 2].min()), float(central[:, 2].max())
    return {'width': high - low, 'central_opening': top - bottom,
            'center_x': middle_x, 'center_z': (bottom + top) / 2,
            'central_vertex_count': int(len(central))}


def compare_colors(before, after):
    old_attributes, new_attributes = before.data.color_attributes, after.data.color_attributes
    assert set(old_attributes.keys()) == set(new_attributes.keys()), 'Body color-attribute set changed'
    result = {}
    for name in old_attributes.keys():
        old_attr, new_attr = old_attributes[name], new_attributes[name]
        assert (old_attr.domain, old_attr.data_type) == (new_attr.domain, new_attr.data_type), (
            'Body color-attribute format changed', name)
        a = attribute(old_attr.data, 'color', 4, np.float32)
        b = attribute(new_attr.data, 'color', 4, np.float32)
        assert np.array_equal(a, b), ('Body vertex/corner colors changed', name)
        result[name] = {'domain': new_attr.domain, 'data_type': new_attr.data_type,
                        'element_count': len(new_attr.data), 'all_values_exact': True}
    assert result, 'Body has no preserved vertex colors'
    return result


def scene_structure():
    """Capture saved-target structure before a library load adds dependencies."""
    rigs = [obj for obj in bpy.data.objects if obj.type == 'ARMATURE']
    bound = [obj for obj in bpy.data.objects if obj.type == 'MESH'
             and any(mod.type == 'ARMATURE' for mod in obj.modifiers)]
    assert len(rigs) == 1, ('Saved target must contain exactly one rig', len(rigs))
    rig = rigs[0]
    assert len(bound) == 6, ('Bound mesh count changed', len(bound))
    for obj in bound:
        modifiers = [mod for mod in obj.modifiers if mod.type == 'ARMATURE']
        assert len(modifiers) == 1 and modifiers[0].object == rig, ('Invalid mesh rig binding', obj.name)
    bones = set(rig.data.bones.keys())
    actions = set(bpy.data.actions.keys())
    assert len(bones) == 10, ('Bone count changed', len(bones))
    assert actions == EXPECTED_ACTIONS, ('Saved animation action set changed', sorted(actions))
    return {'armature_count': len(rigs), 'bound_mesh_count': len(bound),
            'bound_mesh_names': sorted(obj.name for obj in bound),
            'bone_count': len(bones), 'bone_names': sorted(bones),
            'action_count': len(actions), 'action_names': sorted(actions)}


def validate(target):
    target = target.resolve()
    assert target != BASE.resolve(), 'Choose the new head model as the target'
    assert target.is_file() and BASE.is_file(), 'Target or pinned baseline blend is missing'
    record_path = target.parent / 'baseline_dimensions.json'
    build_path = target.parent / 'head_reshape_build_report.json'
    rec = json.loads(record_path.read_text(encoding='utf-8'))
    build = json.loads(build_path.read_text(encoding='utf-8'))
    baseline_digest = hashlib.sha256(BASE.read_bytes()).hexdigest()
    assert rec['source_sha256'] == baseline_digest, 'Pinned original blend no longer matches its baseline record'
    assert build['baseline']['source_sha256'] == baseline_digest, 'Build report refers to a different original'
    assert build['status'] == 'saved', 'Build report does not describe a saved result'

    bpy.ops.wm.open_mainfile(filepath=str(target))
    current = {name: bpy.data.objects[name] for name in NAMES}
    new_scene = scene_structure()
    # Blender replaces entries in loaded.objects with object references.  Pass
    # a fresh list so NAMES remains a stable sequence of original names.
    with bpy.data.libraries.load(str(BASE), link=False) as (available, loaded):
        assert all(name in available.objects for name in NAMES), 'Pinned baseline objects are missing'
        loaded.objects = list(NAMES)
    assert all(obj is not None for obj in loaded.objects), 'Baseline objects failed to load'
    previous = dict(zip(NAMES, loaded.objects))
    temporary = bpy.data.collections.new('Temporary independent head comparison - never saved')
    bpy.context.scene.collection.children.link(temporary)
    for obj in previous.values():
        temporary.objects.link(obj)
        obj.hide_render = True
    bpy.context.scene.frame_set(1)
    bpy.context.view_layer.update()

    body = current[NAMES[0]]
    old_body = previous[NAMES[0]]
    old, new = coordinates(old_body), coordinates(body)
    topology(old_body, body)
    assert old.shape == new.shape and np.isfinite(new).all(), 'Body coordinates are invalid'
    assert body.matrix_world == old_body.matrix_world, 'Body transform changed'
    assert np.array_equal(old[:, 2], new[:, 2]), 'A body Z coordinate changed'
    protected = old[:, 2] <= 2.17
    assert np.array_equal(old[protected], new[protected]), 'A body vertex at or below Z=2.17 moved'
    crown = float(old[:, 2].max())
    assert float(new[:, 2].max()) == crown, 'Crown height changed'
    colors = compare_colors(old_body, body)

    rig, old_rig = current[NAMES[-1]], previous[NAMES[-1]]
    bones = set(rig.data.bones.keys())
    assert bones == set(old_rig.data.bones.keys()), 'Bone names changed'
    assert rig.matrix_world == old_rig.matrix_world, 'Rig transform changed'
    for name in bones:
        a, b = old_rig.data.bones[name], rig.data.bones[name]
        assert a.matrix_local == b.matrix_local, ('Rest bone changed', name)
        assert (a.parent.name if a.parent else None) == (b.parent.name if b.parent else None), (
            'Bone parent changed', name)
    bindings = {}
    for name in new_scene['bound_mesh_names']:
        assert name in previous and name in current, ('Unexpected bound mesh', name)
        a, b = previous[name], current[name]
        topology(a, b)
        before_hash, after_hash = weight_hash(a, bones), weight_hash(b, bones)
        assert before_hash == after_hash, ('Skin weights changed', name)
        bindings[name] = {'weight_sha256': after_hash, 'topology_preserved': True}

    baseline_eyes = {}
    centers = {}
    for side in ('L', 'R'):
        name = side + ' concentric iris and pupil'
        old_eye = previous[name]
        metric = visible_metrics(old_eye, old_body, step=SAMPLE_STEP)
        centers[side] = geometry_center_xz(old_eye)
        baseline_eyes[side] = {'diameter': equivalent_diameter(metric['aperture']),
                               'geometry_center_xz': centers[side].tolist(), 'visible_metrics': metric}
    D0 = float(np.mean([entry['diameter'] for entry in baseline_eyes.values()]))
    D1 = 1.15 * D0
    eye_z = float(np.mean([center[1] for center in centers.values()]))
    width_before = plane_width(old_body, eye_z)
    W0, midline = width_before['width'], width_before['midline_x']
    old_mouth = mouth_dimensions(previous['Short closed oral seam'])
    assert abs(D0 - float(rec['D0'])) <= 1e-9, ('Recorded D0 disagrees with original saved geometry', D0, rec['D0'])
    assert abs(W0 - float(rec['W0'])) <= WIDTH_TOLERANCE, ('Recorded W0 disagrees with original saved geometry', W0, rec['W0'])
    assert abs(eye_z - float(rec['eye_z'])) <= COORDINATE_TOLERANCE, 'Recorded original eye height disagrees with mesh'
    assert abs(old_mouth['center_z'] - float(rec['mouth_z'])) <= COORDINATE_TOLERANCE, 'Recorded original mouth height disagrees with mesh'
    assert crown == float(rec['crown_z']), 'Recorded original crown height disagrees with mesh'

    # Check both unchanged posterior center vertices and central rear rays.
    # The ray samples cover the head/neck profile, excluding only its zero-area apex.
    heights = np.linspace(2.17, crown - 0.0003, 301)
    old_sections = np.asarray([sagittal_section(old_body, float(z), midline) for z in heights])
    new_sections = np.asarray([sagittal_section(body, float(z), midline) for z in heights])
    assert np.array_equal(old_sections[:, 1], new_sections[:, 1]), 'Posterior central sagittal Y changed'
    middle_y = np.interp(old[:, 2], heights, old_sections.mean(axis=1))
    posterior_center = ((old[:, 2] >= 2.17) & (np.abs(old[:, 0] - midline) <= 1e-6)
                        & (old[:, 1] >= middle_y))
    assert np.any(posterior_center), 'No posterior central vertices were found'
    assert np.array_equal(old[posterior_center, 1], new[posterior_center, 1]), 'Posterior central vertex Y changed'

    width_after = plane_width(body, eye_z)
    width_error = width_after['width'] - 1.10 * W0
    assert abs(width_error) <= WIDTH_TOLERANCE, ('Actual eye-line yellow head width is not 1.10 W0', width_after, W0)
    mouth_line_before = plane_width(old_body, old_mouth['center_z'])
    mouth_line_after = plane_width(body, old_mouth['center_z'])
    mouth_line_expansion = mouth_line_after['width'] / mouth_line_before['width'] - 1
    assert mouth_line_expansion <= 0.02 + 1e-7, ('Head expansion at mouth line exceeds 2%', mouth_line_expansion)

    eyes = {}
    visible_centers = {}
    geometry_centers = {}
    for side, direction in (('L', -1), ('R', 1)):
        eye = current[side + ' concentric iris and pupil']
        metric = visible_metrics(eye, body, step=SAMPLE_STEP)
        outer = equivalent_diameter(metric['aperture'])
        pupil = equivalent_diameter(metric['black'])
        outer_error = outer / D1 - 1
        pupil_error = pupil / (0.53 * D1) - 1
        pupil_ratio_error = pupil / (0.53 * outer) - 1
        assert abs(outer_error) <= DIAMETER_FRACTION_TOLERANCE, ('Actual outer eye diameter is not 1.15 D0', side, outer, D1)
        assert abs(pupil_error) <= DIAMETER_FRACTION_TOLERANCE, ('Actual black pupil diameter is not .53 D1', side, pupil, D1)
        assert abs(pupil_ratio_error) <= DIAMETER_FRACTION_TOLERANCE, ('Actual pupil/outer-diameter ratio is not .53', side)
        center = geometry_center_xz(eye)
        expected_center = np.array((midline + direction * D1, centers[side][1]))
        assert np.max(np.abs(center - expected_center)) <= COORDINATE_TOLERANCE, (
            'Saved eye volume center must be at X=midline +/- D1 and original Z', side, center.tolist(), expected_center.tolist())
        visible_center = np.array(metric['aperture']['center'])
        assert np.max(np.abs(visible_center - expected_center)) <= SAMPLE_STEP, (
            'Visible eye center disagrees with required layout', side, visible_center.tolist())
        assert abs(visible_center[1] - baseline_eyes[side]['visible_metrics']['aperture']['center'][1]) <= SAMPLE_STEP, (
            'Visible eye-center Z changed', side)
        maximum_gap = metric['visible_surface_gap']['maximum']
        assert abs(maximum_gap - 0.20 * D1) <= GAP_TOLERANCE, (
            'Measured maximum visible eye/skin gap is not .20 D1', side, maximum_gap)
        visible_centers[side], geometry_centers[side] = visible_center, center
        eyes[side] = {'actual_outer_diameter': outer, 'actual_black_pupil_diameter': pupil,
                      'actual_pupil_to_outer_diameter': pupil / outer,
                      'outer_diameter_fraction_error': outer_error, 'pupil_diameter_fraction_error': pupil_error,
                      'geometry_center_xz': center.tolist(), 'expected_center_xz': expected_center.tolist(),
                      'actual_maximum_visible_surface_gap': maximum_gap,
                      'gap_error': maximum_gap - 0.20 * D1, 'visible_metrics': metric,
                      'surface_integrity': surface_integrity(eye)}
    center_distance = float(np.linalg.norm(geometry_centers['R'] - geometry_centers['L']))
    horizontal_distance = float(geometry_centers['R'][0] - geometry_centers['L'][0])
    visible_distance = float(np.linalg.norm(visible_centers['R'] - visible_centers['L']))
    assert abs(center_distance - 2 * D1) <= 2 * COORDINATE_TOLERANCE, ('Eye-center distance is not 2 D1', center_distance)
    assert abs(horizontal_distance - 2 * D1) <= 2 * COORDINATE_TOLERANCE, 'Horizontal eye-center distance is not 2 D1'
    assert abs(visible_distance - 2 * D1) <= 2 * SAMPLE_STEP, 'Visible eye-center distance is not 2 D1'

    mouth = current['Short closed oral seam']
    mouth_actual = mouth_dimensions(mouth)
    assert abs(mouth_actual['width'] - 1.25 * D1) <= COORDINATE_TOLERANCE, ('Actual mouth width is not 1.25 D1', mouth_actual)
    assert abs(mouth_actual['central_opening'] - 0.06 * D1) <= COORDINATE_TOLERANCE, ('Actual central mouth opening is not .06 D1', mouth_actual)
    assert abs(mouth_actual['center_x'] - midline) <= COORDINATE_TOLERANCE, 'Mouth is off the head midline'
    assert abs(mouth_actual['center_z'] - old_mouth['center_z']) <= COORDINATE_TOLERANCE, 'Mouth center Z changed'
    mouth_actual['surface_integrity'] = surface_integrity(mouth)

    report = {
        'independent_validation': 'passed', 'baseline': str(BASE), 'target': str(target),
        'baseline_source_sha256': baseline_digest, 'baseline_record': str(record_path),
        'build_report': str(build_path), 'measurements_from_saved_geometry': True,
        'sculpt_target_functions_imported': False, 'saved_target_structure_before_baseline_load': new_scene,
        'tolerances': {'diameter_relative_sampling': DIAMETER_FRACTION_TOLERANCE,
                       'eye_line_width_absolute': WIDTH_TOLERANCE, 'eye_gap_absolute': GAP_TOLERANCE,
                       'mesh_coordinate_absolute': COORDINATE_TOLERANCE, 'visible_center_absolute': SAMPLE_STEP},
        'independently_measured_baseline': {'D0': D0, 'W0': W0, 'D1_from_1_15_D0': D1,
                                           'eye_z': eye_z, 'midline_x': midline, 'crown_z': crown,
                                           'mouth': old_mouth, 'eyes': baseline_eyes},
        'body_preservation': {'topology_preserved': True, 'all_Z_exact': True,
                              'body_at_or_below_z_2_17_exact': True,
                              'protected_vertex_count': int(protected.sum()),
                              'posterior_central_vertex_Y_exact': True,
                              'posterior_central_vertex_count': int(posterior_center.sum()),
                              'posterior_central_sagittal_Y_exact': True,
                              'posterior_sagittal_sample_count': len(heights), 'crown_height_exact': True,
                              'body_vertex_colors': colors,
                              'changed_body_vertex_count': int(np.count_nonzero(np.any(old != new, axis=1)))},
        'eye_line_yellow_head_width': {'before': width_before, 'after': width_after,
                                      'required': 1.10 * W0, 'absolute_error': width_error},
        'mouth_line_yellow_head_width': {'before': mouth_line_before, 'after': mouth_line_after,
                                        'expansion_fraction': mouth_line_expansion, 'maximum_allowed': 0.02},
        'rig_bones_rest_matrices_parents_and_weights_preserved': True, 'bindings': bindings,
        'eyes': eyes, 'eye_center_distance': center_distance,
        'horizontal_eye_center_distance': horizontal_distance, 'visible_eye_center_distance': visible_distance,
        'mouth': mouth_actual,
        'body_surface_integrity': surface_integrity(body, z_range=(1.63, crown)),
        'posterior_central_sections': [{'z': float(z), 'before_rear_y': float(a[1]), 'after_rear_y': float(b[1])}
                                       for z, a, b in zip(heights, old_sections, new_sections)],
    }
    # Write only the validation report.  In particular, never save the scene
    # containing the temporary baseline objects or create intermediate blends.
    report_path = target.parent / 'head_reshape_validation.json'
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print('HEAD_RESHAPE_INDEPENDENT_VALIDATION_OK', json.dumps({
        'target': str(target), 'D0': D0, 'D1': D1, 'W0': W0,
        'eye_line_width': width_after['width'], 'eye_center_distance': center_distance,
        'mouth_width': mouth_actual['width'], 'mouth_opening': mouth_actual['central_opening'],
        'bound_mesh_count': new_scene['bound_mesh_count'], 'bone_count': new_scene['bone_count'],
        'action_count': new_scene['action_count'], 'report': str(report_path),
    }, ensure_ascii=False))
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', type=Path, default=DEFAULT_TARGET)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    validate(args.target)
