import bpy
from pathlib import Path

source = Path(__file__).resolve().parent / 'Naiwa_Character.fbx'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source), use_anim=True)
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
rigs = [o for o in bpy.data.objects if o.type == 'ARMATURE']
assert meshes and len(rigs) == 1
assert len(rigs[0].data.bones) >= 10
assert all(any(m.type == 'ARMATURE' for m in o.modifiers) for o in meshes)
names = sorted(a.name for a in bpy.data.actions)
print('IMPORTED_ACTIONS', names)
assert all(any(n.endswith(prefix) for n in names) for prefix in ('Naiwa_Idle', 'Naiwa_Walk', 'Naiwa_Laugh', 'Naiwa_Jump'))
colored = [(o.name, [a.name for a in o.data.color_attributes]) for o in meshes if o.data.color_attributes]
print('IMPORTED_VERTEX_COLORS', colored)
assert colored, 'Cream belly vertex colors did not survive FBX export'
for side in ('L', 'R'):
    eyes = [o for o in meshes if o.name.startswith(f'{side} concentric iris and pupil')]
    assert len(eyes) == 1, f'{side} eye mesh was missing or duplicated after FBX reimport'
    eye_color = eyes[0].data.color_attributes.get('EyeColor')
    assert eye_color is not None and len(eye_color.data) > 0, f'{side} EyeColor vertex colors did not survive FBX export'
    print('IMPORTED_EYE_COLOR', side, eyes[0].name, eye_color.name, len(eye_color.data))
print('FBX_REIMPORT_OK', 'meshes=', len(meshes), 'bones=', len(rigs[0].data.bones), 'actions=', names)
