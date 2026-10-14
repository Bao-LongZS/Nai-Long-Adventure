"""Generate neck candidates from fixed A; C1 from a hash-pinned selected neck."""
import argparse
import hashlib
import json
from pathlib import Path
import runpy
import sys
import bpy
import numpy as np

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from head_neck_refinement_sculpt import refine, parameters
BASE = ROOT/'versions/20261006_spherical_eyes/candidate_A/Naiwa_Character.blend'
BASE_SHA = 'f251f5a0fc00fedf1df76fcc6286bc40e102406a536cb6c676a93e176cb71ca0'
DELIVERY = ROOT/'versions/20261006_head_neck_refinement'
parser = argparse.ArgumentParser()
parser.add_argument('--candidate', choices=['neck', 'N1', 'N2', 'C1', 'all'], default='neck')
parser.add_argument('--output', type=Path, default=DELIVERY/'reproduced')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
out = args.output.resolve()
assert out != BASE.parent.resolve()
out.mkdir(parents=True, exist_ok=True)
DELIVERY.mkdir(parents=True, exist_ok=True)
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
assert sha(BASE) == BASE_SHA
selection = None
selection_file = DELIVERY/'selection.json'
if selection_file.is_file():
    selection = json.loads(selection_file.read_text(encoding='utf-8'))
if args.candidate in ('C1', 'all'):
    assert selection is not None, 'Select and hash-pin a saved neck candidate before C1.'
neck = ['N1', 'N2'] if args.candidate in ('neck', 'all') else ([args.candidate] if args.candidate in ('N1', 'N2') else [])
for name in neck:
    if selection and (out/('candidate_'+name)/'Naiwa_Character.blend').resolve() == Path(selection['neck_input']).resolve():
        raise RuntimeError('Output would overwrite the fixed C1 parent. Use --output with a new directory.')
candidates = neck + (['C1'] if args.candidate in ('C1', 'all') else [])
for candidate in candidates:
    source = Path(selection['neck_input']).resolve() if candidate == 'C1' else BASE
    expected = selection['neck_sha256'] if candidate == 'C1' else BASE_SHA
    assert sha(source) == expected, ('Fixed input hash differs', candidate)
    dest = out/('candidate_'+candidate)
    assert (dest/'Naiwa_Character.blend').resolve() != source.resolve()
    dest.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(source))
    rig = bpy.data.objects['Naiwa_Rig']
    rig.animation_data.action = bpy.data.actions['Naiwa_Idle']
    rig.animation_data.use_nla = False
    rig.data.pose_position = 'POSE'
    bpy.context.scene.frame_set(1)
    bpy.context.view_layer.update()
    report, mask = refine(candidate)
    report.update(source=str(source), source_sha256=expected, fixed_A=str(BASE), fixed_A_sha256=BASE_SHA)
    bpy.context.preferences.filepaths.save_version = 0
    for layer in bpy.context.scene.view_layers:
        layer.material_override = None
    bpy.ops.wm.save_as_mainfile(filepath=str(dest/'Naiwa_Character.blend'), compress=True)
    report['saved_blend_sha256'] = sha(dest/'Naiwa_Character.blend')
    (dest/'build_report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    np.savez_compressed(dest/'deformation_mask.npz', **mask)
    assert sha(source) == expected
    print('HEAD_NECK_CANDIDATE_SAVED', candidate, report['surfaces']['Naiwa unified golden body']['maximum_displacement'], flush=True)
recommended = selection.get('recommended_candidate') if selection else None
if recommended in candidates and (selection.get('scope') == 'neck_only' or selection.get('c1_decision') in ('adopt', 'retain_neck')):
    saved_args = sys.argv[:]
    try:
        sys.argv = ['blender', '--', '--blend', str(out/('candidate_'+recommended)/'Naiwa_Character.blend'),
                    '--output', str(out/('candidate_'+recommended)/'Naiwa_Character.fbx'), '--overwrite']
        runpy.run_path(str(ROOT/'export_saved_fbx.py'), run_name='__main__')
    finally:
        sys.argv = saved_args
saved_parameters = parameters()
saved_parameters.update(executed_candidates=candidates,
                        scope='neck_only' if args.candidate in ('neck', 'N1', 'N2') else 'neck_and_crown',
                        fixed_A=str(BASE), fixed_A_sha256=BASE_SHA)
(out/'parameters.json').write_text(json.dumps(saved_parameters, indent=2), encoding='utf-8')
assert sha(BASE) == BASE_SHA
print('HEAD_NECK_REFINEMENT_BUILD_OK', flush=True)
