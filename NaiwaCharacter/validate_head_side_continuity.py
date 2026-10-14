"""Validate saved head-side correction against the pinned preceding blend.

Run with Blender --python, optionally followed by -- --target file.blend.
The temporary comparison collection is never saved; only a JSON report is written.
"""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
import numpy as np

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from validate_lip_eye_refinement import (
    attribute, coordinates, topology, weight_hash, surface_integrity,
)
from eye_projection_measure import visible_metrics

BASE = ROOT / 'versions/20261005_head_reshape/Naiwa_Character.blend'
DEFAULT_TARGET = ROOT / 'versions/20261005_head_side_continuity/Naiwa_Character.blend'
NAMES = (
    'Naiwa unified golden body', 'Head and torso silhouette only',
    'L concentric iris and pupil', 'R concentric iris and pupil',
    'L eye glint', 'R eye glint', 'Short closed oral seam', 'Naiwa_Rig',
)
EXPECTED_ACTIONS = {'Naiwa_Idle', 'Naiwa_Walk', 'Naiwa_Laugh', 'Naiwa_Jump'}


def plane_width(obj, z):
    """Measure X extent of exact edge intersections with the eye-height plane."""
    xyz = coordinates(obj).astype(np.float64)
    edges = attribute(obj.data.edges, 'vertices', 2, np.int32).reshape((-1, 2))
    a, b = xyz[edges[:, 0]], xyz[edges[:, 1]]
    dz = b[:, 2] - a[:, 2]
    crossed = (np.minimum(a[:, 2], b[:, 2]) <= z) & (np.maximum(a[:, 2], b[:, 2]) >= z)
    nonflat = crossed & (np.abs(dz) > 1e-12)
    t = (z - a[nonflat, 2]) / dz[nonflat]
    xs = a[nonflat, 0] + t * (b[nonflat, 0] - a[nonflat, 0])
    flat = (np.abs(dz) <= 1e-12) & (np.abs(a[:, 2] - z) <= 1e-12)
    xs = np.concatenate((xs, a[flat, 0], b[flat, 0]))
    assert len(xs) >= 2, ('No eye-height section', obj.name, z)
    return float(xs.max() - xs.min())


def colors_exact(a, b):
    old, new = a.data.color_attributes, b.data.color_attributes
    assert set(old.keys()) == set(new.keys()), ('Color attributes changed', b.name)
    for name in old.keys():
        p, q = old[name], new[name]
        assert (p.domain, p.data_type) == (q.domain, q.data_type), ('Color format changed', b.name, name)
        assert np.array_equal(attribute(p.data, 'color', 4, np.float32),
                              attribute(q.data, 'color', 4, np.float32)), ('Colors changed', b.name, name)
    return list(old.keys())


def scene_structure():
    """Count saved data before importing any baseline object or dependency."""
    rigs = [obj for obj in bpy.data.objects if obj.type == 'ARMATURE']
    bound = [obj for obj in bpy.data.objects if obj.type == 'MESH'
             and any(mod.type == 'ARMATURE' for mod in obj.modifiers)]
    assert len(rigs) == 1 and len(bound) == 6, ('Rig/bound mesh counts changed', len(rigs), len(bound))
    rig = rigs[0]
    for obj in bound:
        mods = [mod for mod in obj.modifiers if mod.type == 'ARMATURE']
        assert len(mods) == 1 and mods[0].object == rig, ('Rig binding changed', obj.name)
    bones, actions = set(rig.data.bones.keys()), set(bpy.data.actions.keys())
    assert len(bones) == 10 and actions == EXPECTED_ACTIONS, ('Bones/actions changed', bones, actions)
    return {'bound_mesh_count': len(bound), 'bone_count': len(bones), 'action_count': len(actions),
            'bound_mesh_names': sorted(obj.name for obj in bound), 'action_names': sorted(actions)}


def validate(target):
    target = target.resolve()
    assert target != BASE.resolve() and target.is_file() and BASE.is_file(), 'Invalid target or missing baseline'
    rec = json.loads((target.parent / 'baseline_dimensions.json').read_text(encoding='utf-8'))
    digest = hashlib.sha256(BASE.read_bytes()).hexdigest()
    assert rec['source_sha256'] == digest, 'Baseline SHA256 disagrees with pinned source'
    D, eye_z, mouth_z = (float(rec[key]) for key in ('D', 'eye_z', 'mouth_z'))
    low, high = (float(rec[key]) for key in ('correction_z_low', 'correction_z_high'))
    assert np.isfinite([D, eye_z, mouth_z, low, high]).all() and D > 0 and low < high, 'Invalid baseline dimensions'
    limit = 0.12 * D
    bpy.ops.wm.open_mainfile(filepath=str(target))
    current = dict((name, bpy.data.objects[name]) for name in NAMES)
    structure = scene_structure()
    with bpy.data.libraries.load(str(BASE), link=False) as (available, loaded):
        assert all(name in available.objects for name in NAMES), 'Missing baseline object'
        loaded.objects = list(NAMES)
    assert all(obj is not None for obj in loaded.objects), 'Baseline object failed to load'
    previous = dict(zip(NAMES, loaded.objects))
    temporary = bpy.data.collections.new('Temporary head-side comparison - never saved')
    bpy.context.scene.collection.children.link(temporary)
    for obj in previous.values():
        temporary.objects.link(obj)
        obj.hide_render = True
    bpy.context.scene.frame_set(1)
    bpy.context.view_layer.update()

    rig, old_rig = current[NAMES[-1]], previous[NAMES[-1]]
    bones = set(rig.data.bones.keys())
    assert bones == set(old_rig.data.bones.keys()) and rig.matrix_world == old_rig.matrix_world, 'Rig changed'
    for name in bones:
        a, b = old_rig.data.bones[name], rig.data.bones[name]
        assert a.matrix_local == b.matrix_local, ('Rest bone changed', name)
        assert (a.parent.name if a.parent else None) == (b.parent.name if b.parent else None), ('Bone parent changed', name)
    meshes, fixed = {}, []
    for index, name in enumerate(NAMES[:-1]):
        a, b = previous[name], current[name]
        topology(a, b)
        old, new = coordinates(a), coordinates(b)
        assert old.shape == new.shape and np.isfinite(new).all(), ('Invalid saved coordinates', name)
        assert a.matrix_world == b.matrix_world, ('Object matrix changed', name)
        assert np.array_equal(old[:, 2], new[:, 2]), ('A Z coordinate changed', name)
        assert np.array_equal(old[:, 1], new[:, 1]), ('A front/back coordinate changed', name)
        groups = set(a.vertex_groups.keys())
        assert groups == set(b.vertex_groups.keys()), ('Vertex-group names changed', name)
        weights = weight_hash(a, groups)
        assert weights == weight_hash(b, groups), ('Vertex weights changed', name)
        color_names = colors_exact(a, b)
        if index >= 2:
            assert np.array_equal(old, new), ('Eye/glint/mouth mesh moved', name)
            fixed.append(name)
            continue
        outside = (old[:, 2] < low) | (old[:, 2] > high)
        assert np.array_equal(old[outside], new[outside]), ('Vertex outside correction band moved', name)
        crown = float(old[:, 2].max())
        protected = (old[:, 2] < 2.17) | (old[:, 2] == crown)
        assert np.array_equal(old[protected], new[protected]), ('Shoulder/crown coordinate changed', name)
        middle = np.abs(old[:, 0]) <= 0.065
        assert np.array_equal(old[middle], new[middle]), ('Middle face/throat strip changed', name)
        transform = np.asarray(a.matrix_world.to_3x3(), dtype=np.float64)
        displacement = (new.astype(np.float64) - old.astype(np.float64)) @ transform.T
        normals = attribute(a.data.vertices, 'normal', 3, np.float32).reshape((-1, 3)).astype(np.float64)
        normals = normals @ np.linalg.inv(transform)
        normals /= np.linalg.norm(normals, axis=1)[:, None]
        maximum = float(np.linalg.norm(displacement, axis=1).max())
        normal_maximum = float(np.abs(np.sum(displacement * normals, axis=1)).max())
        assert maximum <= limit and normal_maximum <= limit, ('Displacement exceeds .12D', name, maximum, normal_maximum, limit)
        before_width, after_width = plane_width(a, eye_z), plane_width(b, eye_z)
        assert abs(after_width - before_width) <= 1e-6, ('Eye-line head width changed', name, before_width, after_width)
        changed = np.any(old != new, axis=1)
        bounds = [old[changed].min(axis=0).tolist(), old[changed].max(axis=0).tolist()] if changed.any() else None
        meshes[name] = {'all_Y_and_Z_exact': True, 'middle_face_throat_strip_exact': True,
                        'outside_correction_band_exact': True,
                        'crown_and_below_2_17_exact': True, 'topology_and_weights_exact': True,
                        'color_attributes_exact': color_names, 'weight_sha256': weights,
                        'changed_vertex_count': int(changed.sum()), 'actual_modified_bounds_xyz': bounds,
                        'maximum_euclidean_displacement': maximum, 'maximum_old_normal_projection': normal_maximum,
                        'eye_line_width_before': before_width, 'eye_line_width_after': after_width,
                        'surface_integrity': surface_integrity(b, z_range=(2.15, crown)) if index == 0 else {'unchanged_shell': True}}
    assert meshes[NAMES[0]]['color_attributes_exact'], 'Body vertex colors are missing'
    eyes = {}
    for name in NAMES[2:4]:
        before = visible_metrics(previous[name], previous[NAMES[0]], step=0.0003)
        after = visible_metrics(current[name], current[NAMES[0]], step=0.0003)
        for region in ('aperture', 'green', 'black'):
            assert before[region] == after[region], ('Visible eye projection changed', name, region)
        eyes[name] = {'aperture_green_pupil_projection_exact': True,
                      'visible_outer_equivalent_diameter_before': math.sqrt(4 * before['aperture']['area'] / math.pi),
                      'visible_outer_equivalent_diameter_after': math.sqrt(4 * after['aperture']['area'] / math.pi),
                      'sampling_step': 0.0003}
    report = {'independent_validation': 'passed', 'baseline': str(BASE), 'target': str(target),
              'baseline_source_sha256': digest, 'D': D, 'eye_z': eye_z, 'mouth_z': mouth_z,
              'correction_z_range': [low, high], 'maximum_allowed_displacement': limit,
              'saved_target_structure_before_baseline_load': structure, 'meshes': meshes,
              'visible_eyes': eyes,
              'eyes_glints_mouth_all_coordinates_and_matrices_exact': fixed,
              'all_mesh_topology_colors_weights_and_Y_Z_exact': True, 'rig_rest_bones_and_parents_exact': True}
    output = target.parent / 'head_side_continuity_validation.json'
    output.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
    print('HEAD_SIDE_CONTINUITY_VALIDATION_OK', json.dumps({'report': str(output), 'meshes': meshes}, ensure_ascii=False))
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', type=Path, default=DEFAULT_TARGET)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    validate(args.target)
