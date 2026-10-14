"""Material-only gaze variants and common reference camera, sphere unchanged."""
import bpy,sys,json,math,hashlib
from pathlib import Path
import numpy as np
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from spherical_eye_sculpt import ProjectionCache,TARGETS
from geometry_diagnostic_utils import mesh_record
from spherical_reference_projection import axes,project,eye_pixels
from render_head_reshape_comparison import prepare_scene
OUT=ROOT/'versions/20261006_spherical_eyes';DEST=OUT/'reference_comparison';DEST.mkdir(exist_ok=True)
POSE={'type':'ORTHO','yaw':50.,'pitch':-2.5,'target':[0,-.12,2.425],'distance':7.,'scale':.86,'lens':50.}
RES=900;records=[]
paths={'original':ROOT/'versions/20261005_head_side_continuity/Naiwa_Character.blend',
    'A':OUT/'candidate_A/Naiwa_Character.blend','B':OUT/'candidate_B/Naiwa_Character.blend'}

def setup(scene,pose):
    back,_,_=axes(pose['yaw'],pose['pitch']);cam=scene.camera;target=Vector(pose['target'])
    cam.data.type=pose['type'];cam.data.ortho_scale=pose['scale'];cam.data.lens=pose['lens'];cam.data.sensor_width=36;cam.data.sensor_fit='HORIZONTAL'
    cam.location=target+pose['distance']*Vector(back);cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
    scene.render.resolution_x=scene.render.resolution_y=RES;bpy.context.view_layer.update()

def recolor(obj,C,R,gaze,angle):
    # Scalar values are attached to mesh corners and follow animation; a shader
    # threshold gives a smooth cap boundary without moving any sphere vertices.
    name='EyePupilCos';attr=obj.data.attributes.get(name) or obj.data.attributes.new(name=name,type='FLOAT',domain='CORNER')
    values=np.array([float(np.dot((np.array(obj.data.vertices[l.vertex_index].co)-C)/R,gaze)) for l in obj.data.loops],np.float32)
    attr.data.foreach_set('value',values)
    mat=obj.data.materials[0];nodes=mat.node_tree.nodes;links=mat.node_tree.links
    shader=next(n for n in nodes if n.type=='BSDF_PRINCIPLED')
    attribute=nodes.get('Spherical gaze cosine') or nodes.new('ShaderNodeAttribute');attribute.name='Spherical gaze cosine';attribute.attribute_name=name
    test=nodes.get('Spherical pupil threshold') or nodes.new('ShaderNodeMath');test.name='Spherical pupil threshold';test.operation='GREATER_THAN';test.inputs[1].default_value=math.cos(angle)
    mix=nodes.get('Spherical gaze colors') or nodes.new('ShaderNodeMixRGB');mix.name='Spherical gaze colors';mix.blend_type='MIX';mix.inputs[1].default_value=(.105,.265,.040,1);mix.inputs[2].default_value=(.008,.010,.006,1)
    links.new(attribute.outputs['Fac'],test.inputs[0]);links.new(test.outputs[0],mix.inputs[0]);links.new(mix.outputs[0],shader.inputs['Base Color'])
    # Glint follows its own spherical contact anchor inside the pupil.
    u=np.array([1.,0.,0.]);u-=gaze*np.dot(u,gaze);u/=np.linalg.norm(u);v=np.cross(gaze,u)
    normal=gaze-.09*.117244812742/R*u+.13*.117244812742/R*v;normal/=np.linalg.norm(normal)
    glint=bpy.data.objects[obj.name[0]+' eye glint'];glint.location=C+(R+.00025)*normal;glint.rotation_euler=Vector(normal).to_track_quat('Y','Z').to_euler()

for label,path in paths.items():
    digest=hashlib.sha256(path.read_bytes()).hexdigest();bpy.ops.wm.open_mainfile(filepath=str(path));scene,engine=prepare_scene()
    dg=bpy.context.evaluated_depsgraph_get();body=mesh_record(bpy.data.objects['Naiwa unified golden body'],dg)
    eyes={s:mesh_record(bpy.data.objects[s+' concentric iris and pupil'],dg,eye=True) for s in ('L','R')}
    hashes={s:hashlib.sha256(eyes[s]['points'].tobytes()).hexdigest() for s in ('L','R')}
    for projection in ('ORTHO','PERSP'):
        pose=dict(POSE)
        if projection=='PERSP':pose.update(type='PERSP',lens=135.,distance=.86*135/36)
        setup(scene,pose);name=f'{label}_common_{projection}.png';scene.render.filepath=str(DEST/name);bpy.ops.render.render(write_still=True)
        records.append({'model':label,'gaze_degrees':0,'filename':name,'pose':pose,
            'eyes':{s:eye_pixels(eyes[s],body,pose,RES,2) for s in ('L','R')}})
    if label!='original':
        params=json.loads((path.parent/'spherical_eye_build_report.json').read_text())['eyes']
        caches={s:ProjectionCache(body,TARGETS[s]) for s in ('L','R')}
        for angle_degrees in (20,35):
            actual={};setup(scene,POSE)
            for side,sign in [('L',-1),('R',1)]:
                par=params[side];C=np.array(par['sphere_center']);R=par['R'];a=math.radians(angle_degrees)
                gaze=np.array([sign*math.sin(a),-math.cos(a),0.]);lo,hi=.005,math.radians(85)
                for _ in range(25):
                    cap=(lo+hi)/2
                    if caches[side].metrics(C,R,cap,gaze)['pupil_ratio']<.53:lo=cap
                    else:hi=cap
                cap=(lo+hi)/2;obj=bpy.data.objects[side+' concentric iris and pupil'];recolor(obj,C,R,gaze,cap)
                actual[side]={'gaze_axis':gaze.tolist(),'pupil_cap_half_angle_degrees':math.degrees(cap),
                    'analytic_frontal_pupil_ratio':caches[side].metrics(C,R,cap,gaze)['pupil_ratio'],
                    'reference_pixels':eye_pixels(eyes[side],body,POSE,RES,2,gaze,cap,C,R)}
            name=f'{label}_gaze{angle_degrees}_common_ORTHO.png';scene.render.filepath=str(DEST/name);bpy.context.view_layer.update();bpy.ops.render.render(write_still=True)
            # Include a front view to expose the actual changed gaze direction.
            frontal=dict(POSE,yaw=0,pitch=0);setup(scene,frontal)
            front=f'{label}_gaze{angle_degrees}_front.png';scene.render.filepath=str(DEST/front);bpy.ops.render.render(write_still=True)
            records.append({'model':label,'gaze_degrees':angle_degrees,'filename':name,'front_filename':front,'pose':POSE,
                'eyes':actual,'geometry_unchanged':True,'saved_as_model':False,
                'color_method':'Temporary attached EyePupilCos corner attribute + shader threshold, no sphere displacement'})
        for s in ('L','R'):
            check=mesh_record(bpy.data.objects[s+' concentric iris and pupil'],bpy.context.evaluated_depsgraph_get(),eye=True)
            assert hashlib.sha256(check['points'].tobytes()).hexdigest()==hashes[s]
    assert hashlib.sha256(path.read_bytes()).hexdigest()==digest
(DEST/'reference_and_gaze_protocol.json').write_text(json.dumps({'common_pose':POSE,'resolution':[RES,RES],
    'all_body_geometry_unchanged':True,'gaze_variants_material_only':True,'base_saved_gaze':'F=-Y, 0 degrees',
    'view_transform':'Standard','exposure':-.4,'records':records},indent=2),encoding='utf-8')
print('SPHERICAL_GAZE_REFERENCE_OK')
