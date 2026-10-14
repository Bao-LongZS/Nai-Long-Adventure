"""Deliver the reviewed lip/eye refinement from its saved baseline."""
import bpy,sys,json
from pathlib import Path
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT))
from lip_eye_sculpt import sculpt_lip_eye
from render_character_delivery import deliver_character
from render_surface_diagnostics import render_neutral_views
bpy.ops.wm.open_mainfile(filepath=str(OUT/'iterations/before_lip_eye_refinement/Naiwa_Character.blend'))
report=sculpt_lip_eye(bpy.data.objects['Naiwa unified golden body'],bpy.data.objects.get('Head and torso silhouette only'))
(OUT/'lip_eye_build_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
deliver_character(OUT,revision_tag='LIP_EYE',soft_lighting=True)
render_neutral_views(OUT,'naiwa_surface_gray')
render_neutral_views(OUT,'naiwa_review',neutral=False)
settings=json.loads((OUT/'iterations/before_lip_eye_refinement/broad_face_arc_camera_settings.json').read_text(encoding='utf-8'))
(OUT/'lip_eye_camera_settings.json').write_text(json.dumps(settings,indent=2),encoding='utf-8')
print('LIP_EYE_REVISION_OK')
