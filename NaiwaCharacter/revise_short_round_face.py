"""Create the reviewed short-face revision from its preceding scene."""
import bpy,sys,json,math
from pathlib import Path
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT))
from short_round_face_sculpt import sculpt_short_round_face
from render_character_delivery import deliver_character
from render_surface_diagnostics import render_neutral_views
bpy.ops.wm.open_mainfile(filepath=str(OUT/'iterations/before_short_round_face_revision/Naiwa_Character.blend'))
report=sculpt_short_round_face(bpy.data.objects['Naiwa unified golden body'],bpy.data.objects.get('Head and torso silhouette only'))
(OUT/'short_round_face_build_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
deliver_character(OUT,revision_tag='SHORT_ROUND_FACE',soft_lighting=True)
render_neutral_views(OUT,'naiwa_surface_gray')
camera_report={'projection':'ORTHO','lens_mm':50,'distance':7,
               'full_body_scale':3.45,'neutral_close_scale':1.03,
               'views':{'front':0,'side':90,'three_quarter':35},
               'camera_elevation_degrees':0,'same_scene_lighting_for_each_triplet':True,
               'soft_lights':{'key_power':550,'fill_power':250,'area_size':4.5,'exposure':-0.4}}
(OUT/'short_round_face_camera_settings.json').write_text(json.dumps(camera_report,indent=2),encoding='utf-8')
print('SHORT_ROUND_FACE_REVISION_OK')
