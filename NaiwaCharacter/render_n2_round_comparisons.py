"""Read-only N2/R1 fixed renders and genuine reflection/grazing diagnostics.

blender --background --python-exit-code 1 --python this.py -- --models N2 R1
Reflection stripes are incident radiance in a temporary world, never a body texture.
"""
import argparse
from contextlib import contextmanager
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
import numpy as np
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from render_head_reshape_comparison import prepare_scene, gray_override
from render_head_neck_comparisons import camera, body_id, COMMON
from spherical_reference_projection import eye_pixels
from geometry_diagnostic_utils import mesh_record

BASE = ROOT/'versions/20261006_head_neck_refinement/candidate_N2/Naiwa_Character.blend'
BASE_HASH = '9397186f3ae13e93e05cc0d492c12ceb68b743ab4d2a9a0d650426a47a66b0cb'
OUT = ROOT/'versions/20261007_N2_round_refinement'
VIEWS = ('front','three_quarter','side')
STRIPES = {'type':'incident equirectangular world radiance; not a surface texture',
           'size':[1024,512],'bands':12,'white_fraction':.40,
           'white_radiance':3.0,'dark_radiance':.012,'azimuth_rotation_degrees':11.,
           'material_rgba':[.38,.38,.38,1.],'roughness':.20,'metallic':.70,
           'engine':'CYCLES','samples':32,'camera_background_rgba':[.055,.055,.055,1.]}
GRAZING = {'light_A':[4.,-2.0,2.34],'light_B':[-4.,2.0,2.34],
           'target':[0.,0.,2.34],'area_shape':'RECTANGLE','size':.08,'size_y':3.0,
           'power':500.,'world_rgba':[.10,.10,.10,1.],'world_strength':.35,
           'gray_roughness':1.0,'gray_specular':0.0}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def light_snapshot(scene):
    return [{'name':o.name,'type':o.data.type,'energy':o.data.energy,
             'color':list(o.data.color),'location':list(o.location),
             'rotation':list(o.rotation_euler),'hidden':o.hide_render,
             'shape':getattr(o.data,'shape',None),'size':getattr(o.data,'size',None),
             'size_y':getattr(o.data,'size_y',None)} for o in scene.objects if o.type=='LIGHT']


@contextmanager
def material_override(scene, material):
    layers = [(layer,layer.material_override) for layer in scene.view_layers]
    originals = {}
    slots=[]
    for obj in scene.objects:
        if obj.type in {'MESH','CURVE','SURFACE','FONT','META'} and not obj.hide_render:
            originals.setdefault(obj.data,tuple(obj.data.materials))
            slots.append((obj,tuple((s.link,s.material) for s in obj.material_slots)))
    try:
        for layer,_ in layers:
            layer.material_override=material
        for data,mats in originals.items():
            if mats:
                for i in range(len(mats)):
                    data.materials[i]=material
            else:
                data.materials.append(material)
        for obj,_ in slots:
            for slot in obj.material_slots:
                slot.material=material
        bpy.context.view_layer.update()
        yield
    finally:
        for data,mats in originals.items():
            if mats:
                for i,mat in enumerate(mats):
                    data.materials[i]=mat
            else:
                data.materials.clear()
        for obj,mats in slots:
            for slot,(link,mat) in zip(obj.material_slots,mats):
                slot.link,slot.material=link,mat
        for layer,mat in layers:
            layer.material_override=mat
        bpy.context.view_layer.update()


def flat_world(name,color,strength):
    world=bpy.data.worlds.new(name)
    world.use_nodes=True
    bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND')
    bg.inputs['Color'].default_value=color
    bg.inputs['Strength'].default_value=strength
    return world


def stripe_world():
    width,height=STRIPES['size']
    # Periodic long bright strips separated by dark strips. The map is radiance
    # on the surrounding world, and is sampled by reflected rays in Cycles.
    u=(np.arange(width,dtype=np.float64)+.5)/width
    phase=np.mod(u*STRIPES['bands']+STRIPES['azimuth_rotation_degrees']/360*STRIPES['bands'],1.)
    d=np.minimum(phase,1.-phase)
    half=STRIPES['white_fraction']/2
    feather=.015
    t=np.clip((half+feather-d)/(2*feather),0,1)
    t=t*t*(3-2*t)
    values=STRIPES['dark_radiance']+(STRIPES['white_radiance']-STRIPES['dark_radiance'])*t
    pixels=np.ones((height,width,4),dtype=np.float32)
    pixels[:,:,:3]=values[None,:,None]
    image=bpy.data.images.new('Temporary reflected world strip radiance',width=width,height=height,float_buffer=True)
    image.pixels.foreach_set(pixels.ravel())
    image.update()
    world=bpy.data.worlds.new('Temporary true reflection strip environment')
    world.use_nodes=True
    nodes=world.node_tree.nodes
    nodes.clear()
    env=nodes.new('ShaderNodeTexEnvironment')
    env.image=image
    env.interpolation='Linear'
    env.projection='EQUIRECTANGULAR'
    strip=nodes.new('ShaderNodeBackground')
    strip.inputs['Strength'].default_value=1.0
    camera_bg=nodes.new('ShaderNodeBackground')
    camera_bg.inputs['Color'].default_value=STRIPES['camera_background_rgba']
    camera_bg.inputs['Strength'].default_value=1.0
    rays=nodes.new('ShaderNodeLightPath')
    mix=nodes.new('ShaderNodeMixShader')
    output=nodes.new('ShaderNodeOutputWorld')
    links=world.node_tree.links
    links.new(env.outputs['Color'],strip.inputs['Color'])
    links.new(rays.outputs['Is Camera Ray'],mix.inputs[0])
    links.new(strip.outputs[0],mix.inputs[1])
    links.new(camera_bg.outputs[0],mix.inputs[2])
    links.new(mix.outputs[0],output.inputs['Surface'])
    return world,image,hashlib.sha256(pixels.tobytes()).hexdigest()


@contextmanager
def diagnostic_lighting(scene,kind,variant=None):
    old_world,old_engine=scene.world,scene.render.engine
    old_hidden=[(o,o.hide_render) for o in scene.objects]
    old_view=(scene.view_settings.view_transform,scene.view_settings.exposure,scene.view_settings.gamma)
    old_cycles=None
    if hasattr(scene,'cycles'):
        old_cycles=(scene.cycles.samples,scene.cycles.seed,scene.cycles.use_animated_seed,scene.cycles.device)
    created=[]
    image=None
    mat=None
    metadata={}
    try:
        for obj in scene.objects:
            if obj.type=='LIGHT' or obj.name=='Preview floor':
                obj.hide_render=True
        if kind=='reflection':
            world,image,pixel_hash=stripe_world()
            scene.world=world
            scene.render.engine='CYCLES'
            scene.cycles.samples=STRIPES['samples']
            scene.cycles.seed=0
            scene.cycles.use_animated_seed=False
            scene.cycles.device='CPU'
            mat=bpy.data.materials.new('Temporary neutral genuine reflection material')
            mat.use_nodes=True
            bs=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
            bs.inputs['Base Color'].default_value=STRIPES['material_rgba']
            bs.inputs['Roughness'].default_value=STRIPES['roughness']
            bs.inputs['Metallic'].default_value=STRIPES['metallic']
            spec=bs.inputs.get('Specular IOR Level') or bs.inputs.get('Specular')
            if spec is not None:
                spec.default_value=.5
            metadata=dict(STRIPES,environment_pixel_sha256=pixel_hash)
        else:
            world=flat_world('Temporary grazing light world',GRAZING['world_rgba'],GRAZING['world_strength'])
            scene.world=world
            data=bpy.data.lights.new('Temporary grazing strip '+variant,'AREA')
            light=bpy.data.objects.new('Temporary grazing strip '+variant,data)
            scene.collection.objects.link(light)
            created.append(light)
            light.location=GRAZING['light_'+variant]
            light.rotation_euler=(Vector(GRAZING['target'])-light.location).to_track_quat('-Z','Y').to_euler()
            data.energy=GRAZING['power']
            data.shape=GRAZING['area_shape']
            data.size=GRAZING['size']
            data.size_y=GRAZING['size_y']
            metadata=dict(GRAZING,variant=variant,location=list(light.location))
        scene.view_settings.view_transform='Standard'
        scene.view_settings.exposure=-.4
        scene.view_settings.gamma=1.
        context=material_override(scene,mat) if mat is not None else gray_override(scene)
        with context:
            bpy.context.view_layer.update()
            yield metadata
    finally:
        scene.world=old_world
        scene.render.engine=old_engine
        scene.view_settings.view_transform,scene.view_settings.exposure,scene.view_settings.gamma=old_view
        if old_cycles:
            scene.cycles.samples,scene.cycles.seed,scene.cycles.use_animated_seed,scene.cycles.device=old_cycles
        for obj,hidden in old_hidden:
            obj.hide_render=hidden
        for obj in created:
            data=obj.data
            bpy.data.objects.remove(obj,do_unlink=True)
            bpy.data.lights.remove(data)
        if 'world' in locals():
            bpy.data.worlds.remove(world)
        if image:
            bpy.data.images.remove(image)
        if mat:
            bpy.data.materials.remove(mat)
        bpy.context.view_layer.update()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--models',nargs='+',choices=['N2','R1'],default=['N2','R1'])
    parser.add_argument('--skip-diagnostics',action='store_true',help='For provisional shape iteration only; final requires full diagnostics.')
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    cmp,ref=OUT/'comparisons',OUT/'reference_comparison'
    cmp.mkdir(parents=True,exist_ok=True)
    ref.mkdir(parents=True,exist_ok=True)
    old_protocol=ROOT/'versions/20261006_spherical_eyes/comparisons/camera_protocol.json'
    old_hash=digest(old_protocol)
    prior=json.loads(old_protocol.read_text(encoding='utf-8'))
    output_protocol=OUT/'render_protocol.json'
    protocol=json.loads(output_protocol.read_text(encoding='utf-8')) if output_protocol.exists() else {
        'fixed_baseline':str(BASE),'fixed_baseline_sha256':BASE_HASH,
        'source_camera_protocol':str(old_protocol),'source_camera_protocol_sha256':old_hash,
        'common_reference_camera':COMMON,
        'fixed_reference_registration':{'scale':.41175526202447044,'translation':[245.38813136509236,-17.159665982101984]},
        'animation':'Naiwa_Idle','frame':1,'saved_source_models':False,
        'sources':{},'files':[],'reflection_diagnostic':STRIPES,'grazing_diagnostic':GRAZING,
        'same_camera_light_exposure_per_comparison':True,
        'colored_gray_view_transform':'Standard','colored_gray_exposure':-.4,
        'body_ID_method':'Only evaluated unified body visible, white emission on black world.'}
    assert digest(BASE)==BASE_HASH
    for label in args.models:
        path=BASE if label=='N2' else OUT/'candidate_R1/Naiwa_Character.blend'
        before_hash=digest(path)
        bpy.ops.wm.open_mainfile(filepath=str(path),load_ui=False)
        scene,engine=prepare_scene()
        original_lights=light_snapshot(scene)
        bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
        normal_lighting={'lights':original_lights,'world_color':list(bg.inputs['Color'].default_value),
                         'world_strength':bg.inputs['Strength'].default_value}
        protocol['sources'][label]={'path':str(path),'sha256':before_hash,'standard_lighting':normal_lighting}
        protocol['files']=[r for r in protocol['files'] if r['model']!=label]

        def render(pose,dest,filename,mode,diagnostic=None):
            camera(scene,pose)
            target=dest/filename
            scene.render.filepath=str(target)
            bpy.ops.render.render(write_still=True)
            protocol['files'].append({'model':label,'file':str(target.relative_to(OUT)),
                'mode':mode,'pose':pose,'location':list(scene.camera.location),
                'source_sha256':before_hash,'engine':scene.render.engine,
                'diagnostic':diagnostic,'view_transform':scene.view_settings.view_transform,
                'exposure':scene.view_settings.exposure,'gamma':scene.view_settings.gamma,
                'kind':'full' if '_full_' in filename else ('head' if dest==cmp else 'reference'),
                'gray':mode=='gray','yaw':pose['yaw'],'pitch':pose['pitch'],
                'target':pose['target'],'ortho_scale':pose['scale'],'resolution':pose['resolution']})

        heads=[]
        for view in VIEWS:
            source=next(r for r in prior['files'] if r['model']=='A' and r['kind']=='head' and not r['gray'] and view in r['file'])
            pose={'type':'ORTHO','yaw':source['yaw'],'pitch':source['pitch'],'target':source['target'],
                  'distance':7.,'scale':source['ortho_scale'],'lens':50.,'resolution':source['resolution'],'sensor_fit':'AUTO'}
            heads.append((view,pose))
            render(pose,cmp,f'{label}_head_{view}.png','color')
        full=next(r for r in prior['files'] if r['model']=='A' and r['kind']=='full')
        full_pose={'type':'ORTHO','yaw':0.,'pitch':0.,'target':full['target'],'distance':7.,
                   'scale':full['ortho_scale'],'lens':50.,'resolution':full['resolution'],'sensor_fit':'AUTO'}
        render(full_pose,cmp,f'{label}_full_front.png','color')
        common=dict(COMMON,sensor_fit='HORIZONTAL')
        render(common,ref,f'{label}_common_ORTHO.png','color')
        dg=bpy.context.evaluated_depsgraph_get()
        body=mesh_record(bpy.data.objects['Naiwa unified golden body'],dg)
        eyes={side:mesh_record(bpy.data.objects[side+' concentric iris and pupil'],dg,eye=True) for side in ('L','R')}
        protocol['sources'][label]['common_reference_eye_pixels']={side:eye_pixels(eyes[side],body,COMMON,900,2) for side in ('L','R')}
        with gray_override(scene):
            for view,pose in heads:
                render(pose,cmp,f'{label}_head_gray_{view}.png','gray')
            render(common,ref,f'{label}_common_gray_ORTHO.png','gray')
        with body_id(scene):
            for view,pose in heads:
                render(pose,cmp,f'{label}_body_id_{view}.png','body_id')
            render(common,ref,f'{label}_common_body_id.png','body_id')
        for tag,pose in [('extra_yaw45_pitch-5',dict(COMMON,yaw=45.,pitch=-5.)),
                         ('extra_yaw55_pitch0',dict(COMMON,yaw=55.,pitch=0.)),
                         ('extra_lens135',dict(COMMON,type='PERSP',lens=135.,distance=3.225))]:
            render(dict(pose,sensor_fit='HORIZONTAL'),ref,f'{label}_{tag}.png','extra_color_unranked')
        if not args.skip_diagnostics:
            with diagnostic_lighting(scene,'reflection') as config:
                for view,pose in heads:
                    render(pose,cmp,f'{label}_reflection_{view}.png','reflection',config)
            for variant in ('A','B'):
                with diagnostic_lighting(scene,'grazing',variant) as config:
                    for view,pose in heads:
                        render(pose,cmp,f'{label}_grazing_{variant}_{view}.png','grazing_'+variant,config)
        assert light_snapshot(scene)==original_lights, 'Diagnostic light state not restored'
        assert digest(path)==before_hash, 'Read-only render mutated saved input'
        protocol['sources'][label]['diagnostics_complete']=not args.skip_diagnostics
        output_protocol.write_text(json.dumps(protocol,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        (cmp/'camera_protocol.json').write_text(json.dumps(protocol,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print('N2_ROUND_MODEL_RENDER_OK',label,flush=True)
    assert digest(old_protocol)==old_hash
    assert digest(BASE)==BASE_HASH
    print('N2_ROUND_COMPARISONS_OK',args.models,flush=True)


if __name__=='__main__':
    main()
