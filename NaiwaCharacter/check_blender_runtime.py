"""Read-only native-project and generator syntax check after task recovery."""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import bpy
import numpy as np

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--blend', type=Path, default=ROOT / 'versions/20261005_eye_muzzle_body_delivery/Naiwa_Character.blend')
parser.add_argument('--report', type=Path, default=ROOT / 'runtime_recovery_check.json')
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
source = args.blend.resolve()
digest_before = hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(source))
body = bpy.data.objects['Naiwa unified golden body']
rigs = [obj for obj in bpy.data.objects if obj.type == 'ARMATURE']
assert len(rigs) == 1
meshes = [obj for obj in bpy.data.objects if obj.type == 'MESH'
          and any(mod.type == 'ARMATURE' for mod in obj.modifiers)]
assert len(meshes) == 6
assert len(rigs[0].data.bones) == 10
expected = {'Naiwa_Idle', 'Naiwa_Walk', 'Naiwa_Jump', 'Naiwa_Laugh'}
assert expected.issubset(set(bpy.data.actions.keys()))
points = np.empty(len(body.data.vertices) * 3, dtype=np.float32)
body.data.vertices.foreach_get('co', points)
assert np.isfinite(points).all()
for side in ('L', 'R'):
    eye = bpy.data.objects[f'{side} concentric iris and pupil']
    assert eye.data.color_attributes.get('EyeColor') is not None
scripts = sorted(ROOT.glob('*.py'))
for path in scripts:
    compile(path.read_text(encoding='utf-8-sig'), str(path), 'exec')
digest_after = hashlib.sha256(source.read_bytes()).hexdigest()
assert digest_before == digest_after, 'The source project must remain unchanged'
report = {
    'status': 'passed',
    'blender_version': bpy.app.version_string,
    'source_blend': str(source),
    'source_sha256': digest_after,
    'source_unchanged': True,
    'bound_meshes': len(meshes),
    'bones': len(rigs[0].data.bones),
    'actions': sorted(expected),
    'body_vertices': len(body.data.vertices),
    'generator_scripts_syntax_checked': len(scripts),
    'scope': 'Project open, structure, finite coordinates, eye colors and Python syntax; no geometry was edited or saved.',
}
args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print('BLENDER_RUNTIME_RECOVERY_OK', len(meshes), 'meshes', len(scripts), 'scripts')
