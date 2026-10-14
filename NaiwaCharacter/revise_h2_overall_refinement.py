"""Build each actual H2 trial anew from hash-pinned fixed R1."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from h2_overall_sculpt import sculpt
SOURCE=ROOT/'versions/20261007_N2_round_refinement/candidate_R1/Naiwa_Character.blend'
SOURCE_SHA='ea12e9af2c564527d0c473cd31b28974339fcceb15b666502a33f0ffa057bc24'
OUT=ROOT/'versions/20261007_H2_E2_overall_refinement/trials_H2'

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output',type=Path,default=OUT/'H2_A')
    ap.add_argument('--strength',type=float,default=1.)
    ap.add_argument('--profile',choices=['A','B'],default='B')
    ap.add_argument('--contact-fit',action='store_true')
    ap.add_argument('--actual-tangents',action='store_true')
    ap.add_argument('--hermite-front',action='store_true')
    ap.add_argument('--weighted-final',action='store_true')
    ap.add_argument('--front-arc',action='store_true')
    ap.add_argument('--angular-front',action='store_true')
    ap.add_argument('--target-tangents',action='store_true')
    ap.add_argument('--integrated-turn',action='store_true')
    ap.add_argument('--lateral-turn',action='store_true')
    ap.add_argument('--balanced-turn',action='store_true',help='Bounded soft front-angle fit; retains historical integrated-turn recipes.')
    ap.add_argument('--rear-curve',action='store_true',help='Joint broad final rear curve with derivative goals.')
    ap.add_argument('--contact-aware-turn',action='store_true',help='Fit the front curve to the actual fixed mouth neighborhood before the joint solve.')
    ap.add_argument('--distributed-contact-turn',action='store_true',help='Distribute the central under-mouth turn with a source-compatible angle-rate join and outward fill goals; requires --contact-aware-turn.')
    ap.add_argument('--rear-upper-z',type=float,default=2.54,help='Upper rear curve join; 2.54 preserves the historical M recipe.')
    ap.add_argument('--control-spacing',type=float,choices=[.050,.055,.060,.070,.080],default=.060)
    ap.add_argument('--iterations',type=int,default=1100)
    ap.add_argument('--rho',type=float,default=35.)
    ap.add_argument('--sigma-z',type=float,default=.020)
    ap.add_argument('--sigma-arc',type=float,default=.020)
    ap.add_argument('--clear-version',action='store_true')
    ap.add_argument('--quick-render',action='store_true')
    args=ap.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==SOURCE_SHA
    dest=args.output.resolve();assert dest!=SOURCE.parent.resolve()
    assert not (dest/'Naiwa_Character.blend').exists(), 'Refuse to overwrite an existing H2 candidate; use a new output directory'
    dest.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE),load_ui=False)
    rig=bpy.data.objects['Naiwa_Rig'];rig.animation_data.action=bpy.data.actions['Naiwa_Idle']
    rig.animation_data.use_nla=False;rig.data.pose_position='POSE'
    bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
    assert not args.balanced_turn or args.integrated_turn, '--balanced-turn requires --integrated-turn'
    assert not args.contact_aware_turn or args.balanced_turn, '--contact-aware-turn requires --balanced-turn'
    assert not args.distributed_contact_turn or args.contact_aware_turn, '--distributed-contact-turn requires --contact-aware-turn'
    assert 2.54<=args.rear_upper_z<=2.62, '--rear-upper-z must be in [2.54, 2.62]'
    options={'strength':args.strength,'profile':args.profile,'contact_fit':args.contact_fit,'actual_tangents':args.actual_tangents,'hermite_front':args.hermite_front,'weighted_final':args.weighted_final,'front_arc':args.front_arc,'angular_front':args.angular_front,'target_tangents':args.target_tangents,'integrated_turn':args.integrated_turn,'lateral_turn':args.lateral_turn,'balanced_turn':args.balanced_turn,'rear_curve':args.rear_curve,'contact_aware_turn':args.contact_aware_turn,'distributed_contact_turn':args.distributed_contact_turn,'rear_upper_z':args.rear_upper_z,'control_spacing':args.control_spacing,'iterations':args.iterations,'rho':args.rho,
        'sigma_z':args.sigma_z,'sigma_arc':args.sigma_arc,'clear_version':args.clear_version}
    report,field=sculpt(options)
    report.update(source=str(SOURCE),source_sha256=SOURCE_SHA,options=options,candidate=dest.name)
    if args.clear_version:
        report['stronger_version_reason']='Separately saved trial to remove residual broad back-neck/shoulder steps after regular H2; visual review required.'
    bpy.context.preferences.filepaths.save_version=0
    saved=dest/'Naiwa_Character.blend'
    bpy.ops.wm.save_as_mainfile(filepath=str(saved),compress=True)
    report['saved_blend_sha256']=hashlib.sha256(saved.read_bytes()).hexdigest()
    (dest/'build_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (dest/'parameters.json').write_text(json.dumps(options,indent=2)+'\n',encoding='utf-8')
    np.savez_compressed(dest/'deformation_field.npz',**field)
    print('H2_NATIVE_BUILD_OK',json.dumps(report['surfaces']),flush=True)
    if args.quick_render:
        from render_head_reshape_comparison import prepare_scene,gray_override
        from render_head_neck_comparisons import camera
        scene,_=prepare_scene();renders=dest/'quick_comparisons';renders.mkdir(exist_ok=True)
        for gray in (False,True):
            import contextlib
            with gray_override(scene) if gray else contextlib.nullcontext():
                for yaw in (0.,45.,90.):
                    pose={'type':'ORTHO','yaw':yaw,'pitch':0.,'target':[0,-.12,2.37],
                        'distance':7.,'scale':.85,'lens':50.,'resolution':[720,800]}
                    camera(scene,pose);scene.render.filepath=str(renders/f'head_{"gray" if gray else "color"}_{int(yaw)}.png')
                    bpy.ops.render.render(write_still=True)
        print('H2_QUICK_VIEWS_OK',flush=True)
    assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==SOURCE_SHA

if __name__=='__main__':main()
