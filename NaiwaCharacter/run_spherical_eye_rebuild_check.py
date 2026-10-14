"""Exercise the updated default entry then independently compare saved A/B."""
import runpy,sys,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parent
OUT=ROOT/'versions/20261006_spherical_eyes'
TEMP=OUT/'rebuild_temp';TEMP.mkdir(exist_ok=True)
SOURCE=ROOT/'versions/20261005_head_side_continuity/Naiwa_Character.blend'
SHA='a95a5c7efb25cb6f379897dd6ec4af98a201bc2619dc8caa2bb5b5de8ea3dfd5'
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==SHA
sys.argv=['blender','--','--output',str(TEMP)]
try:runpy.run_path(str(ROOT/'build_naiwa.py'),run_name='__main__')
except SystemExit as e:
    assert e.code in (0,None),e.code
for candidate in ('A','B'):
    sys.argv=['blender','--','--target',str(OUT/('candidate_'+candidate)/'Naiwa_Character.blend'),
        '--rebuild',str(TEMP/('candidate_'+candidate)/'Naiwa_Character.blend'),
        '--output',str(OUT/('candidate_'+candidate)/'rebuild_validation.json')]
    runpy.run_path(str(ROOT/'validate_delivery_rebuild.py'),run_name='__main__')
summary={'passed':True,'entry':'build_naiwa.py default branch',
    'from_primitives':False,'immutable_source_sha256':SHA,
    'source_unchanged':hashlib.sha256(SOURCE.read_bytes()).hexdigest()==SHA,
    'candidates':{candidate:json.loads((OUT/('candidate_'+candidate)/'rebuild_validation.json').read_text(encoding='utf-8'))
                  for candidate in ('A','B')}}
assert summary['source_unchanged']
(OUT/'default_rebuild_validation.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
print('SPHERICAL_DEFAULT_REBUILD_MATCH_OK')
