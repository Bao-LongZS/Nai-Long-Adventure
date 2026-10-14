import bpy,sys,json
from pathlib import Path
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT))
from short_round_face_sculpt import sculpt_short_round_face
from render_surface_diagnostics import render_neutral_views
BASE=OUT/'iterations/before_short_round_face_revision/Naiwa_Character.blend'
DRAFT=OUT/'iterations/short_round_face_draft'
DRAFT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(BASE))
render_neutral_views(DRAFT,'before')
bpy.ops.wm.open_mainfile(filepath=str(BASE))
report=sculpt_short_round_face(bpy.data.objects['Naiwa unified golden body'],bpy.data.objects.get('Head and torso silhouette only'))
(DRAFT/'draft_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(DRAFT/'Naiwa_Character.blend'))
render_neutral_views(DRAFT,'after')
print('SHORT_ROUND_FACE_DRAFT_OK')
