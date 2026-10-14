"""Original/A/B fixed-camera colored, gray, eye-close and full-body renders."""
import bpy,sys,json,math,hashlib
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from render_head_reshape_comparison import prepare_scene,gray_override
BASE=ROOT/'versions/20261005_head_side_continuity/Naiwa_Character.blend'
OUT=ROOT/'versions/20261006_spherical_eyes';DEST=OUT/'comparisons';DEST.mkdir(exist_ok=True)
files={'original':BASE,'A':OUT/'candidate_A/Naiwa_Character.blend','B':OUT/'candidate_B/Naiwa_Character.blend'}
hashes={k:hashlib.sha256(p.read_bytes()).hexdigest() for k,p in files.items()}
views=[('front',0),('three_quarter',45),('side',90)]
specs={'head':{'target':[0,-.12,2.37],'scale':.85,'resolution':[900,1000]},
    'eye':{'target':[.1173129,-.39,2.5053276],'scale':.295,'resolution':[800,800]},
    'full':{'target':[0,-.12,1.31],'scale':3.45,'resolution':[900,1000]}}
protocol=[]
def render(scene,label,kind,view,angle,gray=False):
    spec=specs[kind];target=Vector(spec['target']);a=math.radians(angle)
    cam=scene.camera;cam.data.type='ORTHO';cam.data.ortho_scale=spec['scale']
    cam.location=target+7*Vector((math.sin(a),-math.cos(a),0))
    cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
    scene.render.resolution_x,scene.render.resolution_y=spec['resolution']
    name=f'{label}_{kind}_{"gray_" if gray else ""}{view}.png'
    scene.render.filepath=str(DEST/name);bpy.context.view_layer.update();bpy.ops.render.render(write_still=True)
    protocol.append({'file':name,'model':label,'kind':kind,'gray':gray,'yaw':angle,'pitch':0,
        'target':list(target),'location':list(cam.location),'ortho_scale':spec['scale'],'resolution':spec['resolution']})
for label,path in files.items():
    bpy.ops.wm.open_mainfile(filepath=str(path));scene,engine=prepare_scene()
    for kind in ('head','eye'):
        for view,angle in views:render(scene,label,kind,view,angle)
    render(scene,label,'full','front',0)
    with gray_override(scene):
        for kind in ('head','eye'):
            for view,angle in views:render(scene,label,kind,view,angle,True)
    print('SPHERE_COMPARE_MODEL_COMPLETE',label,flush=True)
assert all(hashlib.sha256(p.read_bytes()).hexdigest()==hashes[k] for k,p in files.items())
(DEST/'camera_protocol.json').write_text(json.dumps({'engine':engine,'view_transform':'Standard','exposure':-.4,
    'same_camera_light_exposure_per_comparison':True,'source_hashes':hashes,'files':protocol},indent=2),encoding='utf-8')
print('SPHERICAL_EYE_COMPARISON_OK')
