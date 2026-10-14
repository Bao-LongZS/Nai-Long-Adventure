"""Read-only local validation of the run copy and its two FBX exports."""
import hashlib
import json
from pathlib import Path

import bpy
import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SOURCE = ROOT / 'NaiwaCharacter/versions/20261007_H2_E2_overall_refinement/candidate_E2/Naiwa_Character.blend'


def mesh_hashes():
    record = {}
    for obj in bpy.data.objects:
        if obj.type != 'MESH' or not any(m.type == 'ARMATURE' for m in obj.modifiers):
            continue
        vertices = np.empty(len(obj.data.vertices)*3, dtype=np.float32)
        obj.data.vertices.foreach_get('co', vertices)
        record[obj.name] = {'vertices': len(obj.data.vertices), 'polygons': len(obj.data.polygons),
                            'coordinates_sha256': hashlib.sha256(vertices.tobytes()).hexdigest()}
    return record


def curves(action):
    result = []
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    result.append({'path': curve.data_path, 'index': curve.array_index,
                                   'keys': [[list(k.co), k.interpolation] for k in curve.keyframe_points]})
    return result


def snapshot():
    rig = bpy.data.objects['Naiwa_Rig']
    return {'meshes': mesh_hashes(),
            'bones': {b.name: {'parent': b.parent.name if b.parent else None,
                              'rest': [list(row) for row in b.matrix_local]} for b in rig.data.bones},
            'original_actions': {a.name: curves(a) for a in bpy.data.actions if a.name != 'Naiwa_Run' and not a.name.startswith('Key')},
            'rotation_modes': {p.name: p.rotation_mode for p in rig.pose.bones}}


def parse_fbx(path):
    from io_scene_fbx import parse_fbx as parse
    data, version = parse.parse(str(path))
    objects = next(e for e in data.elems if e.id == b'Objects')
    stacks, shapes, channels, curve_nodes = [], [], [], []
    for obj in objects.elems:
        if obj.id == b'AnimationStack':
            stacks.append(repr(obj.props[1]))
        elif obj.id == b'Geometry' and obj.props[2] == b'Shape':
            count = next((len(c.props[0]) for c in obj.elems if c.id == b'Indexes'), None)
            shapes.append({'name': repr(obj.props[1]), 'changed_vertex_count': count})
        elif obj.id == b'Deformer' and obj.props[2] == b'BlendShapeChannel':
            channels.append(repr(obj.props[1]))
        elif obj.id == b'AnimationCurveNode' and b'DeformPercent' in obj.props[1]:
            curve_nodes.append(repr(obj.props[1]))
    return {'fbx_version': version, 'animation_stacks': stacks, 'shapes': shapes,
            'blendshape_channels': channels, 'morph_animation_curve_nodes': curve_nodes}


bpy.ops.wm.open_mainfile(filepath=str(SOURCE), load_ui=False)
baseline = snapshot()
bpy.ops.wm.open_mainfile(filepath=str(HERE / 'Naiwa_E2_Run.blend'), load_ui=False)
candidate = snapshot()
report = {'rest_mesh_coordinate_hashes_equal': baseline['meshes'] == candidate['meshes'],
          'rest_bone_matrices_parents_equal': baseline['bones'] == candidate['bones'],
          'original_four_actions_equal': all(candidate['original_actions'].get(name) == value for name, value in baseline['original_actions'].items()),
          'original_four_rotation_modes_equal': baseline['rotation_modes'] == candidate['rotation_modes'],
          'source_meshes': baseline['meshes'], 'original_action_names': sorted(baseline['original_actions']),
          'candidate_extra_actions': sorted(set(candidate['original_actions']) - set(baseline['original_actions'])),
          'animation_only_fbx': parse_fbx(HERE / 'Naiwa_Run.fbx'),
          'mesh_morph_fbx': parse_fbx(HERE / 'Naiwa_Elastic_Run.fbx')}
samples = json.loads((HERE / 'run_parameters.json').read_text(encoding='utf-8'))['samples']
velocities = []
for before, after in zip(samples, samples[1:]):
    if before['support'] and before['support'] == after['support']:
        side = before['support']
        dt = (after['frame'] - before['frame']) / 24
        velocities.append((after['feet'][side]['contact_y_m'] - before['feet'][side]['contact_y_m']) / dt * 1.8/2.636830637455*100)
report['support_foot_velocity_game_cm_s'] = {'min': min(velocities), 'max': max(velocities), 'mean': sum(velocities)/len(velocities),
                                          'intended_capsule_speed': 300, 'samples': len(velocities),
                                          'method': 'Lowest 5 mm sole vertices mean Y across 96 Hz source poses during same-foot support.'}
assert report['rest_mesh_coordinate_hashes_equal']
assert report['rest_bone_matrices_parents_equal']
assert report['original_four_actions_equal']
assert report['original_four_rotation_modes_equal']
assert len(report['animation_only_fbx']['animation_stacks']) == 1
assert len(report['mesh_morph_fbx']['animation_stacks']) == 1
assert len(report['mesh_morph_fbx']['shapes']) == 3
(HERE / 'local_validation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print('NAIWA_RUN_LOCAL_VALIDATION_OK', json.dumps(report, ensure_ascii=False), flush=True)
