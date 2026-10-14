"""Build a new native head revision from the delivered preceding version."""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import bpy

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from head_reshape_sculpt import measure_baseline, reshape_body, reshape_eyes_mouth

BASE = ROOT / 'versions/20261005_eye_muzzle_body_delivery/Naiwa_Character.blend'
SMOOTH = BASE.parent / 'stage_face_raise.blend'
parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, default=ROOT / 'versions/20261005_head_reshape')
parser.add_argument('--measure-only', action='store_true')
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
out = args.output.resolve()
assert out != BASE.parent.resolve(), 'Preserve the preceding project.'
out.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(BASE))
body = bpy.data.objects['Naiwa unified golden body']
record_path = out / 'baseline_dimensions.json'
digest = hashlib.sha256(BASE.read_bytes()).hexdigest()
if record_path.exists():
    rec = json.loads(record_path.read_text(encoding='utf-8'))
    assert rec['source_sha256'] == digest, 'Pinned baseline changed; do not reuse its recorded dimensions.'
else:
    rec = measure_baseline(body)
    rec.update(source_blend=str(BASE), source_sha256=digest, smooth_base_sha256=hashlib.sha256(SMOOTH.read_bytes()).hexdigest())
    record_path.write_text(json.dumps(rec, ensure_ascii=False, indent=2), encoding='utf-8')
print('HEAD_RESHAPE_BASELINE_OK', rec['D0'], rec['W0'], rec['targets']['D1'])
if args.measure_only:
    raise SystemExit(0)
data_sets = ('objects', 'meshes', 'armatures', 'actions', 'materials')
existing_data = {name: {item.as_pointer() for item in getattr(bpy.data, name)} for name in data_sets}
with bpy.data.libraries.load(str(SMOOTH), link=False) as (available, loaded):
    loaded.objects = ['Naiwa unified golden body']
reference = loaded.objects[0]
bpy.context.scene.collection.objects.link(reference)
reference.hide_render = True
for modifier in reference.modifiers:
    modifier.show_viewport = False
bpy.context.view_layer.update()
report = {'baseline': rec, 'body': reshape_body(body, reference, rec)}
# A body reference can pull in its rig/action dependencies. Remove imported
# data after reading its coordinates so the new project keeps one original rig.
for name in data_sets:
    collection = getattr(bpy.data, name)
    for item in list(collection):
        if item.as_pointer() not in existing_data[name]:
            collection.remove(item, do_unlink=True)
report['features'] = reshape_eyes_mouth(body, rec)
rig = bpy.data.objects['Naiwa_Rig']
assert len([obj for obj in bpy.data.objects if obj.type == 'ARMATURE']) == 1
if bpy.data.actions.get('Naiwa_Idle'):
    rig.animation_data.action = bpy.data.actions['Naiwa_Idle']
bpy.context.scene.frame_set(1)
bpy.context.view_layer.update()
shell = bpy.data.objects.get('Head and torso silhouette only')
if shell:
    shell.hide_render = True
    shell.hide_set(True)
meshes = [obj for obj in bpy.data.objects if obj.type == 'MESH'
          and any(mod.type == 'ARMATURE' for mod in obj.modifiers)]
for obj in meshes:
    obj.hide_render = False
    obj.hide_set(False)
bpy.context.scene.view_layers[0].material_override = None
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(out / 'Naiwa_Character.blend'), compress=True)
bpy.ops.object.select_all(action='DESELECT')
for obj in meshes + [rig]:
    obj.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.export_scene.fbx(filepath=str(out / 'Naiwa_Character.fbx'), use_selection=True,
                        object_types={'MESH', 'ARMATURE'}, add_leaf_bones=False,
                        bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
                        path_mode='AUTO')
report.update(status='saved', source_unchanged=hashlib.sha256(BASE.read_bytes()).hexdigest() == digest)
(out / 'head_reshape_build_report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print('HEAD_RESHAPE_BUILD_OK')
