"""Read-only early shape gate: fixed gray views and two real reflections.

This limited report cannot be used as a complete delivery render protocol.
"""
import argparse
import json
from pathlib import Path
import sys
import bpy

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from render_head_reshape_comparison import prepare_scene, gray_override
from render_head_neck_comparisons import camera
from render_n2_round_comparisons import digest, light_snapshot, diagnostic_lighting


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--candidate', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    source = args.candidate.resolve()
    out = args.output.resolve()/'shape_gate'
    out.mkdir(parents=True, exist_ok=True)
    before = digest(source)
    original = ROOT/'versions/20261006_spherical_eyes/comparisons/camera_protocol.json'
    prior = json.loads(original.read_text(encoding='utf-8'))
    prior_hash = digest(original)
    heads=[]
    for view in ('front','three_quarter','side'):
        old=next(r for r in prior['files'] if r['model']=='A' and r['kind']=='head' and not r['gray'] and view in r['file'])
        heads.append((view, {'type':'ORTHO','yaw':old['yaw'],'pitch':old['pitch'],'target':old['target'],
            'distance':7.,'scale':old['ortho_scale'],'lens':50.,'resolution':old['resolution'],'sensor_fit':'AUTO'}))
    old=next(r for r in prior['files'] if r['model']=='A' and r['kind']=='full')
    fulls=[(view, {'type':'ORTHO','yaw':yaw,'pitch':0.,'target':old['target'],'distance':7.,
        'scale':old['ortho_scale'],'lens':50.,'resolution':old['resolution'],'sensor_fit':'AUTO'})
        for view,yaw in zip(('front','three_quarter','side'),(0.,45.,90.))]
    bpy.ops.wm.open_mainfile(filepath=str(source), load_ui=False)
    scene,engine=prepare_scene()
    lights=light_snapshot(scene)
    protocol={'source':str(source),'source_sha256':before,'limited_shape_gate':True,
        'complete_delivery_diagnostics':False,'saved_source_model':False,
        'original_camera_protocol_sha256':prior_hash,'files':[]}

    def render(kind,view,pose,diagnostic=None):
        camera(scene,pose)
        path=out/f'H2_{kind}_{view}.png'
        scene.render.filepath=str(path)
        bpy.ops.render.render(write_still=True)
        protocol['files'].append({'file':path.name,'source_sha256':before,'pose':pose,
            'engine':scene.render.engine,'view_transform':scene.view_settings.view_transform,
            'exposure':scene.view_settings.exposure,'gamma':scene.view_settings.gamma,'diagnostic':diagnostic})

    with gray_override(scene):
        for view,pose in heads: render('head_gray',view,pose)
        for view,pose in fulls: render('full_gray',view,pose)
    with diagnostic_lighting(scene,'reflection') as config:
        for view,pose in heads[1:]: render('reflection',view,pose,config)
    assert light_snapshot(scene)==lights and digest(source)==before and digest(original)==prior_hash
    (out/'protocol.json').write_text(json.dumps(protocol,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('H2_SHAPE_GATE_RENDER_OK',flush=True)


if __name__=='__main__': main()
