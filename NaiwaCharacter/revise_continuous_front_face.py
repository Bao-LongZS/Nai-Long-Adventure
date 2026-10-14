"""Deliver the continuous foreface from the saved mouth/eye revision."""
import bpy,sys,json
from pathlib import Path
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT))
from continuous_front_face_sculpt import sculpt_continuous_front_face
from render_character_delivery import deliver_character
from render_surface_diagnostics import render_neutral_views
bpy.ops.wm.open_mainfile(filepath=str(OUT/'iterations/before_continuous_front_face/Naiwa_Character.blend'))
report=sculpt_continuous_front_face(bpy.data.objects['Naiwa unified golden body'],bpy.data.objects.get('Head and torso silhouette only'))
(OUT/'continuous_front_face_build_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
deliver_character(OUT,revision_tag='CONTINUOUS_FRONT_FACE',soft_lighting=True)
render_neutral_views(OUT,'naiwa_surface_gray')
render_neutral_views(OUT,'naiwa_review',neutral=False)
settings=json.loads((OUT/'iterations/before_continuous_front_face/lip_eye_camera_settings.json').read_text(encoding='utf-8'))
(OUT/'continuous_front_face_camera_settings.json').write_text(json.dumps(settings,indent=2),encoding='utf-8')
print('CONTINUOUS_FRONT_FACE_REVISION_OK')
