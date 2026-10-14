"""Assemble a reviewable A/B delivery from independently validated files."""
from pathlib import Path
import csv
import hashlib
import html
import json
import math
import posixpath
import re
import zipfile

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'versions/20261006_spherical_eyes'
BASE = ROOT / 'versions/20261005_head_side_continuity/Naiwa_Character.blend'
BASE_SHA = 'a95a5c7efb25cb6f379897dd6ec4af98a201bc2619dc8caa2bb5b5de8ea3dfd5'
D0 = 0.11724481274201151
S0 = 0.23462580179330159
INSTRUCTION = Path('C:/Users/31314/Documents/Codex/2026-10-05/li/outputs/Naiwa_眼体重建指令_20261006.md')

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def read_json(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

assert digest(BASE) == BASE_SHA
summary = read_json(OUT / 'spherical_eyes_validation.json')
assert summary['source_unchanged']
default_rebuild = read_json(OUT / 'default_rebuild_validation.json')
assert default_rebuild['passed'] and default_rebuild['source_unchanged']
assert default_rebuild['immutable_source_sha256'] == BASE_SHA
rows = []
links = []
validation_records = {}
for candidate in ('A', 'B'):
    folder = OUT / ('candidate_' + candidate)
    report = read_json(folder / 'validation_report.json')
    build = read_json(folder / 'spherical_eye_build_report.json')
    fbx = read_json(folder / 'fbx_validation.json')
    rebuild = read_json(folder / 'rebuild_validation.json')
    assert report['passed'] and report['non_eye_meshes_rig_exact']
    assert fbx['status'] == 'passed'
    assert fbx['fbx_sha256'] == digest(folder / 'Naiwa_Character.fbx')
    assert rebuild['independent_validation'] == 'passed'
    assert abs(report['actual_spacing'] / S0 - 1) <= .02
    links.append(f'- [{candidate} 原生工程](candidate_{candidate}/Naiwa_Character.blend) · '
                 f'[FBX](candidate_{candidate}/Naiwa_Character.fbx) · '
                 f'[实际几何验收](candidate_{candidate}/validation_report.json) · '
                 f'[复建一致性](candidate_{candidate}/rebuild_validation.json)')
    validation_records[candidate] = {
        'blend_sha256': digest(folder / 'Naiwa_Character.blend'),
        'fbx_sha256': digest(folder / 'Naiwa_Character.fbx'),
        'independent_geometry_validation': True,
        'rebuild_exact': True,
        'non_eye_meshes_rig_exact': True,
        'actual_spacing': report['actual_spacing'],
    }
    for side in ('L', 'R'):
        eye = report['eyes'][side]
        params = build['eyes'][side]
        raw = eye['raw_world_fit']
        evaluated = eye['Idle_frame1_evaluated_world_fit']
        assert raw['maximum_relative_radius_residual'] <= 1e-4
        assert evaluated['maximum_relative_radius_residual'] <= 1e-4
        assert abs(eye['actual_visible_D'] / D0 - 1) <= .03
        assert abs(eye['actual_pupil_ratio'] - .53) <= .02
        rows.append({
            'candidate': candidate, 'eye': side,
            'R': params['R'], 'h_local_normal': params['h'],
            'R_over_D0': params['R'] / D0, 'h_over_D0': params['h'] / D0,
            'visible_D': eye['actual_visible_D'],
            'visible_D_error_percent': 100 * (eye['actual_visible_D'] / D0 - 1),
            'pupil_ratio': eye['actual_pupil_ratio'],
            'C_X': params['sphere_center'][0], 'C_Y': params['sphere_center'][1],
            'C_Z': params['sphere_center'][2],
            'P_X': params['install_P'][0], 'P_Y': params['install_P'][1],
            'P_Z': params['install_P'][2],
            'n_X': params['install_normal'][0], 'n_Y': params['install_normal'][1],
            'n_Z': params['install_normal'][2],
            'visible_center_X': eye['frontal_metrics']['aperture']['center'][0],
            'visible_center_Z': eye['frontal_metrics']['aperture']['center'][1],
            'raw_max_relative_radius_residual': raw['maximum_relative_radius_residual'],
            'Idle_frame1_max_relative_radius_residual': evaluated['maximum_relative_radius_residual'],
        })

with (OUT / 'calibrated_parameters.csv').open('w', encoding='utf-8-sig', newline='') as f:
    writer = csv.DictWriter(f, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)
(OUT / 'calibrated_parameters.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')

table_header = '|方案/眼|实际 R|实际 h（局部法线）|可见 D|D 偏差|黑瞳/眼径|球面最大相对残差（Idle）|\n|---|---:|---:|---:|---:|---:|---:|'
table_rows = [f"|{r['candidate']}/{r['eye']}|{r['R']:.10f}|{r['h_local_normal']:.10f}|{r['visible_D']:.10f}|{r['visible_D_error_percent']:+.4f}%|{r['pupil_ratio']:.6f}|{r['Idle_frame1_max_relative_radius_residual']:.3g}|" for r in rows]
param_table = '\n'.join([table_header] + table_rows)
reference_review = OUT / 'reference_comparison/reference_review.md'
assert reference_review.is_file(), 'Reference review must finish before packaging'
review_text = reference_review.read_text(encoding='utf-8-sig')
def relocated_review_link(match):
    target = match.group(2)
    if target.startswith(('#', '/', 'http:', 'https:')):
        return match.group(0)
    return match.group(1) + posixpath.normpath('reference_comparison/' + target) + match.group(3)
review_in_delivery = re.sub(r'(\]\()([^\)]+)(\))', relocated_review_link, review_text)
review_in_delivery = review_in_delivery.replace('`../reference_search/camera_search.json`', '`reference_search/camera_search.json`')
reference_data = read_json(OUT / 'reference_comparison/reference_review.json')
assert reference_data['fixed_scale_registration']['eye_diameter_used_for_scale'] is False
selection_summary = ('建议以 A 作为下一轮继续打磨的基准：参考视角下露出较克制、远眼暴露较少。'
                     'B 的正面与斜侧近眼更圆，保留供比较。两者仍有远眼遮挡与头部轮廓残差，'
                     '本轮没有据此缩头或修改颈部。')
if INSTRUCTION.is_file():
    (OUT / '修改指令.md').write_text(INSTRUCTION.read_text(encoding='utf-8-sig'), encoding='utf-8')

sections = [
    ('头部彩色三视图', 'comparisons/comparison_head.png'),
    ('头部灰模三视图', 'comparisons/comparison_head_gray.png'),
    ('眼部彩色近景', 'comparisons/comparison_eye.png'),
    ('眼部灰模近景', 'comparisons/comparison_eye_gray.png'),
    ('全身正面，同一尺度', 'comparisons/comparison_full.png'),
    ('原版与 A/B 眼体侧剖面', 'comparisons/eye_side_sections.png'),
    ('三个经过球心的正交剖面', 'comparisons/sphere_center_sections.png'),
    ('原版与 A/B 实际测量表', 'comparisons/eye_measurements.png'),
    ('参考图与原版/A/B，稳定整体尺度', 'reference_comparison/reference_stable_comparison.png'),
    ('正面注视与外展注视诊断', 'reference_comparison/reference_gaze_comparison.png'),
]
for _, path in sections:
    assert (OUT / path).is_file(), path

readme = f'''# 奶蛙独立球面眼体 A/B 交付 · 2026-10-06

按本目录 [修改指令](修改指令.md) 完成固定主体上的眼体重建、实际网格验收、参考相机复核与可重复生成。打开 [图文总览](index.html) 查看所有对比。

{selection_summary}

## 工程和修改范围

{chr(10).join(links)}

固定输入为 `../20261005_head_side_continuity/Naiwa_Character.blend`，SHA256 `{BASE_SHA}`。黄色主体、嘴、头部截面、骨架、姿态及非眼部网格逐项保持。仅替换左右眼网格并重新贴合独立高光；对象名、父子、材质关联及有效权重的独立核查见验收报告。

眼体使用单一球心与半径的完整闭合球面。皮肤仅用于安装点、安装法线与遮挡计算，不逐顶点叠加皮肤坡度；没有旧构造的 0.12 回背偏移。黑绿为同球面的颜色分区，无独立瞳孔台阶。保存的 A/B 均保持前方 F=−Y 注视；20°/35°外展只做诊断渲染，没有保存为工程变更。

## 校准后的实际参数

D0={D0:.12f}，S0={S0:.12f}；均为工程单位。D 为正面可见外缘包围面积（含黑瞳）的等面积直径。h 沿安装法线量取，不能与旧全局 F 向凸出混用。A/B 的初始 h=0.30D0/0.40D0 为构造对照，不是从参考图恢复的真实尺寸。校准保持各方案初始 h/R 比值，调整球半径与安装位置，所以表中实际 R/h 与初始值不同。

{param_table}

完整的左右球心 C、安装点 P、单位法线 n、实际可见中心及残差见 [CSV](calibrated_parameters.csv)、[JSON](calibrated_parameters.json) 和各候选 `spherical_eye_build_report.json`。水平眼距 A={validation_records['A']['actual_spacing']:.12f}、B={validation_records['B']['actual_spacing']:.12f}，相对 S0 误差分别 {100*(validation_records['A']['actual_spacing']/S0-1):+.4f}%、{100*(validation_records['B']['actual_spacing']/S0-1):+.4f}%。

## 验收证据

对原始未变形网格和 Idle 第1帧实际评估网格分别拟合球心/半径；三个经过球心的正交剖面、闭合拓扑、外向法线、黑绿共享边界、安装穿入与高光接触均独立检查。未使用 `complete_spherical_eye` 元数据代替几何验收。身体不为满足眼径而变形；被主体遮挡后的投影不要求在各视角均为圆。

两候选 FBX 重导入保留 6 个绑定网格、10 根骨骼、4 个动作和身体/眼睛颜色。默认生成入口独立复建后，坐标、拓扑、权重、颜色和骨架与交付逐项一致。相机协议记录每一张原图的镜头、取景、源工程 SHA256；检查图对应当前保存版本。

## 对比材料

{chr(10).join(f'- [{title}]({path})' for title, path in sections)}

三列从左至右为 Original、A、B，头部与眼部均包含正面/45°/90°。同一组使用相同相机、灯光、曝光与画面缩放。全身保持相同尺度，不按各自眼径单独放大。原始 PNG、像素测量及相机搜索均保留。参考残差仅记录，未据旧眼径归一化图计算缩头倍率。

## 参考视角与候选建议

{review_in_delivery}

## 生成与复查

项目 `NaiwaCharacter/build_naiwa.py` 默认调用 `revise_spherical_eyes.py`，从固定输入重新生成 A/B。可传 `--output` 指定新目录和 `--candidate A/B/both`；禁止以历史 `--from-primitives` 替代当前工程。

通过 `run_blender_stage.ps1` 运行生成入口，Python异常须触发非零退出，成功标记 `NAIWA_BUILD_OK`。默认几何验收入口 `validate_head_revision.py` 调用 `validate_spherical_eyes.py`；标记 `SPHERICAL_EYES_VALIDATION_OK`。复建验证入口 `run_spherical_eye_rebuild_check.py`，完成标记 `SPHERICAL_DEFAULT_REBUILD_MATCH_OK`。

[生成脚本快照](generation_scripts.zip) 保存根目录 Python/PowerShell 脚本与固定基准记录；不复制历史工程。复现需要原项目中的固定输入 `.blend` 和参考图目录。解压到项目副本的 `NaiwaCharacter`，保留 `versions/20261005_head_side_continuity`。源文件清单见 `generation_source_manifest.json`，交付校验见 `SHA256SUMS.json`、`delivery_validation.json`。
'''
(OUT / 'README_交付说明.md').write_text(readme, encoding='utf-8')

def e(value):
    return html.escape(str(value))

table_html = '<table><thead><tr>' + ''.join(f'<th>{e(x)}</th>' for x in ('方案/眼', 'R', 'h 沿法线', '可见 D', 'D 偏差', '黑瞳比', 'Idle 相对残差')) + '</tr></thead><tbody>'
for r in rows:
    values = (r['candidate']+'/'+r['eye'], f"{r['R']:.8f}", f"{r['h_local_normal']:.8f}", f"{r['visible_D']:.8f}", f"{r['visible_D_error_percent']:+.4f}%", f"{r['pupil_ratio']:.6f}", f"{r['Idle_frame1_max_relative_radius_residual']:.3g}")
    table_html += '<tr>' + ''.join(f'<td>{e(x)}</td>' for x in values) + '</tr>'
table_html += '</tbody></table>'
image_html = ''.join(f'<section id="view{i}"><h2>{e(title)}</h2><a href="{path}"><img loading="lazy" src="{path}" alt="{e(title)}"></a></section>' for i, (title, path) in enumerate(sections))
navigation = ''.join(f'<a href="#view{i}">{e(title)}</a>' for i, (title, _) in enumerate(sections))
model_links = ''.join(f'<div class="model"><b>候选 {c}</b><a href="candidate_{c}/Naiwa_Character.blend">Blender 工程</a><a href="candidate_{c}/Naiwa_Character.fbx">FBX</a><a href="candidate_{c}/validation_report.json">验收报告</a></div>' for c in ('A', 'B'))
document = f'''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>奶蛙独立球面眼体 A/B</title>
<style>body{{margin:0;background:#f6f5ef;color:#252a26;font:16px/1.7 "Microsoft YaHei",sans-serif}}main{{max-width:1240px;margin:auto;padding:32px}}h1{{font-size:32px;margin-bottom:8px}}h2{{font-size:22px;margin-top:36px}}a{{color:#376139}}nav{{display:flex;flex-wrap:wrap;gap:8px 20px;padding:18px 0;border-block:1px solid #d9ded4}}.intro{{max-width:900px}}.models{{display:flex;gap:18px;flex-wrap:wrap}}.model{{background:white;border:1px solid #d9ded4;border-radius:10px;padding:18px 26px;display:flex;gap:18px;align-items:center}}.pass{{background:#e3eddc;border-radius:10px;padding:16px 22px}}table{{border-collapse:collapse;background:white;width:100%;font-variant-numeric:tabular-nums}}td,th{{padding:10px;border-bottom:1px solid #d9ded4;text-align:right}}td:first-child,th:first-child{{text-align:left}}.table{{overflow:auto}}section img{{display:block;width:100%;height:auto;border-radius:8px;border:1px solid #d9ded4}}pre{{white-space:pre-wrap;font:inherit;background:white;padding:24px;border-radius:10px}}small{{color:#647063}}</style>
<main><small>NAIWA CHARACTER · 2026-10-06</small><h1>独立球面眼体 A / B</h1><p class="intro">从固定头身工程重建完整球面，保持主体、嘴与绑定。三列同机位图、实际球面验收、参考视角复核和完整参数集中于本页。</p>
<div class="models">{model_links}</div><p><a href="README_交付说明.md">完整中文交付说明</a> · <a href="修改指令.md">修改指令</a> · <a href="generation_scripts.zip">脚本快照</a> · <a href="calibrated_parameters.csv">实际参数 CSV</a> · <a href="delivery_validation.json">交付验收</a></p>
<div class="pass">A/B 原始与 Idle 第1帧球面拟合、几何验收、固定区域核查、FBX 重导入及独立复建通过。基础工程保持正面注视。<p>{e(selection_summary)}</p></div><h2>校准后实际参数</h2><p>D0={D0:.12f}；h 沿局部安装法线，A/B 初值是构造对照。</p><div class="table">{table_html}</div>
<h2>对比索引</h2><nav>{navigation}</nav>{image_html}<h2>参考复核与残差记录</h2><p><a href="reference_comparison/reference_review.md">单独阅读参考复核说明</a></p><pre>{e(review_text)}</pre><p><a href="reference_comparison/reference_review.json">原始像素与稳定对照 JSON</a> · <a href="comparisons/camera_protocol.json">三视图相机协议</a> · <a href="SHA256SUMS.json">SHA256 校验清单</a></p></main></html>'''
(OUT / 'index.html').write_text(document, encoding='utf-8')

# Snapshot existing scripts, including the exact current generator and helpers.
sources = sorted([*ROOT.glob('*.py'), *ROOT.glob('*.ps1')])
source_manifest = {p.name: digest(p) for p in sources}
with zipfile.ZipFile(OUT / 'generation_scripts.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    for p in sources:
        z.write(p, p.name)
    for version in ('20261005_head_side_continuity', '20261005_head_reshape', '20261005_eye_muzzle_body_delivery'):
        p = ROOT / 'versions' / version / 'baseline_dimensions.json'
        if p.is_file():
            z.write(p, str(p.relative_to(ROOT)).replace('\\', '/'))
(OUT / 'generation_source_manifest.json').write_text(json.dumps({'source_sha256': BASE_SHA, 'scripts': source_manifest}, indent=2), encoding='utf-8')

required = ['README_交付说明.md', 'index.html', '修改指令.md', 'calibrated_parameters.csv', 'calibrated_parameters.json',
            'generation_scripts.zip', 'generation_source_manifest.json', 'spherical_eyes_validation.json',
            'default_rebuild_validation.json',
            'reference_comparison/reference_review.md', 'reference_comparison/reference_review.json']
required += [path for _, path in sections]
for c in ('A', 'B'):
    required += [f'candidate_{c}/{name}' for name in ('Naiwa_Character.blend', 'Naiwa_Character.fbx', 'validation_report.json', 'fbx_validation.json', 'rebuild_validation.json', 'sphere_center_sections.json', 'spherical_eye_build_report.json')]
assert all((OUT / p).is_file() and (OUT / p).stat().st_size > 0 for p in required)
protocol = read_json(OUT / 'comparisons/camera_protocol.json')
actual = {'original': digest(BASE), **{c: validation_records[c]['blend_sha256'] for c in ('A', 'B')}}
assert protocol['source_hashes'] == actual, 'Three-view renders do not match current models'
assert len(protocol['files']) == 39
for record in protocol['files']:
    assert (OUT / 'comparisons' / record['file']).is_file()
with zipfile.ZipFile(OUT / 'generation_scripts.zip') as z:
    assert z.testzip() is None
    assert all(hashlib.sha256(z.read(name)).hexdigest() == sha for name, sha in source_manifest.items())
for target in re.findall(r'(?:href|src)="([^"]+)"', document):
    if not target.startswith('#'):
        assert target == 'delivery_validation.json' or target == 'SHA256SUMS.json' or (OUT / target).is_file(), target
assert digest(BASE) == BASE_SHA
checks = {'status': 'passed', 'source_unchanged': True, 'source_sha256': BASE_SHA,
          'required_files': required, 'original_comparison_png_count': 39,
          'comparison_source_hashes_exact': True, 'source_snapshot_zip_verified': True,
          'candidates': validation_records}
(OUT / 'delivery_validation.json').write_text(json.dumps(checks, indent=2), encoding='utf-8')
excluded_dirs = {'rebuild_temp'}
manifest = {str(p.relative_to(OUT)).replace('\\', '/'): digest(p)
            for p in sorted(OUT.rglob('*')) if p.is_file() and p.name != 'SHA256SUMS.json'
            and not any(x in excluded_dirs for x in p.relative_to(OUT).parts)}
(OUT / 'SHA256SUMS.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
print('SPHERICAL_EYE_DELIVERY_PACKAGED', len(required), len(manifest))
