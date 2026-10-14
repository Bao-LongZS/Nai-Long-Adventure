"""Read-only camera search for PDF Fig. 1.3. Never saves or exports geometry."""
import argparse, hashlib, json, math, sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from render_head_reshape_comparison import prepare_scene

SOURCE=ROOT/'versions/20261005_head_side_continuity/Naiwa_Character.blend'
OUT=ROOT/'diagnostics/20261005_head_geometry'
OUT.mkdir(parents=True,exist_ok=True)
parser=argparse.ArgumentParser()
parser.add_argument('--yaw',type=int)
parser.add_argument('--pitch',type=int,default=0)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
stored=OUT/'reference_candidate_cameras.json'
records=json.loads(stored.read_text(encoding='utf-8'))['candidates'] if args.yaw is not None and stored.is_file() else []
digest=hashlib.sha256(SOURCE.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
scene,engine=prepare_scene()
scene.render.resolution_x=1000
scene.render.resolution_y=1000
scene.render.resolution_percentage=100
scene.render.film_transparent=False
target=Vector((0,-.12,2.425))
cam=scene.camera
cam.data.type='ORTHO'
cam.data.ortho_scale=.86
cam.data.clip_start=.001
body=bpy.data.objects['Naiwa unified golden body']
dg=bpy.context.evaluated_depsgraph_get()

def pixel(p):
    v=world_to_camera_view(scene,cam,p)
    return np.array((v.x*1000,(1-v.y)*1000))

def stats(rows,step):
    if len(rows)<3:return {'sample_count':len(rows),'visible':False}
    pts=np.array(rows,float)
    vals,vecs=np.linalg.eigh(np.cov(pts.T,bias=True))
    axes=4*np.sqrt(vals)
    spans=np.ptp(pts,axis=0)+step
    return {'visible':True,'sample_count':len(rows),'bbox_pixels':[pts.min(0).tolist(),pts.max(0).tolist()],
            'center_pixels':pts.mean(0).tolist(),'width_height_ratio':float(spans[0]/spans[1]),
            'principal_axis_ratio':float(axes[1]/axes[0]),
            'principal_major_axis_degrees_screen':float(math.degrees(math.atan2(vecs[1,1],vecs[0,1]))),
            'ellipse_axes_pixels':axes.tolist(),'equivalent_ellipse_diameter_pixels':float(math.sqrt(axes[0]*axes[1])),
            'equivalent_area_diameter_pixels':float(math.sqrt(4*len(rows)*step*step/math.pi))}

def eye_projection(side):
    src=bpy.data.objects[side+' concentric iris and pupil']
    eye=src.evaluated_get(dg)
    skin=body.evaluated_get(dg)
    cols=eye.data.color_attributes['EyeColor']
    pp=np.asarray([pixel(eye.matrix_world@v.co) for v in eye.data.vertices])
    low=np.floor(pp.min(0)/2)*2-2
    high=np.ceil(pp.max(0)/2)*2+2
    inv_eye=eye.matrix_world.inverted(); inv_skin=skin.matrix_world.inverted()
    direction=(cam.rotation_euler.to_matrix()@Vector((0,0,-1))).normalized()
    right=cam.rotation_euler.to_matrix()@Vector((1,0,0))
    up=cam.rotation_euler.to_matrix()@Vector((0,1,0))
    ed=(inv_eye.to_3x3()@direction).normalized(); bd=(inv_skin.to_3x3()@direction).normalized()
    rows=[]; black=[]
    step=2
    for x in np.arange(low[0],high[0]+1,step):
        for y in np.arange(low[1],high[1]+1,step):
            origin=cam.location+right*((x/1000-.5)*cam.data.ortho_scale)+up*((.5-y/1000)*cam.data.ortho_scale)
            eh,ep,en,fi=eye.ray_cast(inv_eye@origin,ed)
            if not eh:continue
            bh,bp,bn,bi=skin.ray_cast(inv_skin@origin,bd)
            ew=eye.matrix_world@ep
            if bh and (skin.matrix_world@bp-origin).dot(direction)<(ew-origin).dot(direction)-.0000005:continue
            row=(float(x),float(y)); rows.append(row)
            poly=eye.data.polygons[fi]
            if cols.data[poly.loop_start].color[1]<.04:black.append(row)
    return {'aperture':stats(rows,step),'pupil':stats(black,step),'sampling_step_pixels':step,
            'method':'orthographic visible ray projection against evaluated body; outer green boundary encloses pupil'}

for yaw,pitch in ([(args.yaw,args.pitch)] if args.yaw is not None else [(70,0),(80,0),(85,0)]):
    theta,phi=math.radians(yaw),math.radians(pitch)
    cam.location=target+7*Vector((math.sin(theta)*math.cos(phi),-math.cos(theta)*math.cos(phi),math.sin(phi)))
    cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
    bpy.context.view_layer.update()
    name=f'reference_camera_yaw{abs(yaw):02d}_pitch{pitch:02d}.png'
    scene.render.filepath=str(OUT/name)
    bpy.ops.render.render(write_still=True)
    rec={'filename':name,'yaw_from_front_F_degrees':yaw,'pitch_from_horizontal_degrees':pitch,'roll_degrees':0,
         'camera_type':cam.data.type,'ortho_scale':cam.data.ortho_scale,'target':list(target),'location':list(cam.location),
         'eyes':{side:eye_projection(side) for side in ('L','R')}}
    records=[r for r in records if (r['yaw_from_front_F_degrees'],r['pitch_from_horizontal_degrees'])!=(yaw,pitch)]+[rec]
    print('REFERENCE_CANDIDATE',yaw,json.dumps(rec['eyes']))
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==digest
(OUT/'reference_candidate_cameras.json').write_text(json.dumps({'source_blend':str(SOURCE),'source_sha256':digest,
    'source_unchanged':True,'no_blend_save':True,'engine':engine,'resolution':[1000,1000],
    'coordinate_definition':'F=-Y; yaw camera offset atan2(X,-Y), positive yaw observes positive-X side and character faces screen left; pitch elevation from horizontal; roll=0',
    'candidates':records},indent=2),encoding='utf-8')
print('REFERENCE_ALIGNMENT_CANDIDATES_OK')
