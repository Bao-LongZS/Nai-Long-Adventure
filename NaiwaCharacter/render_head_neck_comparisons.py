"""Read-only fixed camera head/neck renders and illumination-independent body IDs.

blender --background --python-exit-code 1 --python this.py -- --models A N1 N2
Run again with --models C1 after the selected neck candidate is fixed.
"""
import argparse
from contextlib import contextmanager
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from render_head_reshape_comparison import prepare_scene, gray_override
from spherical_reference_projection import axes, eye_pixels
from geometry_diagnostic_utils import mesh_record

BASE = ROOT/'versions/20261006_spherical_eyes/candidate_A/Naiwa_Character.blend'
BASE_HASH = 'f251f5a0fc00fedf1df76fcc6286bc40e102406a536cb6c676a93e176cb71ca0'
OUT = ROOT/'versions/20261006_head_neck_refinement'
COMMON = {'type':'ORTHO','yaw':50.,'pitch':-2.5,'target':[0,-.12,2.425],
          'distance':7.,'scale':.86,'lens':50.,'resolution':[900,900]}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def camera(scene, pose):
    back, _, _ = axes(pose['yaw'], pose['pitch'])
    target = Vector(pose['target'])
    cam = scene.camera
    cam.data.type = pose['type']
    cam.data.ortho_scale = pose['scale']
    cam.data.lens = pose['lens']
    cam.data.sensor_width = 36.
    # AUTO matches previous portrait head/full renders. HORIZONTAL matches the
    # square common reference and long-focus projection protocol.
    cam.data.sensor_fit = pose.get('sensor_fit','AUTO')
    cam.data.shift_x = cam.data.shift_y = 0.
    cam.location = target + pose['distance'] * Vector(back)
    cam.rotation_euler = (target-cam.location).to_track_quat('-Z','Y').to_euler()
    scene.render.resolution_x, scene.render.resolution_y = pose['resolution']
    bpy.context.view_layer.update()


@contextmanager
def body_id(scene):
    """White emission body on black, other geometry hidden; no color shading."""
    obj = bpy.data.objects['Naiwa unified golden body']
    materials = tuple(obj.data.materials)
    hidden = [(o, o.hide_render) for o in scene.objects]
    view = (scene.view_settings.view_transform,scene.view_settings.exposure,
            scene.view_settings.gamma,scene.view_settings.look)
    world_nodes = scene.world.node_tree.nodes
    bg = next(n for n in world_nodes if n.type == 'BACKGROUND')
    bg_color, bg_strength = tuple(bg.inputs['Color'].default_value),bg.inputs['Strength'].default_value
    mat = bpy.data.materials.new('Temporary white subject body ID')
    mat.use_nodes = True
    mat.node_tree.nodes.clear()
    emission = mat.node_tree.nodes.new('ShaderNodeEmission')
    emission.inputs['Color'].default_value = (1,1,1,1)
    emission.inputs['Strength'].default_value = 1.
    output = mat.node_tree.nodes.new('ShaderNodeOutputMaterial')
    mat.node_tree.links.new(emission.outputs[0],output.inputs['Surface'])
    try:
        for o in scene.objects:
            if o.type in {'MESH','CURVE','SURFACE','FONT','META'}:
                o.hide_render = o != obj
        for i in range(len(obj.data.materials)):
            obj.data.materials[i] = mat
        scene.view_settings.view_transform = 'Standard'
        scene.view_settings.exposure = 0.
        scene.view_settings.gamma = 1.
        bg.inputs['Color'].default_value = (0,0,0,1)
        bg.inputs['Strength'].default_value = 0.
        yield
    finally:
        for i, material in enumerate(materials):
            obj.data.materials[i] = material
        for o, hide in hidden:
            o.hide_render = hide
        scene.view_settings.view_transform,scene.view_settings.exposure,scene.view_settings.gamma,scene.view_settings.look = view
        bg.inputs['Color'].default_value = bg_color
        bg.inputs['Strength'].default_value = bg_strength
        bpy.data.materials.remove(mat)
        bpy.context.view_layer.update()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--models',nargs='+',choices=['A','N1','N2','C1'],default=['A','N1','N2'])
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    cmp, ref = OUT/'comparisons',OUT/'reference_comparison'
    cmp.mkdir(parents=True,exist_ok=True)
    ref.mkdir(parents=True,exist_ok=True)
    old_protocol = ROOT/'versions/20261006_spherical_eyes/comparisons/camera_protocol.json'
    source_protocol_hash = digest(old_protocol)
    prior = json.loads(old_protocol.read_text(encoding='utf-8'))
    existing = OUT/'render_protocol.json'
    protocol = json.loads(existing.read_text(encoding='utf-8')) if existing.exists() else {
        'source_camera_protocol':str(old_protocol),'source_camera_protocol_sha256':source_protocol_hash,
        'fixed_baseline':str(BASE),'fixed_baseline_sha256':BASE_HASH,'common_reference_camera':COMMON,
        'fixed_reference_registration':{'scale':.41175526202447044,'translation':[245.38813136509236,-17.159665982101984]},
        'colored_gray_view_transform':'Standard','colored_gray_exposure':-.4,
        'body_ID_method':'Only evaluated unified body visible, white emission on black world; illumination and surface colors excluded.',
        'saved_source_models':False,'sources':{},'files':[]}
    assert digest(BASE)==BASE_HASH
    for label in args.models:
        path = BASE if label=='A' else OUT/('candidate_'+label)/'Naiwa_Character.blend'
        before_hash = digest(path)
        protocol['sources'][label] = {'path':str(path),'sha256':before_hash}
        protocol['files'] = [r for r in protocol['files'] if r['model']!=label]
        bpy.ops.wm.open_mainfile(filepath=str(path),load_ui=False)
        scene, engine = prepare_scene()
        protocol['engine'] = engine

        def render(pose, dest, filename, mode):
            camera(scene,pose)
            scene.render.filepath = str(dest/filename)
            bpy.ops.render.render(write_still=True)
            protocol['files'].append({'model':label,'file':str((dest/filename).relative_to(OUT)),
                'mode':mode,'pose':pose,'location':list(scene.camera.location),
                'source_sha256':before_hash,
                'kind':'full' if '_full_' in filename else ('head' if dest==cmp else 'reference'),
                'gray':mode=='gray','yaw':pose['yaw'],'pitch':pose['pitch'],
                'target':pose['target'],'ortho_scale':pose['scale'],'resolution':pose['resolution']})

        heads=[]
        for view in ('front','three_quarter','side'):
            source = next(r for r in prior['files'] if r['model']=='A' and r['kind']=='head' and not r['gray'] and view in r['file'])
            pose={'type':'ORTHO','yaw':source['yaw'],'pitch':source['pitch'],
                  'target':source['target'],'distance':7.,'scale':source['ortho_scale'],
                  'lens':50.,'resolution':source['resolution'],'sensor_fit':'AUTO'}
            heads.append((view,pose))
            render(pose,cmp,f'{label}_head_{view}.png','color')
        full=next(r for r in prior['files'] if r['model']=='A' and r['kind']=='full')
        full_pose={'type':'ORTHO','yaw':0.,'pitch':0.,'target':full['target'],'distance':7.,
                   'scale':full['ortho_scale'],'lens':50.,'resolution':full['resolution'],'sensor_fit':'AUTO'}
        render(full_pose,cmp,f'{label}_full_front.png','color')
        render(dict(COMMON,sensor_fit='HORIZONTAL'),ref,f'{label}_common_ORTHO.png','color')
        dg=bpy.context.evaluated_depsgraph_get()
        body=mesh_record(bpy.data.objects['Naiwa unified golden body'],dg)
        eyes={side:mesh_record(bpy.data.objects[side+' concentric iris and pupil'],dg,eye=True) for side in ('L','R')}
        protocol['sources'][label]['common_reference_eye_pixels']={
            side:eye_pixels(eyes[side],body,COMMON,900,2) for side in ('L','R')}
        with gray_override(scene):
            for view,pose in heads:
                render(pose,cmp,f'{label}_head_gray_{view}.png','gray')
            render(dict(COMMON,sensor_fit='HORIZONTAL'),ref,f'{label}_common_gray_ORTHO.png','gray')
        with body_id(scene):
            for view,pose in heads:
                render(pose,cmp,f'{label}_body_id_{view}.png','body_id')
            render(dict(COMMON,sensor_fit='HORIZONTAL'),ref,f'{label}_common_body_id.png','body_id')
        for tag,pose in [('extra_yaw45_pitch-5',dict(COMMON,yaw=45.,pitch=-5.)),
                         ('extra_yaw55_pitch0',dict(COMMON,yaw=55.,pitch=0.)),
                         ('extra_lens135',dict(COMMON,type='PERSP',lens=135.,distance=3.225))]:
            render(dict(pose,sensor_fit='HORIZONTAL'),ref,f'{label}_{tag}.png','extra_color_unranked')
        assert digest(path)==before_hash
        existing.write_text(json.dumps(protocol,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        common_protocol={'engine':engine,'view_transform':'Standard','exposure':-.4,
            'same_camera_light_exposure_per_comparison':True,
            'source_hashes':{m:s['sha256'] for m,s in protocol['sources'].items()},
            'source_camera_protocol_sha256':source_protocol_hash,
            'files':[dict(r,file=Path(r['file']).name) for r in protocol['files']
                     if Path(r['file']).parts[0]=='comparisons']}
        (cmp/'camera_protocol.json').write_text(json.dumps(common_protocol,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print('HEAD_NECK_MODEL_RENDER_OK',label,flush=True)
    assert digest(old_protocol)==source_protocol_hash
    assert digest(BASE)==BASE_HASH
    print('HEAD_NECK_COMPARISONS_OK',args.models,flush=True)


if __name__=='__main__':
    main()
