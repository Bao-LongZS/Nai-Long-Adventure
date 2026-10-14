"""Package the recorded C3 decision without changing any saved model."""
import ast
import csv
import hashlib
import json
from pathlib import Path
import sys
import zipfile

ROOT = Path(__file__).resolve().parent
SCRIPTS = ROOT.parents[1]
sys.path.insert(0, str(SCRIPTS))
from assemble_c3_review import compose, read_image, registered_crop, REFERENCE, REFERENCE_CROP


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def dump(path, value):
    Path(path).write_text(json.dumps(value, ensure_ascii=False, indent=2,
                                    allow_nan=False) + '\n', encoding='utf-8')


def main():
    visual = read(ROOT / 'visual_acceptance.json')
    validation = read(ROOT / 'candidate_B/validation_report.json')
    rebuild = read(ROOT / 'rebuild_selected/rebuild_report.json')
    build = read(ROOT / 'candidate_B/build_report.json')
    accepted = bool(visual['passed'] and validation['passed'] and rebuild['passed'])
    candidate = ROOT / 'candidate_B/Naiwa_Character.blend'
    source = Path(build['fixed_source'])
    assert sha(candidate) == build['candidate_sha256'] == validation['candidate_sha256'] == rebuild['selected_candidate_sha256']
    assert sha(source) == build['source_sha256']
    contours = read(ROOT / 'review/assembled/fixed_reference_contours.json')['models']
    key = read(ROOT / 'candidate_B/key_actual_sections_summary.json')['comparison']
    central = key['0.0']
    labels = ['图1.3原图', 'E2', 'C3 B' + ('（采用）' if accepted else '（未采用）')]
    paths = [REFERENCE] + [ROOT / 'review/key' / name / f'{name}_common_ORTHO_50.png' for name in ('E2', 'B')]
    destination = ROOT / 'review/final'
    destination.mkdir(parents=True, exist_ok=True)
    composed = compose('原图 / E2 / C3 B · 固定参考对照',
        '同一机位、曝光与注册；原图姿态和照明不同。', labels,
        [('固定50°；无重新拟合', [read_image(REFERENCE).crop(REFERENCE_CROP)] + [registered_crop(p) for p in paths[1:]])],
        destination / 'comparison_reference_E2_B.png', paths)
    dump(destination / 'comparison_protocol.json', composed)

    # Export the measured saved curves verbatim, without another fit or read.
    baseline_raw = read(ROOT / 'baseline/measurements_Raw.json')
    with (ROOT / 'actual_sections_E2_B.csv').open('w', encoding='utf-8', newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(['model', 'state', 'plane_X', 'arc_world', 'world_X', 'world_Y', 'world_Z', 'tangent_radians', 'signed_curvature_per_world_unit'])
        for model, state, sections in [('E2', 'Raw', baseline_raw)] + [('B', state, sections) for state, sections in validation['actual_sections'].items()]:
            for x, record in sections['longitudinal'].items():
                samples = record['samples']
                for arc, point, angle, curvature in zip(samples['arc'], samples['actual_points_world'], samples['unwrapped_tangent_angle_radians'], samples['signed_curvature_per_world_unit']):
                    writer.writerow([model, state, x, arc, *point, angle, curvature])

    selection = {
        'recommended': 'C3_B' if accepted else 'E2', 'C3_B_adopted': accepted,
        'native': 'candidate_B/Naiwa_Character.blend' if accepted else str(source),
        'native_sha256': build['candidate_sha256'] if accepted else build['source_sha256'],
        'fixed_E2_sha256': build['source_sha256'], 'candidate_B_sha256': build['candidate_sha256'],
        'saved_geometry_assets_animation_validation': validation['passed'],
        'independent_native_reproduction': rebuild['passed'], 'visual_acceptance': visual['passed'],
        'native_only_delivery': True, 'new_FBX_exported': False,
        'front_reference_error_pixels_same_protocol': {k: v['neck_mean_abs_front_error_px'] for k, v in contours.items()},
        'back_reference_error_pixels_same_protocol': {k: v['neck_mean_abs_back_error_px'] for k, v in contours.items()},
        'maximum_final_world_displacement_B': build['maximum_total_world_displacement'],
        'maximum_eye_local_displacement_B': build['maximum_eye_local_world_displacement'],
        'source_E2_unchanged': True, 'no_third_candidate': True,
        'residuals': visual['residuals']}
    dump(ROOT / 'selection.json', selection)

    # Snapshot the local Python import closure used in generation and checks.
    pending = [SCRIPTS / (name + '.py') for name in (
        'revise_c3_joint_reference', 'c3_joint_curve_solver', 'render_c3_review',
        'measure_c3_candidate_key', 'assemble_c3_review', 'validate_c3_saved', 'reproduce_c3_selected')]
    sources = set()
    while pending:
        path = pending.pop()
        if path in sources:
            continue
        sources.add(path)
        tree = ast.parse(path.read_text(encoding='utf-8-sig'))
        for node in ast.walk(tree):
            names = [a.name for a in node.names] if isinstance(node, ast.Import) else ([node.module] if isinstance(node, ast.ImportFrom) and node.module else [])
            for name in names:
                dependency = SCRIPTS / (name.split('.')[0] + '.py')
                if dependency.is_file() and dependency not in sources:
                    pending.append(dependency)
    sources.update(ROOT.glob('*.py'))
    with zipfile.ZipFile(ROOT / 'source_scripts.zip', 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(sources):
            archive.write(path, ('version_tools/' if path.parent == ROOT else 'character_scripts/') + path.name)
        archive.write(SCRIPTS / 'run_blender_stage.ps1', 'character_scripts/run_blender_stage.ps1')
    with zipfile.ZipFile(ROOT / 'source_scripts.zip') as archive:
        assert archive.testzip() is None
    dump(ROOT / 'source_manifest.json', {str(p): sha(p) for p in sorted(sources)})

    # File manifest is the final output; exclude itself and Python caches.
    files = [p for p in ROOT.rglob('*') if p.is_file() and p.name != 'file_manifest.json' and '__pycache__' not in p.parts]
    dump(ROOT / 'file_manifest.json', {'schema_version': 1, 'files': [
        {'path': p.relative_to(ROOT).as_posix(), 'bytes': p.stat().st_size, 'sha256': sha(p)} for p in sorted(files)]})
    print('C3_RESULTS_PACKAGED', 'C3_B' if accepted else 'E2', len(files))


if __name__ == '__main__':
    main()
