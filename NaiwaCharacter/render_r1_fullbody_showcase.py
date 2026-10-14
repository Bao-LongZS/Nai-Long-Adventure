"""Read-only full-body front/side views of the delivered R1 character."""
import hashlib
import json
from pathlib import Path
import sys
import bpy

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from render_head_reshape_comparison import prepare_scene
from render_head_neck_comparisons import camera

OUT=ROOT/'versions/20261007_N2_round_refinement/showcase'
SOURCE=OUT.parent/'candidate_R1/Naiwa_Character.blend'
PINNED_SHA='ea12e9af2c564527d0c473cd31b28974339fcceb15b666502a33f0ffa057bc24'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
assert sha(SOURCE)==PINNED_SHA
OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(SOURCE),load_ui=False)
scene,engine=prepare_scene()
records=[]
for name,yaw in (('front',0.),('side',90.)):
    pose={'type':'ORTHO','yaw':yaw,'pitch':0.,'target':[0.,-.12,1.31],
        'distance':7.,'scale':3.05,'lens':50.,'resolution':[1100,1500],
        'sensor_fit':'VERTICAL'}
    camera(scene,pose)
    file=OUT/f'R1_full_{name}.png'
    scene.render.filepath=str(file)
    bpy.ops.render.render(write_still=True)
    records.append({'view':name,'pose':pose,'file':file.name,'sha256':sha(file)})
assert sha(SOURCE)==PINNED_SHA
report={'source':str(SOURCE),'source_sha256':PINNED_SHA,'source_unchanged':True,
    'animation':'Naiwa_Idle','frame':1,'engine':engine,
    'same_materials_lighting_exposure_and_scale':True,
    'view_transform':scene.view_settings.view_transform,'exposure':scene.view_settings.exposure,
    'views':records,'render_script_sha256':sha(Path(__file__))}
(OUT/'render_protocol.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('R1_FULLBODY_SHOWCASE_RENDER_OK',flush=True)
