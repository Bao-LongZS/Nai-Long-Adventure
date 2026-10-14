"""Generate A/B from the pinned current model; never accumulates changes."""
import bpy,sys,json,argparse,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from spherical_eye_sculpt import *
SOURCE=ROOT/'versions/20261005_head_side_continuity/Naiwa_Character.blend'
EXPECTED='a95a5c7efb25cb6f379897dd6ec4af98a201bc2619dc8caa2bb5b5de8ea3dfd5'
DEFAULT=ROOT/'versions/20261006_spherical_eyes'
parser=argparse.ArgumentParser()
parser.add_argument('--output',type=Path,default=DEFAULT)
parser.add_argument('--candidate',choices=['A','B','both'],default='both')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
out=args.output.resolve();assert out!=SOURCE.parent.resolve();out.mkdir(parents=True,exist_ok=True)
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==EXPECTED
# Cache installation rays once, but reopen the immutable source per candidate.
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
rig=bpy.data.objects['Naiwa_Rig'];rig.animation_data.action=bpy.data.actions['Naiwa_Idle'];rig.animation_data.use_nla=False
bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
body_record=mesh_record(bpy.data.objects['Naiwa unified golden body'],bpy.context.evaluated_depsgraph_get())
caches={s:ProjectionCache(body_record,TARGETS[s]) for s in ('L','R')}
for candidate in (['A','B'] if args.candidate=='both' else [args.candidate]):
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
    rig=bpy.data.objects['Naiwa_Rig'];rig.animation_data.action=bpy.data.actions['Naiwa_Idle'];rig.animation_data.use_nla=False
    bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
    frozen=frozen_snapshot();body=bpy.data.objects['Naiwa unified golden body']
    records={}
    for side in ('L','R'):
        par=calibrate(body_record,caches[side],side,candidate)
        apply_eye(body,side,par);records[side]=par
        print('EYE_CALIBRATED',candidate,side,par['R'],par['h'],par['actual_visible_D'],par['actual_pupil_ratio'],flush=True)
    spacing=records['R']['actual_frontal_metrics']['aperture']['center'][0]-records['L']['actual_frontal_metrics']['aperture']['center'][0]
    assert abs(spacing/S0-1)<=.02
    assert frozen_snapshot()==frozen,'Non-eye body/rig changed'
    dest=out/('candidate_'+candidate);dest.mkdir(exist_ok=True)
    bpy.context.preferences.filepaths.save_version=0
    for layer in bpy.context.scene.view_layers:layer.material_override=None
    bpy.ops.wm.save_as_mainfile(filepath=str(dest/'Naiwa_Character.blend'),compress=True)
    meshes=[o for o in bpy.data.objects if o.type=='MESH' and any(m.type=='ARMATURE' for m in o.modifiers)]
    bpy.ops.object.select_all(action='DESELECT')
    for obj in meshes+[rig]:obj.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.fbx(filepath=str(dest/'Naiwa_Character.fbx'),use_selection=True,
        object_types={'MESH','ARMATURE'},add_leaf_bones=False,bake_anim=True,
        bake_anim_use_all_actions=True,bake_anim_use_nla_strips=False,path_mode='AUTO')
    report={'source':str(SOURCE),'source_sha256':EXPECTED,'candidate':candidate,'D0':D0,'S0':S0,
        'actual_spacing':spacing,'actual_spacing_over_D0':spacing/D0,'eyes':records,
        'unchanged_non_eye_snapshot':frozen,'non_eye_geometry_rig_unchanged':True,
        'source_unchanged':hashlib.sha256(SOURCE.read_bytes()).hexdigest()==EXPECTED,
        'coordinate_units':'Blender project world units; F=-Y','pose':'Naiwa_Idle frame 1'}
    (dest/'spherical_eye_build_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==EXPECTED
print('SPHERICAL_EYES_BUILD_OK',flush=True)
