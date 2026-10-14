"""Export a saved character without rebuilding geometry or saving its blend.

Run with Blender --background --python export_saved_fbx.py -- --blend PATH.
The default output is Naiwa_Character.fbx beside the input; existing files
require --overwrite. Historical exports removed during cleanup can be restored
this way while their original .blend files remain untouched.
"""
import argparse
import hashlib
from pathlib import Path
import sys

import bpy


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--blend', required=True, type=Path)
parser.add_argument('--output', type=Path)
parser.add_argument('--overwrite', action='store_true')
parser.add_argument('--bake-step',type=float,default=1.,help='Animation sampling step in frames')
parser.add_argument('--bake-simplify',type=float,default=1.,help='FBX animation curve simplification factor')
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
if not (.01 <= args.bake_step <= 100) or args.bake_simplify < 0:
    parser.error('Choose a positive supported bake step and a nonnegative simplification factor')
source = args.blend.resolve(strict=True)
output = (args.output or source.with_suffix('.fbx')).resolve()
if output.suffix.lower() != '.fbx':
    parser.error('--output must end in .fbx')
if output.exists() and not args.overwrite:
    parser.error('Output exists; choose another --output or pass --overwrite')
if output == source:
    parser.error('Output must differ from the input .blend')

source_hash = digest(source)
bpy.ops.wm.open_mainfile(filepath=str(source))
rigs = [obj for obj in bpy.context.scene.objects if obj.type == 'ARMATURE']
assert len(rigs) == 1, 'Expected one character rig'
rig = rigs[0]
meshes = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH'
          and any(mod.type == 'ARMATURE' and mod.object == rig for mod in obj.modifiers)]
assert meshes and len(bpy.data.actions) >= 4, 'Character meshes or actions missing'
if rig.animation_data and bpy.data.actions.get('Naiwa_Idle'):
    rig.animation_data.action = bpy.data.actions['Naiwa_Idle']
    rig.animation_data.use_nla = False
rig.data.pose_position = 'POSE'
bpy.context.scene.frame_set(1)
bpy.ops.object.select_all(action='DESELECT')
for obj in [*meshes, rig]:
    obj.hide_set(False)
    obj.select_set(True)
bpy.context.view_layer.objects.active = rig
output.parent.mkdir(parents=True, exist_ok=True)
bpy.ops.export_scene.fbx(
    filepath=str(output), use_selection=True, object_types={'ARMATURE', 'MESH'},
    add_leaf_bones=False, apply_unit_scale=True, bake_anim=True,
    bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False, path_mode='AUTO',
    bake_anim_step=args.bake_step,bake_anim_simplify_factor=args.bake_simplify,
)
assert digest(source) == source_hash, 'Input .blend changed'
print('SAVED_FBX_EXPORT_OK', str(output), 'meshes=', len(meshes),
      'bones=', len(rig.data.bones), 'actions=', len(bpy.data.actions), flush=True)
