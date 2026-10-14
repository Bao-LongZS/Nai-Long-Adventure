"""One reproducible side-contour refinement from the immutable latest head."""
import bpy,sys,argparse,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from head_side_continuity_sculpt import fix_side_continuity
BASE=ROOT/'versions/20261005_head_reshape/Naiwa_Character.blend'
RECORD=ROOT/'versions/20261005_head_side_continuity/baseline_dimensions.json'
parser=argparse.ArgumentParser()
parser.add_argument('--output',type=Path,default=RECORD.parent)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
out=args.output.resolve();assert out!=BASE.parent.resolve()
out.mkdir(parents=True,exist_ok=True)
rec=json.loads(RECORD.read_text(encoding='utf-8'))
assert hashlib.sha256(BASE.read_bytes()).hexdigest()==rec['source_sha256']
bpy.ops.wm.open_mainfile(filepath=str(BASE))
body=bpy.data.objects['Naiwa unified golden body']
report=fix_side_continuity(body,rec)
rig=bpy.data.objects['Naiwa_Rig'];rig.animation_data.action=bpy.data.actions['Naiwa_Idle']
bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
bpy.context.preferences.filepaths.save_version=0
bpy.context.scene.view_layers[0].material_override=None
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Naiwa_Character.blend'),compress=True)
meshes=[obj for obj in bpy.data.objects if obj.type=='MESH' and any(m.type=='ARMATURE' for m in obj.modifiers)]
bpy.ops.object.select_all(action='DESELECT')
for obj in meshes+[rig]:obj.select_set(True)
bpy.context.view_layer.objects.active=rig
bpy.ops.export_scene.fbx(filepath=str(out/'Naiwa_Character.fbx'),use_selection=True,
    object_types={'MESH','ARMATURE'},add_leaf_bones=False,bake_anim=True,
    bake_anim_use_all_actions=True,bake_anim_use_nla_strips=False,path_mode='AUTO')
(out/'baseline_dimensions.json').write_text(json.dumps(rec,indent=2),encoding='utf-8')
report['source_unchanged']=hashlib.sha256(BASE.read_bytes()).hexdigest()==rec['source_sha256']
(out/'head_side_continuity_build_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('HEAD_SIDE_CONTINUITY_BUILD_OK',report['maximum_euclidean_displacement'],report['maximum_normal_displacement'])
