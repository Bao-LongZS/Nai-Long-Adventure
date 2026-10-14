import bpy
import sys
import json
from pathlib import Path
from mathutils import Vector

out = Path(__file__).resolve().parent
sys.path.insert(0, str(out))
from connection_sculpt import section
from head_volume_sculpt import sculpt_head_volume, LOWER_Z, new_z

baseline = out / 'iterations' / 'before_smaller_head_depth_revision' / 'Naiwa_Character.blend'
bpy.ops.wm.open_mainfile(filepath=str(baseline))
body = bpy.data.objects['Naiwa unified golden body']
before = [v.co.copy() for v in body.data.vertices]
sample_z = [2.10, 2.16, 2.22, 2.28, 2.34, 2.361, 2.40, 2.46, 2.50]
old_sections = [section(body, z) for z in sample_z]
old_height = max(v.co.z for v in body.data.vertices)
shell = bpy.data.objects['Head and torso silhouette only']
sculpt_head_volume(body, shell)

protected = [(a, b.co) for a, b in zip(before, body.data.vertices) if a.z <= LOWER_Z]
max_error = max((a - b).length for a, b in protected)
assert max_error == 0.0, f'Protected torso/limbs moved: {max_error}'
print('PROTECTED_BODY_OK', len(protected), max_error)
report = {
    'revision': 'Reduced head width and front-back thickness',
    'protected_body_below_z': LOWER_Z,
    'protected_vertex_count': len(protected),
    'protected_max_position_error': max_error,
    'vertices': len(body.data.vertices),
    'height_before': old_height,
    'height_after': max(v.co.z for v in body.data.vertices),
    'sections': [{'source_z': z, 'new_z': new_z(z),
                  'old_width_front_rear': old,
                  'new_width_front_rear': section(body, new_z(z))}
                 for z, old in zip(sample_z, old_sections)]
}
(out / 'head_volume_validation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')

scene = bpy.context.scene
rig = bpy.data.objects['Naiwa_Rig']
rig.animation_data.action = bpy.data.actions['Naiwa_Idle']
scene.frame_set(1)
scene.camera.data.ortho_scale = 3.45
scene.camera.location = (0, -7, 1.31)
scene.camera.rotation_euler = (Vector((0, 0, 1.31)) - scene.camera.location).to_track_quat('-Z', 'Y').to_euler()
bpy.ops.wm.save_as_mainfile(filepath=str(out / 'Naiwa_Character.blend'))
character = [o for o in bpy.data.objects if o.type == 'MESH' and any(m.type == 'ARMATURE' for m in o.modifiers)]
bpy.ops.object.select_all(action='DESELECT')
for obj in character:
    obj.select_set(True)
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.export_scene.fbx(filepath=str(out/'Naiwa_Character.fbx'), use_selection=True,
    object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False, apply_unit_scale=True,
    bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False, path_mode='AUTO')

cam = scene.camera
scene.render.resolution_x = 900
scene.render.resolution_y = 1000
scene.render.resolution_percentage = 100

def render(name, location, target=(0, 0, 1.31), scale=3.45):
    cam.data.ortho_scale = scale
    cam.location = location
    cam.rotation_euler = (Vector(target) - cam.location).to_track_quat('-Z', 'Y').to_euler()
    scene.render.filepath = str(out / name)
    bpy.ops.render.render(write_still=True)

render('naiwa_front.png', (0, -7, 1.31))
render('naiwa_side.png', (7, 0, 1.31))
render('naiwa_three_quarter.png', (6, -4.3, 2.55))

view_layer = bpy.context.view_layer
def flat_material(name, color, roughness=1.0):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    shader = next((n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
    if shader is None:
        shader = material.node_tree.nodes.new('ShaderNodeBsdfPrincipled')
        output = next((n for n in material.node_tree.nodes if n.type == 'OUTPUT_MATERIAL'), None)
        if output is None:
            output = material.node_tree.nodes.new('ShaderNodeOutputMaterial')
        material.node_tree.links.new(shader.outputs['BSDF'], output.inputs['Surface'])
    shader.inputs['Base Color'].default_value = (*color, 1)
    shader.inputs['Roughness'].default_value = roughness
    return material

diagnostic = flat_material('Plain yellow diagnostic', (0.94, 0.66, 0.13), 0.72)
view_layer.material_override = diagnostic
render('naiwa_surface_check_side.png', (7, 0, 1.31))
view_layer.material_override = flat_material('Silhouette preview black', (0, 0, 0))
floor = bpy.data.objects['Preview floor']
floor.hide_render = True
world_background = scene.world.node_tree.nodes['Background']
old_color = tuple(world_background.inputs['Color'].default_value)
old_strength = world_background.inputs['Strength'].default_value
world_background.inputs['Color'].default_value = (1, 1, 1, 1)
world_background.inputs['Strength'].default_value = 1
render('naiwa_silhouette_front.png', (0, -7, 1.31))
render('naiwa_silhouette_side.png', (7, 0, 1.31))
for obj in character:
    obj.hide_render = True
shell.hide_render = False
render('naiwa_torso_silhouette_front.png', (0, -7, 1.31))
render('naiwa_torso_silhouette_side.png', (7, 0, 1.31))
shell.hide_render = True
for obj in character:
    obj.hide_render = False
view_layer.material_override = None
floor.hide_render = False
world_background.inputs['Color'].default_value = old_color
world_background.inputs['Strength'].default_value = old_strength

scene.render.resolution_y = 900
for name, scale, location, target in [
    ('naiwa_face_close.png', 0.95, (0, -7, 2.26), (0, 0, 2.26)),
    ('naiwa_face_side_close.png', 0.88, (6, -0.13, 2.24), (0, -0.13, 2.24)),
    ('naiwa_face_three_quarter_close.png', 0.95, (4.5, -6, 2.24), (0, -0.13, 2.24)),
    ('naiwa_hand_close.png', 0.65, (0.77, -5, 1.10), (0.77, 0, 1.10)),
    ('naiwa_foot_close.png', 0.75, (1.6, -3, 1.45), (0.35, -0.15, 0.13)),
    ('naiwa_foot_side_close.png', 0.62, (5, -0.15, 0.16), (0.35, -0.15, 0.16)),
]:
    render(name, location, target, scale)
print('HEAD_VOLUME_DELIVERABLES_OK', len(character), 'meshes', len(rig.data.bones), 'bones')
