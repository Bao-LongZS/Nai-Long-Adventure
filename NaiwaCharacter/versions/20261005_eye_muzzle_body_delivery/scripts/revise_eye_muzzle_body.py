"""Versioned delivery, always from the immutable measured preceding scene."""
import bpy,sys,json,shutil,argparse,hashlib
from pathlib import Path
OUT=Path(__file__).resolve().parent;sys.path.insert(0,str(OUT))
from eye_muzzle_sculpt import sculpt_eye_muzzle,baseline_record
from body_finish_sculpt import finish_body
from render_character_delivery import deliver_character
from render_surface_diagnostics import render_neutral_views
from render_eye_muzzle_comparison import render_six
BASE=OUT/'iterations/before_eye_muzzle_body_revision/Naiwa_Character.blend'
parser=argparse.ArgumentParser()
parser.add_argument('--output',type=Path,default=OUT/'versions/20261005_eye_muzzle_body_delivery')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
VERSION=args.output.resolve();VERSION.mkdir(parents=True,exist_ok=True)
assert VERSION != BASE.parent.resolve(),'Never overwrite the immutable measured baseline.'
record_path=BASE.parent/'baseline_dimensions.json'
rec=json.loads(record_path.read_text(encoding='utf8'))
rec['D_definition']='Equivalent circular diameter enclosed by the green outer boundary; pupil size does not enter the diameter measurement.'
rec['baseline_blend_sha256']=hashlib.sha256(BASE.read_bytes()).hexdigest()
(VERSION/'baseline_dimensions.json').write_text(json.dumps(rec,ensure_ascii=False,indent=2),encoding='utf8')
bpy.ops.wm.open_mainfile(filepath=str(BASE))
body=bpy.data.objects['Naiwa unified golden body'];shell=bpy.data.objects.get('Head and torso silhouette only')
report=sculpt_eye_muzzle(body,shell,rec,stage_directory=VERSION)
report['body_finish']=finish_body(body,shell,rec['height'],rec['D'],rec)
(VERSION/'eye_muzzle_build_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
deliver_character(VERSION,revision_tag='EYE_MUZZLE_BODY',soft_lighting=True)
render_neutral_views(VERSION,'naiwa_surface_gray')
render_neutral_views(VERSION,'naiwa_review',neutral=False)
render_six(VERSION,'after')
bpy.ops.wm.open_mainfile(filepath=str(BASE));render_six(VERSION,'before')
# Leave the new project open in the process, and save an editable full-body
# orthographic view rather than a before-render or material-override state.
bpy.ops.wm.open_mainfile(filepath=str(VERSION/'Naiwa_Character.blend'))
print('EYE_MUZZLE_BODY_REVISION_OK')
