"""Arrange unaltered same-condition E2/B selected checks for visual review."""
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent
SCRIPT_ROOT = ROOT.parents[1]
sys.path.insert(0, str(SCRIPT_ROOT))
from assemble_c3_review import compose, digest, read_image

out = ROOT / 'review' / 'selected_panels'
out.mkdir(parents=True, exist_ok=True)
contract_sha = digest(ROOT / 'c3_render_contract.json')
for label in ('E2', 'B'):
    protocol = json.loads((ROOT / 'review/selected' / label / 'protocol.json').read_text(encoding='utf-8'))
    assert protocol['complete'] and protocol['contract_sha256'] == contract_sha
    for record in protocol['files']:
        assert digest(ROOT / 'review/selected' / label / record['file']) == record['sha256']

records = []
for mode, title in (('grazing_A', '掠射方向 A'), ('grazing_B', '掠射方向 B'), ('reflection', '条纹反射')):
    rows, inputs = [], []
    for view, caption in (('three_quarter', '45°'), ('side', '90°')):
        paths = [ROOT / 'review/selected' / label / f'{label}_{mode}_{view}.png' for label in ('E2', 'B')]
        inputs.extend(paths)
        rows.append((caption, [read_image(p) for p in paths]))
    records.append(compose(title + ' · E2 / B',
        '固定机位、曝光和诊断照明；原图统一显示缩放，无修图。', ['E2', 'B'], rows,
        out / f'comparison_{mode}.png', inputs))
(out / 'assembly_protocol.json').write_text(json.dumps({
    'contract_sha256': contract_sha, 'photometric_changes': False,
    'candidate_camera_refitted': False, 'files': records}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print('C3_SELECTED_PANELS_OK', str(out))
