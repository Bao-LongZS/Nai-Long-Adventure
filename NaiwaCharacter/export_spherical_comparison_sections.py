import bpy,sys,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from geometry_diagnostic_utils import mesh_record,section_object
OUT=ROOT/'versions/20261006_spherical_eyes'
files={'original':ROOT/'versions/20261005_head_side_continuity/Naiwa_Character.blend',
    'A':OUT/'candidate_A/Naiwa_Character.blend','B':OUT/'candidate_B/Naiwa_Character.blend'}
data={};x=.11731289860933969
for label,path in files.items():
    bpy.ops.wm.open_mainfile(filepath=str(path));rig=bpy.data.objects['Naiwa_Rig']
    rig.animation_data.action=bpy.data.actions['Naiwa_Idle'];rig.animation_data.use_nla=False
    bpy.context.scene.frame_set(1);bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get()
    eye=mesh_record(bpy.data.objects['R concentric iris and pupil'],dg,eye=True)
    body=mesh_record(bpy.data.objects['Naiwa unified golden body'],dg)
    data[label]={'plane_X':x,'eye':section_object(eye,0,x,select_head=False),'body':section_object(body,0,x)}
(OUT/'comparisons/eye_side_sections_raw.json').write_text(json.dumps(data,separators=(',',':')),encoding='utf-8')
print('SPHERICAL_COMPARISON_SECTIONS_OK')
