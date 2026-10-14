"""Package saved, independently measured and visually reviewed H2/E2 assets."""
import ast
import argparse
import hashlib
import html
import json
from pathlib import Path
import shutil
import zipfile

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'versions/20261007_H2_E2_overall_refinement'
R1 = ROOT / 'versions/20261007_N2_round_refinement/candidate_R1/Naiwa_Character.blend'
R1_SHA = 'ea12e9af2c564527d0c473cd31b28974339fcceb15b666502a33f0ffa057bc24'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def write(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def main():
    global OUT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=OUT, help='Final version root; all reports must bind these saved candidates')
    args = parser.parse_args()
    OUT = args.output.resolve()
    assert sha(R1) == R1_SHA
    render = read(OUT / 'render_protocol.json')
    visual = read(OUT / 'visual_acceptance.json')
    diagnostic = read(OUT / 'diagnostics/overall_surface_diagnostics.json')
    reference = read(OUT / 'reference_comparison/overall_reference_review.json')
    poses = read(OUT / 'pose_comparison/pose_protocol.json')
    assembly = read(OUT / 'comparisons/assembly_protocol.json')
    assert assembly['model_order'] == ['R1', 'H2', 'E2'], 'Final panels must include R1/H2/E2 in that order'
    assert not assembly['standard_only'] and assembly['same_render_conditions_verified']
    assert not poses['saved_source_models'] and poses['same_camera_light_exposure']
    assert reference['R1_baseline_consistency']['all_five_rows_exact']
    assert render['same_camera_light_exposure_per_comparison']
    assert not render['saved_source_models']
    assert render['sources']['R1']['sha256'] == R1_SHA
    assert diagnostic['actual_surface_measurement'] and not diagnostic['automatic_visual_acceptance']
    expected_hashes = {'R1': R1_SHA}
    for label in ('H2', 'E2'):
        expected_hashes[label] = sha(OUT / f'candidate_{label}/Naiwa_Character.blend')
    for label, expected in expected_hashes.items():
        assert assembly['source_sha256'][label] == expected
        assert poses['sources'][label]['sha256'] == expected
        assert diagnostic['models'][label]['source']['source_sha256'] == expected
        assert set(diagnostic['models'][label]['poses']) == {'Raw', 'Idle1'}
        assert reference['models'][label]['source_sha256'] == expected
    for record in render['files']:
        assert record['source_sha256'] == expected_hashes[record['model']], f'Stale render {record["file"]}'
        assert (OUT / record['file']).is_file(), record['file']
    pose_keys = set()
    for record in poses['files']:
        assert record['source_sha256'] == expected_hashes[record['model']], f'Stale action view {record["file"]}'
        assert (OUT / 'pose_comparison' / record['file']).is_file(), record['file']
        pose_keys.add((record['model'], record['action'], record['frame'], record['pose']['yaw']))
    for label in expected_hashes:
        for action, frame in (('Naiwa_Walk', 9), ('Naiwa_Laugh', 7), ('Naiwa_Jump', 6)):
            for yaw in (45., 90.):
                assert (label, action, frame, yaw) in pose_keys, ('Missing action comparison', label, action, frame, yaw)
    for panel in assembly['panels']:
        assert panel['output_sha256'] == sha(OUT / 'comparisons' / panel['output']), f'Stale panel {panel["output"]}'
        for path in panel['inputs']:
            assert panel['input_sha256'][path] == sha(OUT / path), f'Stale panel input {path}'
    selection = {'recommended': 'E2', 'fixed_R1_sha256': R1_SHA, 'models': {},
                 'stage_order': ['R1 -> H2 overall surface', 'selected H2 -> E2 eyes'],
                 'numeric_and_visual_acceptance_recorded_separately': True}
    for label in ('H2', 'E2'):
        folder = OUT / f'candidate_{label}'
        native, fbx = folder / 'Naiwa_Character.blend', folder / 'Naiwa_Character.fbx'
        native_sha = sha(native)
        native_check = read(folder / 'validation_report.json')
        assert native_check['passed']
        assert native_check['candidate_sha256'] == native_sha
        assert native_check['source_sha256'] == (R1_SHA if label == 'H2' else expected_hashes['H2']), 'Wrong stage input'
        build = read(folder / ('build_report.json' if label == 'H2' else 'e2_build_report.json'))
        assert build['saved_blend_sha256'] == native_sha
        assert build['source_sha256'] == (R1_SHA if label == 'H2' else expected_hashes['H2']), 'Wrong construction lineage'
        fbx_check = read(folder / 'fbx_validation.json')
        assert fbx_check['status'] == 'passed'
        assert fbx_check['source_blend_sha256'] == native_sha
        assert fbx_check['fbx_sha256'] == sha(fbx)
        rebuild = read(folder / 'rebuild_validation.json')
        assert rebuild['independent_validation'] == 'passed'
        assert rebuild['target_sha256'] == native_sha
        assert rebuild['separate_saved_file_loads']
        rebuilt = Path(rebuild['rebuild']).resolve(strict=True)
        assert rebuilt != native.resolve(), 'A delivery is not an independent rebuild'
        assert sha(rebuilt) == rebuild['rebuild_sha256'], 'Rebuild source changed or disappeared'
        assert render['sources'][label]['sha256'] == native_sha
        assert render['sources'][label]['diagnostics_complete']
        assert visual['models'][label]['passed']
        assert visual['models'][label]['candidate_sha256'] == native_sha
        assert poses['sources'][label]['sha256'] == native_sha
        assert diagnostic['models'][label]['source']['source_sha256'] == native_sha
        assert reference['models'][label]['source_sha256'] == native_sha
        selection['models'][label] = {
            'native': str(native.relative_to(OUT)).replace('\\', '/'),
            'fbx': str(fbx.relative_to(OUT)).replace('\\', '/'),
            'native_sha256': native_sha, 'fbx_sha256': sha(fbx),
            'native_geometry_asset_animation_validation': 'passed',
            'visual_acceptance': 'passed', 'independent_rebuild': 'passed',
            'FBX_actual_surface_weights_colors_actions_validation': 'passed',
            'neck_reference_error_pixels': reference['models'][label]['neck_mean_absolute_front_back_error_pixels']}
    selection['native'] = selection['models']['E2']['native']
    selection['fbx'] = selection['models']['E2']['fbx']
    selection['native_sha256'] = selection['models']['E2']['native_sha256']
    selection['fbx_sha256'] = selection['models']['E2']['fbx_sha256']
    for required in ('README_交付说明.md', 'rebuild_command.ps1', '修改指令_固定输入.md', 'input_manifest.json'):
        assert (OUT / required).is_file(), required

    seeds = ['revise_h2_overall_refinement.py', 'h2_overall_sculpt.py',
             'revise_e2_eye_refinement.py', 'e2_eye_sculpt.py',
             'validate_h2_overall_refinement.py', 'validate_e2_refinement.py',
             'validate_h2_e2_fbx.py', 'validate_delivery_rebuild.py',
             'render_h2_e2_comparisons.py', 'make_h2_e2_comparisons.py',
             'render_h2_e2_poses.py',
             'analyze_h2_e2_overall.py', 'compare_h2_e2_reference.py',
             'evaluate_e2_gaze_candidates.py', 'export_saved_fbx.py',
             'package_h2_e2_delivery.py', 'finalize_h2_e2_review.py',
             'render_h2_shape_gate.py']
    needed = set()

    def collect(name):
        path = ROOT / name
        if name in needed or not path.is_file():
            return
        needed.add(name)
        tree = ast.parse(path.read_text(encoding='utf-8-sig'))
        for node in ast.walk(tree):
            modules = []
            if isinstance(node, ast.Import):
                modules = [item.name.split('.')[0] for item in node.names]
            elif isinstance(node, ast.ImportFrom) and node.module:
                modules = [node.module.split('.')[0]]
            for module in modules:
                collect(module + '.py')

    for name in seeds:
        assert (ROOT / name).is_file(), name
        collect(name)
    needed.add('run_blender_stage.ps1')
    assert all((ROOT / name).is_file() for name in needed)
    dependencies = [R1,
        ROOT / 'versions/20261006_spherical_eyes/comparisons/camera_protocol.json',
        ROOT / 'versions/20261007_N2_round_refinement/reference_comparison/reference_review.json',
        ROOT / 'references/20261005/pdf_page03_image03_IM60.jpg',
        ROOT / 'references/20261005/pdf_page08_fig6_2.png',
        ROOT / 'references/20261005/pdf_page09_fig6_5.png']
    input_manifest = read(OUT / 'input_manifest.json')
    for record in input_manifest['inputs']:
        source = Path(record['path'])
        assert sha(source) == record['sha256'], f'Fixed input changed: {record["role"]}'
    dependency_manifest = {
        'source_scripts_include_local_import_closure': True,
        'inputs_are_in_existing_project_not_in_script_zip': True,
        'runtime': {'Blender': 'Use the same Blender release as the saved models; rebuild_command.ps1 accepts -Blender',
                    'analysis_Python': 'Python with numpy and Pillow'},
        'files': [{'path': str(path.relative_to(ROOT)).replace('\\', '/'),
                   'bytes': path.stat().st_size, 'sha256': sha(path)} for path in dependencies],
        'delivered_recipe_and_comparison_targets': [
            {'path': f'candidate_{label}/{name}', 'sha256': sha(OUT / f'candidate_{label}' / name)}
            for label in ('H2', 'E2')
            for name in ('Naiwa_Character.blend', 'parameters.json' if label == 'H2' else 'e2_build_report.json')]}

    panels = [('全身形体三视图', 'comparisons/comparison_full.png'),
              ('全身灰模', 'comparisons/comparison_full_gray.png'),
              ('主体轮廓', 'comparisons/comparison_full_body_id.png'),
              ('头部彩色', 'comparisons/comparison_head.png'),
              ('头部灰模', 'comparisons/comparison_head_gray.png'),
              ('嘴下连接', 'comparisons/comparison_lowerface_detail.png'),
              ('眼球与皮肤接触', 'comparisons/comparison_eyes_detail.png'),
              ('真实反射条纹', 'comparisons/comparison_reflection.png'),
              ('嘴下反射细节', 'comparisons/comparison_reflection_detail.png'),
              ('掠射方向 A', 'comparisons/comparison_grazing_A.png'),
              ('掠射方向 B', 'comparisons/comparison_grazing_B.png'),
              ('固定参考与原注册', 'reference_comparison/overall_fixed_reference_color.png'),
              ('纯主体 ID 与原注册', 'reference_comparison/overall_fixed_reference_body_ID.png'),
              ('固定 45° / 55° / 长焦交叉检查', 'comparisons/comparison_reference_camera_crosschecks.png')]
    for mode in ('Raw', 'Idle1'):
        for title, stem in (('实际纵剖面', 'longitudinal_sections'), ('实际外围长弧', 'outer_profiles'),
                            ('实际横截面', 'horizontal_sections'), ('实际带符号曲率', 'signed_curvature'),
                            ('实际弧长切向角', 'tangent_angle_degrees')):
            panels.append((f'{mode} · {title}', f'diagnostics/actual_{stem}_{mode}.png'))
    for action in ('Walk', 'Laugh', 'Jump'):
        panels.append((f'{action} 肩背与五官', f'comparisons/comparison_pose_{action}.png'))
    figures = []
    for title, path in panels:
        assert (OUT / path).is_file(), path
        figures.append(f'<figure><figcaption>{html.escape(title)}</figcaption><a href="{path}"><img loading="lazy" src="{path}" alt="{html.escape(title)}"></a></figure>')
    for filename in ('actual_arc_samples.csv', 'actual_transverse_arc_samples.csv', 'overall_surface_diagnostics.md'):
        assert (OUT / 'diagnostics' / filename).is_file(), filename
    for label in ('R1', 'H2', 'E2'):
        observation = diagnostic['models'][label]['source']
        assert sha(OUT / 'diagnostics' / f'overall_mesh_{label}.npz') == observation['mesh_sha256']
    # Generate release labels only after all independent reports and actual views agree.
    write(OUT / 'selection.json', selection)
    write(OUT / 'input_dependency_manifest.json', dependency_manifest)
    source_manifest = {name: sha(ROOT / name) for name in sorted(needed)}
    command_entry = 'versions/20261007_H2_E2_overall_refinement/rebuild_command.ps1'
    source_manifest[command_entry] = sha(OUT / 'rebuild_command.ps1')
    with zipfile.ZipFile(OUT / 'source_scripts.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for name in sorted(needed):
            archive.write(ROOT / name, 'NaiwaCharacter/' + name)
        archive.write(OUT / 'rebuild_command.ps1', 'NaiwaCharacter/' + command_entry)
    with zipfile.ZipFile(OUT / 'source_scripts.zip') as archive:
        assert archive.testzip() is None
        for name, expected in source_manifest.items():
            assert hashlib.sha256(archive.read('NaiwaCharacter/' + name)).hexdigest() == expected
    write(OUT / 'source_scripts_manifest.json', source_manifest)
    (OUT / 'references').mkdir(exist_ok=True)
    for source, name, title in (
        (dependencies[3], 'figure_1_3.jpg', '图 1.3 · 主形体参考'),
        (dependencies[4], 'figure_6_2.png', '图 6.2 · 形态交叉参考'),
        (dependencies[5], 'figure_6_5.png', '图 6.5 · 形态交叉参考')):
        dest = OUT / 'references' / name
        shutil.copyfile(source, dest)
        assert sha(dest) == sha(source)
        figures.append(f'<figure><figcaption>{title}（原始图；姿态和镜头不同）</figcaption><a href="references/{name}"><img loading="lazy" src="references/{name}" alt="{title}"></a></figure>')
    page = '''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>奶蛙 H2 / E2 整体修正</title><style>body{font:16px/1.65 system-ui;background:#eef2f6;color:#1a2940;max-width:1800px;margin:32px auto;padding:0 24px}h1{font-size:30px}a{color:#236ba0}figure{margin:28px 0;background:white;border-radius:12px;padding:20px}figcaption{font-size:22px;margin-bottom:14px}img{display:block;width:100%;height:auto}.intro{background:#19283d;color:white;padding:24px;border-radius:12px}.intro a{color:#9fcef1}</style><h1>奶蛙 · R1 → H2 → E2</h1><div class="intro">H2 联合修正头颈肩背；E2 从选定 H2 独立校正眼部。推荐 E2，H2 保留为形体阶段工程。数值完整性与整体视觉验收分别记录。<p><a href="candidate_E2/Naiwa_Character.blend">推荐 E2 工程</a>　<a href="candidate_E2/Naiwa_Character.fbx">E2 FBX</a>　<a href="candidate_H2/Naiwa_Character.blend">H2 工程</a>　<a href="candidate_H2/Naiwa_Character.fbx">H2 FBX</a>　<a href="README_交付说明.md">完整说明</a>　<a href="visual_acceptance.json">视觉验收</a></p></div>'''
    evidence = '<p>实际数据：<a href="diagnostics/actual_arc_samples.csv">纵剖面与外围长弧 CSV</a> · <a href="diagnostics/actual_transverse_arc_samples.csv">横截面 CSV</a> · <a href="diagnostics/overall_surface_diagnostics.json">曲面诊断 JSON</a> · <a href="reference_comparison/overall_reference_review.json">固定参考诊断</a> · <a href="source_scripts.zip">源码及本地导入闭包</a> · <a href="input_dependency_manifest.json">复建输入依赖及校验值</a> · <a href="file_manifest.json">全部交付文件校验值</a></p>'
    for label in ('H2', 'E2'):
        evidence += f'<p>{label} 独立证据：<a href="candidate_{label}/validation_report.json">原生几何与资产检查</a> · <a href="candidate_{label}/fbx_validation.json">FBX 重导入检查</a> · <a href="candidate_{label}/rebuild_validation.json">独立复建比对</a></p>'
    (OUT / 'index.html').write_text(page + evidence + ''.join(figures) + '</html>\n', encoding='utf-8')
    entries = []
    for path in sorted(OUT.rglob('*')):
        if not path.is_file() or path.name == 'file_manifest.json' or '__pycache__' in path.parts:
            continue
        entries.append({'path': str(path.relative_to(OUT)).replace('\\', '/'),
                        'bytes': path.stat().st_size, 'sha256': sha(path)})
    write(OUT / 'file_manifest.json', {'recommended': 'E2', 'source_R1_unchanged': sha(R1) == R1_SHA,
                                     'files': entries})
    print('H2_E2_DELIVERY_PACKAGED', len(entries), 'files', len(needed), 'source scripts', flush=True)


if __name__ == '__main__':
    main()
