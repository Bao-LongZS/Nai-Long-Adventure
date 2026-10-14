"""Independently reproduce one selected native C3 candidate from fixed E2.

Run only after selecting A or B. This reuses the frozen direction field, not
the solver or generation driver. It opens E2 once, writes a new reproduction,
then actually reopens that saved project and the selected candidate to compare
every native mesh, asset fingerprint, material/link setting and action curve.
It does not render, export FBX, repeat pose/contact tests, or reconstruct history.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import traceback

import bpy
import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from geometry_diagnostic_utils import array
from head_neck_validation_utils import fingerprint
from saved_delivery_metadata import differences, preserved_links
from validate_n2_round_refinement import rig_snapshot

BODY = 'Naiwa unified golden body'
SOURCE = HERE / 'versions/20261007_H2_E2_overall_refinement/candidate_E2/Naiwa_Character.blend'
SOURCE_SHA = '317f62278d63217636bf393f831e2fb41ce21acfefbe21beed6ad6d4dc75f322'


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def digest(data):
    return hashlib.sha256(data.tobytes()).hexdigest()


def exact_bits(a, b):
    return a.shape == b.shape and a.dtype == b.dtype and a.tobytes() == b.tobytes()


def load_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def dump(path, value):
    Path(path).write_text(json.dumps(value, ensure_ascii=False, indent=2,
                                   allow_nan=False) + '\n', encoding='utf-8')


def read_native(obj):
    local = array(obj.data.vertices, 'co', 3, np.float32)
    obj.data.calc_loop_triangles()
    triangles = array(obj.data.loop_triangles, 'vertices', 3, np.int32)
    return local, triangles


def native_manifest():
    """Hash actual indexed native data; never use field predictions here."""
    objects = {}
    for obj in sorted((o for o in bpy.data.objects if o.type == 'MESH'), key=lambda o: o.name):
        local, triangles = read_native(obj)
        objects[obj.name] = {
            'native_coordinate_dtype': str(local.dtype),
            'native_coordinate_shape': list(local.shape),
            'native_coordinate_sha256': digest(local),
            'native_triangle_dtype': str(triangles.dtype),
            'native_triangle_shape': list(triangles.shape),
            'native_triangle_sha256': digest(triangles),
            'fingerprint': fingerprint(obj),
            'hide_render': bool(obj.hide_render),
            'hide_viewport': bool(obj.hide_viewport),
        }
    return {'objects': objects, 'preserved_links': preserved_links(),
            'rig_snapshot': rig_snapshot()}


def assert_hashes(pinned):
    changed = [path for path, expected in pinned.items() if sha(path) != expected]
    assert not changed, ('Immutable input changed during reproduction', changed)


def independent_coordinates(old_world, world_delta, matrix, old_local, frozen, eye):
    """Solve the affine world-to-local equation independently of the driver."""
    desired_world = old_world + world_delta
    new_local = np.linalg.solve(matrix[:3, :3],
                                (desired_world - matrix[:3, 3]).T).T.astype(np.float32)
    new_local[frozen | eye] = old_local[frozen | eye]
    return new_local


def run(args):
    root = args.root.resolve(strict=True)
    candidate = args.candidate.resolve(strict=True)
    assert candidate.suffix.lower() == '.blend' and candidate.is_relative_to(root)
    assert candidate != SOURCE.resolve(), 'Selected candidate must differ from fixed E2'
    output = root / 'rebuild_selected'
    assert not output.exists(), 'Independent reproduction output already exists; do not overwrite it'
    destination = output / 'Naiwa_Character.blend'

    snapshot_path = root / 'baseline/snapshot.json'
    arrays_path = root / 'baseline/baseline_arrays.npz'
    protocol_path = root / 'measurement_protocol.json'
    field_path = root / 'c3_field.npz'
    field_report_path = root / 'c3_field_report.json'
    build_path = candidate.parent / 'build_report.json'
    snapshot = load_json(snapshot_path)
    protocol = load_json(protocol_path)
    field_report = load_json(field_report_path)
    build = load_json(build_path)
    source = Path(snapshot['source']).resolve(strict=True)
    assert source == SOURCE.resolve(strict=True), 'Unexpected E2 source path'
    pinned = {str(path.resolve(strict=True)): sha(path) for path in
              (source, candidate, snapshot_path, arrays_path, protocol_path,
               field_path, field_report_path, build_path, Path(__file__))}
    assert pinned[str(source)] == snapshot['source_sha256'] == SOURCE_SHA
    assert snapshot.get('protocol_frozen'), 'Baseline protocol is not frozen'
    assert snapshot['array_file_sha256'] == pinned[str(arrays_path)]
    assert snapshot['measurement_protocol_sha256'] == pinned[str(protocol_path)]
    helper = Path(protocol['sections']['measurement_helper_file']).resolve(strict=True)
    assert helper == (HERE / 'c3_measurement_utils.py').resolve(strict=True)
    pinned[str(helper)] = sha(helper)
    assert protocol['sections']['measurement_helper_sha256'] == pinned[str(helper)]
    assert build['label'] == args.label
    assert build['source_sha256'] == SOURCE_SHA
    assert build['candidate_sha256'] == pinned[str(candidate)]
    assert build['measurement_protocol_sha256'] == pinned[str(protocol_path)]
    assert build['field_sha256'] == pinned[str(field_path)]
    assert field_report['source_sha256'] == SOURCE_SHA
    assert field_report['baseline_arrays_sha256'] == pinned[str(arrays_path)]
    assert field_report['measurement_protocol_sha256'] == pinned[str(protocol_path)]
    assert field_report['field_sha256'] == pinned[str(field_path)]
    scale = float(field_report['scales'][args.label])
    assert np.isfinite(scale) and scale > 0
    assert build['amplitude_scale'] == scale

    report = {
        'schema_version': 1, 'passed': False, 'label': args.label,
        'purpose': 'one independent selected-native reproduction from fixed E2',
        'selected_candidate': str(candidate), 'selected_candidate_sha256': pinned[str(candidate)],
        'source': str(source), 'source_sha256': SOURCE_SHA,
        'reproduced_project': str(destination), 'amplitude_scale': scale,
        'immutable_inputs_sha256': pinned,
        'recipe': 'frozen world direction times recorded scale; solve affine transform; float32 native coordinates; exact frozen and eye-local restoration',
        'solver_or_generation_driver_imported': False,
        'historical_stages_reconstructed': False,
        'render_or_export_performed': False,
        'pose_contact_validation_repeated': False,
        'objects': {},
    }
    output.mkdir()
    try:
        with np.load(arrays_path, allow_pickle=False) as cache, np.load(field_path, allow_pickle=False) as field:
            prefix = snapshot['objects'][BODY]['array_prefix']
            old_local = cache[prefix + '_raw_local'].copy()
            old_world = cache[prefix + '_raw_world'].copy()
            direction = field['vertex_direction_world'].copy()
            frozen = cache['body_frozen_mask'].astype(bool)
            eye = cache['body_eye_local_mask'].astype(bool)
            allowed = (cache['body_scope_weight'] > 0) | (cache['body_release_weight'] > 0)
            assert old_local.dtype == np.float32 and old_world.shape == old_local.shape
            assert direction.shape == old_world.shape and np.isfinite(direction).all()
            assert np.array_equal(field['frozen_mask'].astype(bool), frozen)
            assert np.array_equal(field['eye_local_mask'].astype(bool), eye)
            assert not np.any(direction[frozen | eye | ~allowed]), 'Frozen/eye/outside-scope direction is nonzero'
            report['field_masks_equal_immutable_baseline'] = True
            bpy.ops.wm.open_mainfile(filepath=str(source), load_ui=False)
            names = set(snapshot['objects'])
            assert names == {obj.name for obj in bpy.data.objects if obj.type == 'MESH'}
            for name in sorted(names):
                local, triangles = read_native(bpy.data.objects[name])
                original = snapshot['objects'][name]
                pre = original['array_prefix']
                assert exact_bits(local, cache[pre + '_raw_local']), ('Actual E2 native coordinates differ from cache', name)
                assert exact_bits(triangles, cache[pre + '_triangles']), ('Actual E2 triangles differ from cache', name)
                delta = differences(original['fingerprint'], fingerprint(bpy.data.objects[name]))
                assert not delta, ('Actual E2 fingerprint differs from frozen snapshot', name, delta[:12])
            link_diff = differences(snapshot['preserved_links'], preserved_links())
            rig_diff = differences(snapshot['rig_snapshot'], rig_snapshot())
            assert not link_diff, ('Actual E2 asset links/actions differ from frozen snapshot', link_diff[:12])
            assert not rig_diff, ('Actual E2 rig differs from frozen snapshot', rig_diff[:12])
            report['actual_source_native_data_matches_fixed_cache'] = True
            body = bpy.data.objects[BODY]
            matrix = np.asarray(body.matrix_world, float)
            assert matrix.tolist() == snapshot['objects'][BODY]['transform']
            new_local = independent_coordinates(old_world, direction * scale, matrix, old_local, frozen, eye)
            assert np.isfinite(new_local).all()
            actual_world = new_local.astype(float) @ matrix[:3, :3].T + matrix[:3, 3]
            amount = np.linalg.norm(actual_world - old_world, axis=1)
            assert amount.max(initial=0) <= protocol['budgets'][args.label]
            assert exact_bits(new_local[frozen | eye], old_local[frozen | eye])
            assert exact_bits(new_local[~allowed], old_local[~allowed])
            report['maximum_actual_world_displacement'] = float(amount.max(initial=0))
            report['frozen_and_eye_local_coordinates_bit_exact'] = True
            expected_coordinate_hash = digest(new_local)
            body.data.vertices.foreach_set('co', new_local.ravel())
            body.data.update()
            bpy.context.view_layer.update()
            bpy.ops.wm.save_as_mainfile(filepath=str(destination), check_existing=False)
        assert_hashes(pinned)
        saved_sha = sha(destination)
        bpy.ops.wm.open_mainfile(filepath=str(destination), load_ui=False)
        reproduced = native_manifest()
        assert reproduced['objects'][BODY]['native_coordinate_sha256'] == expected_coordinate_hash, 'Saved/reopened reproduction differs from independent coordinates'
        report['reproduction_actually_saved_and_reopened'] = True
        print('C3_REPRODUCTION_SAVED_REOPENED', args.label, flush=True)
        bpy.ops.wm.open_mainfile(filepath=str(candidate), load_ui=False)
        selected = native_manifest()
        mismatch = differences(reproduced, selected)
        assert not mismatch, ('Reopened reproduction differs from selected candidate', mismatch[:16])
        assert sha(destination) == saved_sha, 'Read-only comparison changed reproduction'
        assert_hashes(pinned)
        for name, item in reproduced['objects'].items():
            report['objects'][name] = {
                'native_coordinates_bit_exact': True, 'native_triangles_bit_exact': True,
                'coordinate_sha256': item['native_coordinate_sha256'],
                'triangle_sha256': item['native_triangle_sha256'],
                'vertex_count': item['native_coordinate_shape'][0],
                'triangle_count': item['native_triangle_shape'][0],
                'fingerprint_exact': True, 'visibility_exact': True,
            }
        report['all_materials_links_modifiers_constraints_exact'] = True
        report['rig_exact'] = True
        report['complete_original_action_curves_exact'] = True
        report['all_immutable_input_hashes_unchanged'] = True
        report['reproduced_project_sha256'] = saved_sha
        report['passed'] = True
    except Exception as error:
        report['error'] = str(error)
        report['traceback'] = traceback.format_exc()
        raise
    finally:
        report['final_immutable_hash_check'] = {
            path: Path(path).is_file() and sha(path) == expected for path, expected in pinned.items()
        }
        if not all(report['final_immutable_hash_check'].values()):
            report['passed'] = False
            report['hash_error'] = 'At least one fixed input hash changed during reproduction'
        dump(output / 'rebuild_report.json', report)
    assert report['passed'], 'Independent reproduction did not pass'
    print('C3_SELECTED_REPRODUCTION_OK', args.label, str(destination), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--label', choices=('A', 'B'), required=True)
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
    run(parser.parse_args(argv))


if __name__ == '__main__':
    main()
