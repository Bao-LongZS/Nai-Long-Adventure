"""Reproduce this refinement from its immutable preceding scene."""
import bpy, sys, json, math
from pathlib import Path
OUT = Path(__file__).resolve().parent
sys.path.insert(0, str(OUT))
from broad_face_arc_sculpt import sculpt_broad_face_arc
from render_character_delivery import deliver_character
from render_surface_diagnostics import render_neutral_views
bpy.ops.wm.open_mainfile(filepath=str(OUT/'iterations/before_broad_face_arc_refinement/Naiwa_Character.blend'))
report = sculpt_broad_face_arc(bpy.data.objects['Naiwa unified golden body'], bpy.data.objects.get('Head and torso silhouette only'))
(OUT/'broad_face_arc_build_report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
deliver_character(OUT, revision_tag='BROAD_FACE_ARC', soft_lighting=True)
render_neutral_views(OUT, 'naiwa_surface_gray')
render_neutral_views(OUT, 'naiwa_review', neutral=False)
camera_report = {'projection':'ORTHO', 'lens_mm':50, 'distance':7,
    'full_body_scale':3.45, 'neutral_close_scale':1.03,
    'views':{'front':0, 'side':90, 'three_quarter':35},
    'camera_elevation_degrees':0, 'same_scene_lighting_for_each_triplet':True,
    'soft_lights':{'key_power':550, 'fill_power':250, 'area_size':4.5, 'exposure':-.4}}
(OUT/'broad_face_arc_camera_settings.json').write_text(json.dumps(camera_report, indent=2), encoding='utf-8')
print('BROAD_FACE_ARC_REVISION_OK')
