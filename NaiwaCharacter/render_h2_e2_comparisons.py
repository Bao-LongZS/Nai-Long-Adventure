"""Read-only, fixed R1/H2/E2 full-body and head comparisons.

This script opens saved native files, uses the original color/light/exposure
protocol and a fixed camera for every label, and never saves a source file.
"""
import argparse
import json
from pathlib import Path
import sys
import bpy

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from render_head_reshape_comparison import prepare_scene, gray_override
from render_head_neck_comparisons import camera, body_id, COMMON
from render_n2_round_comparisons import (digest, light_snapshot, diagnostic_lighting,
    STRIPES, GRAZING)
from spherical_reference_projection import eye_pixels
from geometry_diagnostic_utils import mesh_record

BASE = ROOT/'versions/20261007_N2_round_refinement/candidate_R1/Naiwa_Character.blend'
BASE_HASH = 'ea12e9af2c564527d0c473cd31b28974339fcceb15b666502a33f0ffa057bc24'
OUT = ROOT/'versions/20261007_H2_E2_overall_refinement'
VIEWS = ('front','three_quarter','side')
YAWS = (0.,45.,90.)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--labels','--models',dest='labels',nargs='+',choices=['R1','H2','E2'],default=['R1','H2'])
    parser.add_argument('--skip-diagnostics',action='store_true')
    parser.add_argument('--output',type=Path,default=OUT)
    parser.add_argument('--candidate',type=Path,help='Override one candidate path for iteration; cannot override R1.')
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    if args.candidate:
        assert len(args.labels)==1 and args.labels[0]!='R1'
    out=args.output.resolve()
    cmp,ref=out/'comparisons',out/'reference_comparison'
    cmp.mkdir(parents=True,exist_ok=True)
    ref.mkdir(parents=True,exist_ok=True)
    original_protocol=ROOT/'versions/20261006_spherical_eyes/comparisons/camera_protocol.json'
    original_hash=digest(original_protocol)
    prior=json.loads(original_protocol.read_text(encoding='utf-8'))
    path_protocol=out/'render_protocol.json'
    protocol=json.loads(path_protocol.read_text(encoding='utf-8')) if path_protocol.exists() else {
        'fixed_baseline':str(BASE),'fixed_baseline_sha256':BASE_HASH,
        'source_camera_protocol':str(original_protocol),'source_camera_protocol_sha256':original_hash,
        'common_reference_camera':COMMON,
        'fixed_reference_registration':{'scale':.41175526202447044,'translation':[245.38813136509236,-17.159665982101984]},
        'animation':'Naiwa_Idle','frame':1,'saved_source_models':False,
        'sources':{},'files':[],'reflection_diagnostic':STRIPES,'grazing_diagnostic':GRAZING,
        'same_camera_light_exposure_per_comparison':True,
        'colored_gray_view_transform':'Standard','colored_gray_exposure':-.4,
        'body_ID_method':'Evaluated unified body only; white emission on black world.'}
    assert digest(BASE)==BASE_HASH
    for label in args.labels:
        path=BASE if label=='R1' else (args.candidate or OUT/f'candidate_{label}/Naiwa_Character.blend')
        source_hash=digest(path)
        bpy.ops.wm.open_mainfile(filepath=str(path),load_ui=False)
        scene,engine=prepare_scene()
        original_lights=light_snapshot(scene)
        bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
        protocol['sources'][label]={'path':str(path),'sha256':source_hash,'standard_lighting':{
            'lights':original_lights,'world_color':list(bg.inputs['Color'].default_value),
            'world_strength':bg.inputs['Strength'].default_value}}
        protocol['files']=[r for r in protocol['files'] if r['model']!=label]

        def render(pose,dest,filename,mode,diagnostic=None):
            camera(scene,pose)
            target=dest/filename
            scene.render.filepath=str(target)
            bpy.ops.render.render(write_still=True)
            protocol['files'].append({'model':label,'file':str(target.relative_to(out)),
                'mode':mode,'pose':pose,'location':list(scene.camera.location),
                'source_sha256':source_hash,'engine':scene.render.engine,
                'diagnostic':diagnostic,'view_transform':scene.view_settings.view_transform,
                'exposure':scene.view_settings.exposure,'gamma':scene.view_settings.gamma,
                'kind':'full' if '_full_' in filename else ('head' if dest==cmp else 'reference'),
                'gray':mode=='gray','yaw':pose['yaw'],'pitch':pose['pitch'],
                'target':pose['target'],'ortho_scale':pose['scale'],'resolution':pose['resolution']})

        heads=[]
        for view in VIEWS:
            old=next(r for r in prior['files'] if r['model']=='A' and r['kind']=='head' and not r['gray'] and view in r['file'])
            heads.append((view,{'type':'ORTHO','yaw':old['yaw'],'pitch':old['pitch'],'target':old['target'],
                'distance':7.,'scale':old['ortho_scale'],'lens':50.,'resolution':old['resolution'],'sensor_fit':'AUTO'}))
        old=next(r for r in prior['files'] if r['model']=='A' and r['kind']=='full')
        fulls=[(view,{'type':'ORTHO','yaw':yaw,'pitch':0.,'target':old['target'],'distance':7.,
            'scale':old['ortho_scale'],'lens':50.,'resolution':old['resolution'],'sensor_fit':'AUTO'})
            for view,yaw in zip(VIEWS,YAWS)]
        for view,pose in heads:
            render(pose,cmp,f'{label}_head_{view}.png','color')
        for view,pose in fulls:
            render(pose,cmp,f'{label}_full_{view}.png','color')
        common=dict(COMMON,sensor_fit='HORIZONTAL')
        render(common,ref,f'{label}_common_ORTHO.png','color')
        dg=bpy.context.evaluated_depsgraph_get()
        body=mesh_record(bpy.data.objects['Naiwa unified golden body'],dg)
        eyes={s:mesh_record(bpy.data.objects[s+' concentric iris and pupil'],dg,eye=True) for s in ('L','R')}
        protocol['sources'][label]['common_reference_eye_pixels']={s:eye_pixels(eyes[s],body,COMMON,900,2) for s in ('L','R')}
        with gray_override(scene):
            for view,pose in heads:
                render(pose,cmp,f'{label}_head_gray_{view}.png','gray')
            for view,pose in fulls:
                render(pose,cmp,f'{label}_full_gray_{view}.png','gray')
            render(common,ref,f'{label}_common_gray_ORTHO.png','gray')
        with body_id(scene):
            for view,pose in heads:
                render(pose,cmp,f'{label}_body_id_{view}.png','body_id')
            for view,pose in fulls:
                render(pose,cmp,f'{label}_full_body_id_{view}.png','body_id')
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
        assert light_snapshot(scene)==original_lights
        assert digest(path)==source_hash
        protocol['sources'][label]['diagnostics_complete']=not args.skip_diagnostics
        path_protocol.write_text(json.dumps(protocol,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        (cmp/'camera_protocol.json').write_text(json.dumps(protocol,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print('H2_E2_MODEL_RENDER_OK',label,flush=True)
    assert digest(original_protocol)==original_hash
    assert digest(BASE)==BASE_HASH
    print('H2_E2_COMPARISONS_OK',args.labels,flush=True)


if __name__=='__main__':
    main()
