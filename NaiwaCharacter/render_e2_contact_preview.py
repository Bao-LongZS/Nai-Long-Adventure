"""Read-only, same-camera H2/E2 contact previews; no model selection."""
import argparse,hashlib,json,sys
from pathlib import Path
import bpy
sys.path.insert(0,str(Path(__file__).resolve().parent))
from render_head_reshape_comparison import prepare_scene
from render_head_neck_comparisons import camera

ap=argparse.ArgumentParser()
ap.add_argument('--source',required=True,type=Path)
ap.add_argument('--candidate',required=True,type=Path)
ap.add_argument('--output',required=True,type=Path)
ap.add_argument('--resolution',type=int,default=600)
args=ap.parse_args(sys.argv[sys.argv.index('--')+1:])
args.output.mkdir(parents=True,exist_ok=True);records=[]
for label,path in (('H2',args.source),('E2',args.candidate)):
    path=path.resolve();digest=hashlib.sha256(path.read_bytes()).hexdigest()
    bpy.ops.wm.open_mainfile(filepath=str(path),load_ui=False)
    scene,engine=prepare_scene()
    for view,yaw in (('front',0.),('three_quarter',45.),('reference50',50.),('side',90.)):
        pose={'type':'ORTHO','yaw':yaw,'pitch':-2.5 if yaw==50. else 0.,
              'target':[0.,-.12,2.425 if yaw==50. else 2.37],
              'distance':7.,'scale':.86 if yaw==50. else .85,'lens':50.,
              'resolution':[args.resolution,args.resolution]}
        camera(scene,pose);filename=f'{label}_{view}.png'
        scene.render.filepath=str(args.output.resolve()/filename)
        bpy.ops.render.render(write_still=True)
        records.append({'label':label,'path':str(path),'sha256':digest,'filename':filename,
                        'engine':engine,'pose':pose})
        print('E2_CONTACT_PREVIEW',label,view,flush=True)
    assert hashlib.sha256(path.read_bytes()).hexdigest()==digest
(args.output/'preview_protocol.json').write_text(json.dumps({'read_only':True,
    'purpose':'diagnostic eye socket continuity; not an adopted candidate',
    'same_camera_and_lighting':True,'records':records},indent=2)+'\n',encoding='utf-8')
