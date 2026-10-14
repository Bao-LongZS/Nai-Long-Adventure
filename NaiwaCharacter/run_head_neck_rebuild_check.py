"""Exercise the default generator and independently compare every saved candidate."""
import hashlib
import json
from pathlib import Path
import runpy
import sys

ROOT = Path(__file__).resolve().parent
OUT = ROOT/'versions/20261006_head_neck_refinement'
TEMP = OUT/'rebuild_temp'
SOURCE = ROOT/'versions/20261006_spherical_eyes/candidate_A/Naiwa_Character.blend'
SOURCE_SHA = 'f251f5a0fc00fedf1df76fcc6286bc40e102406a536cb6c676a93e176cb71ca0'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


selection = json.loads((OUT/'selection.json').read_text(encoding='utf-8'))
parent = Path(selection['neck_input']).resolve()
assert digest(SOURCE) == SOURCE_SHA
assert digest(parent) == selection['neck_sha256']
TEMP.mkdir(exist_ok=True)
sys.argv = ['blender', '--', '--output', str(TEMP)]
try:
    runpy.run_path(str(ROOT/'build_naiwa.py'), run_name='__main__')
except SystemExit as exception:
    assert exception.code in (None, 0), exception.code
reports = {}
for candidate in ('N1', 'N2', 'C1'):
    target = OUT/f'candidate_{candidate}/Naiwa_Character.blend'
    rebuilt = TEMP/f'candidate_{candidate}/Naiwa_Character.blend'
    output = target.parent/'rebuild_validation.json'
    sys.argv = ['blender', '--', '--target', str(target), '--rebuild', str(rebuilt), '--output', str(output)]
    runpy.run_path(str(ROOT/'validate_delivery_rebuild.py'), run_name='__main__')
    reports[candidate] = json.loads(output.read_text(encoding='utf-8'))
recommended = selection['recommended_candidate']
rebuilt_fbx = TEMP/f'candidate_{recommended}/Naiwa_Character.fbx'
assert rebuilt_fbx.is_file(), 'Default build did not export the recommended FBX'
sys.argv = ['blender', '--', '--fbx', str(rebuilt_fbx)]
runpy.run_path(str(ROOT/'validate_fbx.py'), run_name='__main__')
assert digest(SOURCE) == SOURCE_SHA and digest(parent) == selection['neck_sha256']
summary = {'passed': True, 'entry': 'build_naiwa.py default branch',
           'from_primitives': False, 'immutable_source_sha256': SOURCE_SHA,
           'source_unchanged': True, 'selected_neck_sha256': selection['neck_sha256'],
           'selected_neck_unchanged': True, 'candidates': reports,
           'recommended_candidate': recommended, 'recommended_rebuilt_fbx_reimport_passed': True,
           'recommended_rebuilt_fbx_sha256': digest(rebuilt_fbx)}
(OUT/'default_rebuild_validation.json').write_text(json.dumps(summary, indent=2)+'\n', encoding='utf-8')
print('HEAD_NECK_DEFAULT_REBUILD_MATCH_OK', flush=True)
