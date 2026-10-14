"""Prepare one frozen joint C3 direction, then save A or B from the pinned E2.

Preparation uses cached baseline data and the actual protected curve operator. Generation opens E2, changes BODY
coordinates only, and saves a new project. B requires a recorded A gate.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import numpy as np

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from c3_joint_curve_solver import prepare

BODY = 'Naiwa unified golden body'
SOURCE = ROOT / 'versions/20261007_H2_E2_overall_refinement/candidate_E2/Naiwa_Character.blend'
SOURCE_SHA = '317f62278d63217636bf393f831e2fb41ce21acfefbe21beed6ad6d4dc75f322'


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def dump(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2,
                               allow_nan=False) + '\n', encoding='utf-8')


def load_protocol(root):
    protocol = json.loads((root / 'measurement_protocol.json').read_text(encoding='utf-8'))
    snapshot = json.loads((root / 'baseline/snapshot.json').read_text(encoding='utf-8'))
    assert sha(SOURCE) == SOURCE_SHA == snapshot['source_sha256']
    assert snapshot['measurement_protocol_sha256'] == sha(root / 'measurement_protocol.json')
    assert snapshot['array_file_sha256'] == sha(root / 'baseline/baseline_arrays.npz')
    return protocol, snapshot


def prepare_direction(root):
    protocol, snapshot = load_protocol(root)
    output = root / 'c3_field.npz'
    if output.exists():
        raise RuntimeError('Frozen joint direction already exists; do not silently refit')
    field, report = prepare(root)
    direction = field['vertex_direction_world']
    frozen = field['frozen_mask']
    eye = field['eye_local_mask']
    assert direction.ndim == 2 and direction.shape[1] == 3
    assert np.isfinite(direction).all()
    assert not np.any(direction[frozen])
    assert not np.any(direction[eye]), 'Eye-local skin is preserved by this solver'
    norm = np.linalg.norm(direction, axis=1)
    assert norm.max() > 1e-8
    scales = report['scales']
    assert scales['B'] > scales['A'] * 1.01
    for label in ('A', 'B'):
        assert norm.max() * scales[label] <= protocol['budgets'][label]
        assert norm[eye].max() * scales[label] <= protocol['budgets']['eye_local']
    np.savez_compressed(output, **field)
    report.update({'source_sha256': SOURCE_SHA,
                   'measurement_protocol_sha256': sha(root / 'measurement_protocol.json'),
                   'baseline_arrays_sha256': sha(root / 'baseline/baseline_arrays.npz'),
                   'field_sha256': sha(output), 'D': protocol['scale']['D'],
                   'unscaled_maximum_vertex_displacement': float(norm.max()),
                   'unscaled_maximum_eye_local_displacement': float(norm[eye].max()),
                   'field_script_sha256': sha(ROOT / 'c3_joint_curve_solver.py'),
                   'driver_sha256': sha(Path(__file__))})
    dump(root / 'c3_field_report.json', report)
    print('C3_JOINT_DIRECTION_PREPARED', json.dumps({k: report[k] for k in
        ('unscaled_maximum_vertex_displacement', 'scales', 'D')}, ensure_ascii=False), flush=True)


def generate(root, label):
    import bpy
    protocol, snapshot = load_protocol(root)
    report = json.loads((root / 'c3_field_report.json').read_text(encoding='utf-8'))
    assert report['field_sha256'] == sha(root / 'c3_field.npz')
    assert report['measurement_protocol_sha256'] == sha(root / 'measurement_protocol.json')
    assert report['baseline_arrays_sha256'] == sha(root / 'baseline/baseline_arrays.npz')
    assert report['field_script_sha256'] == sha(ROOT / 'c3_joint_curve_solver.py')
    assert report['driver_sha256'] == sha(Path(__file__))
    if label == 'B':
        gate = json.loads((root / 'candidate_A/key_view_gate.json').read_text(encoding='utf-8'))
        assert gate['decision'] == 'direction_correct_amplitude_insufficient'
        assert gate['same_target_and_scope_for_B'] and not gate['new_defects']
        assert report['scales']['B'] > report['scales']['A'] * 1.01, 'Eye preservation prevents an increased B amplitude'
    output = root / ('candidate_' + label)
    destination = output / 'Naiwa_Character.blend'
    if destination.exists():
        raise RuntimeError('Candidate already exists; do not overwrite a saved trial')
    with np.load(root / 'baseline/baseline_arrays.npz') as data:
        prefix = snapshot['objects'][BODY]['array_prefix']
        old = data[prefix + '_raw_local'].copy()
        world = data[prefix + '_raw_world'].copy()
    with np.load(root / 'c3_field.npz') as data:
        delta = data['vertex_direction_world'] * report['scales'][label]
        frozen = data['frozen_mask'].copy()
        eye = data['eye_local_mask'].copy()
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE), load_ui=False)
    obj = bpy.data.objects[BODY]
    local = np.empty(len(obj.data.vertices) * 3, np.float32)
    obj.data.vertices.foreach_get('co', local)
    local = local.reshape(-1, 3)
    assert local.dtype == old.dtype and local.tobytes() == old.tobytes()
    matrix = np.asarray(obj.matrix_world, float)
    assert matrix.tolist() == snapshot['objects'][BODY]['transform']
    inverse = np.linalg.inv(matrix)
    wanted_world = world + delta
    new = (wanted_world @ inverse[:3, :3].T + inverse[:3, 3]).astype(np.float32)
    new[frozen] = old[frozen]
    new[eye] = old[eye]
    actual_world = new.astype(float) @ matrix[:3, :3].T + matrix[:3, 3]
    actual_delta = actual_world - world
    norm = np.linalg.norm(actual_delta, axis=1)
    assert norm.max() <= protocol['budgets'][label]
    assert norm[eye].max() <= protocol['budgets']['eye_local']
    assert new[frozen].tobytes() == old[frozen].tobytes()
    obj.data.vertices.foreach_set('co', new.ravel())
    obj.data.update()
    bpy.context.view_layer.update()
    output.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(destination), check_existing=False)
    assert sha(SOURCE) == SOURCE_SHA
    np.savez_compressed(output / 'generation_actual_delta.npz', actual_delta_world=actual_delta)
    dump(output / 'build_report.json', {
        'label': label, 'fixed_source': str(SOURCE), 'source_sha256': SOURCE_SHA,
        'candidate_sha256': sha(destination), 'field_sha256': report['field_sha256'],
        'measurement_protocol_sha256': report['measurement_protocol_sha256'],
        'amplitude_scale': report['scales'][label],
        'maximum_total_world_displacement': float(norm.max()),
        'maximum_eye_local_world_displacement': float(norm[eye].max()),
        'changed_vertices': int(np.count_nonzero(norm)),
        'delta_world_ranges': [[float(actual_delta[:, k].min()), float(actual_delta[:, k].max())]
                               for k in range(3)],
        'frozen_vertices': int(frozen.sum()), 'only_changed_object': BODY,
        'source_unchanged': True, 'geometry_only': True,
        'visual_acceptance': 'pending key views', 'independent_validation': 'pending',
        'FBX': 'native-only candidate; no export requested'})
    print('C3_CANDIDATE_SAVED', label, str(destination), float(norm.max()), flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root', type=Path, required=True)
    ap.add_argument('--prepare', action='store_true')
    ap.add_argument('--candidate', choices=('A', 'B'))
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
    args = ap.parse_args(argv)
    assert args.prepare != bool(args.candidate), 'Choose preparation or one candidate'
    root = args.root.resolve()
    prepare_direction(root) if args.prepare else generate(root, args.candidate)


if __name__ == '__main__':
    main()
