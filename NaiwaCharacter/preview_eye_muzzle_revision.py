import bpy,sys,json
from pathlib import Path
OUT=Path(__file__).resolve().parent;sys.path.insert(0,str(OUT))
from eye_muzzle_sculpt import sculpt_eye_muzzle,baseline_record
from body_finish_sculpt import finish_body
from render_eye_muzzle_comparison import render_six
BASE=OUT/'iterations/before_eye_muzzle_body_revision/Naiwa_Character.blend'
DRAFT=OUT/'iterations/eye_muzzle_body_draft';DRAFT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(BASE))
rec=baseline_record()
body=bpy.data.objects['Naiwa unified golden body'];shell=bpy.data.objects.get('Head and torso silhouette only')
report=sculpt_eye_muzzle(body,shell,rec)
bpy.ops.wm.save_as_mainfile(filepath=str(DRAFT/'head_stage.blend'))
render_six(DRAFT,'head_stage',('head',))
report['body_finish']=finish_body(body,shell,rec['height'],rec['D'],rec)
bpy.ops.wm.save_as_mainfile(filepath=str(DRAFT/'Naiwa_Character.blend'))
(DRAFT/'draft_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
render_six(DRAFT,'after')
bpy.ops.wm.open_mainfile(filepath=str(BASE));render_six(DRAFT,'before')
print('EYE_MUZZLE_DRAFT_OK')
