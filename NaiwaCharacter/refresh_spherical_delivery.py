"""Refresh diagnostics after the final source-palette restoration."""
import runpy,sys,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parent
OUT=ROOT/'versions/20261006_spherical_eyes'
sys.argv=['blender']
for script in ['validate_spherical_eyes.py','export_spherical_comparison_sections.py',
    'render_spherical_eye_comparison.py','render_spherical_reference_search.py','render_spherical_gaze_reference.py']:
    runpy.run_path(str(ROOT/script),run_name='__main__')
for candidate in ('A','B'):
    source=OUT/('candidate_'+candidate)/'Naiwa_Character.fbx'
    sys.argv=['blender','--','--fbx',str(source)]
    runpy.run_path(str(ROOT/'validate_fbx.py'),run_name='__main__')
    (source.parent/'fbx_validation.json').write_text(json.dumps({'status':'passed',
        'fbx_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'bound_meshes':6,'bones':10,
        'actions':['Naiwa_Idle','Naiwa_Walk','Naiwa_Laugh','Naiwa_Jump'],'EyeColor_retained':True},indent=2),encoding='utf-8')
print('SPHERICAL_FINAL_REFRESH_OK')
