import bpy,sys,json
from pathlib import Path
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT))
from continuous_front_face_sculpt import sculpt_continuous_front_face
from render_surface_diagnostics import render_neutral_views
DRAFT=OUT/'iterations/continuous_front_face_draft'
DRAFT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(OUT/'iterations/before_continuous_front_face/Naiwa_Character.blend'))
report=sculpt_continuous_front_face(bpy.data.objects['Naiwa unified golden body'],bpy.data.objects.get('Head and torso silhouette only'))
bpy.ops.wm.save_as_mainfile(filepath=str(DRAFT/'Naiwa_Character.blend'))
(DRAFT/'draft_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
render_neutral_views(DRAFT,'after_gray')
render_neutral_views(DRAFT,'after',neutral=False)
print('CONTINUOUS_FRONT_FACE_DRAFT_OK')
