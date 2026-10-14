"""Export the current rig and refresh all standard character previews."""
import bpy
import math
from mathutils import Vector

def material(name,rgb,roughness=1):
    m=bpy.data.materials.new(name); m.use_nodes=True
    bs=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    bs.inputs['Base Color'].default_value=(*rgb,1)
    bs.inputs['Roughness'].default_value=roughness
    return m

def deliver_character(out,revision_tag='LONGITUDINAL_SURFACE',soft_lighting=False):
    scene=bpy.context.scene; cam=scene.camera
    if soft_lighting:
        from render_surface_diagnostics import configure_soft_lighting
        configure_soft_lighting(scene)
    rig=bpy.data.objects['Naiwa_Rig']; rig.animation_data.action=bpy.data.actions['Naiwa_Idle']
    scene.frame_set(1)
    character=[o for o in bpy.data.objects if o.type=='MESH' and any(m.type=='ARMATURE' for m in o.modifiers)]
    shell=bpy.data.objects['Head and torso silhouette only']
    shell.hide_render=True; shell.hide_set(True)
    for o in character: o.hide_render=False; o.hide_set(False)
    cam.data.type='ORTHO'; cam.data.ortho_scale=3.45; cam.data.lens=50
    cam.location=(0,-7,1.31)
    cam.rotation_euler=(Vector((0,0,1.31))-cam.location).to_track_quat('-Z','Y').to_euler()
    bpy.ops.object.select_all(action='DESELECT')
    body=bpy.data.objects['Naiwa unified golden body']
    body.select_set(True); bpy.context.view_layer.objects.active=body
    # Leave the saved native project in a clean editable view of the model.
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type=='CONSOLE':
                area.type='VIEW_3D'
            if area.type=='VIEW_3D':
                space=next((s for s in area.spaces if s.type=='VIEW_3D'),None)
                if space is None: continue
                space.overlay.show_overlays=False
                space.region_3d.view_location=(0,-0.10,1.34)
                space.region_3d.view_distance=5.1
                space.region_3d.view_rotation=Vector((1,0,0)).to_track_quat('Z','Y')
                space.region_3d.view_perspective='ORTHO'
    bpy.ops.wm.save_as_mainfile(filepath=str(out/'Naiwa_Character.blend'))
    bpy.ops.object.select_all(action='DESELECT')
    for o in character: o.select_set(True)
    rig.select_set(True); bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.fbx(filepath=str(out/'Naiwa_Character.fbx'),use_selection=True,
        object_types={'ARMATURE','MESH'},add_leaf_bones=False,apply_unit_scale=True,
        bake_anim=True,bake_anim_use_all_actions=True,bake_anim_use_nla_strips=False,path_mode='AUTO')
    scene.render.resolution_x=900; scene.render.resolution_y=1000
    scene.render.resolution_percentage=100
    def render(name,loc,target=(0,0,1.31),scale=3.45):
        cam.data.ortho_scale=scale; cam.location=loc
        cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler()
        scene.render.filepath=str(out/name)
        bpy.ops.render.render(write_still=True)
    render('naiwa_front.png',(0,-7,1.31))
    render('naiwa_side.png',(7,0,1.31))
    render('naiwa_three_quarter.png',(7*math.sin(math.radians(35)),-7*math.cos(math.radians(35)),1.31))
    layer=bpy.context.view_layer
    layer.material_override=material('Plain yellow diagnostic',(0.94,0.66,0.13),0.72)
    render('naiwa_surface_check_side.png',(7,0,1.31))
    layer.material_override=material('Silhouette preview black',(0,0,0))
    floor=bpy.data.objects['Preview floor']; floor.hide_render=True
    world=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
    color=tuple(world.inputs['Color'].default_value); strength=world.inputs['Strength'].default_value
    world.inputs['Color'].default_value=(1,1,1,1); world.inputs['Strength'].default_value=1
    render('naiwa_silhouette_front.png',(0,-7,1.31))
    render('naiwa_silhouette_side.png',(7,0,1.31))
    for o in character: o.hide_render=True
    shell.hide_render=False
    render('naiwa_torso_silhouette_front.png',(0,-7,1.31))
    render('naiwa_torso_silhouette_side.png',(7,0,1.31))
    shell.hide_render=True
    for o in character: o.hide_render=False
    layer.material_override=None; floor.hide_render=False
    world.inputs['Color'].default_value=color; world.inputs['Strength'].default_value=strength
    scene.render.resolution_y=900
    raise_z=float(body.get('head_rigid_raise',0.14))
    for name,scale,loc,target in [
        ('naiwa_face_close.png',0.95,(0,-7,2.26+raise_z),(0,0,2.26+raise_z)),
        ('naiwa_face_side_close.png',0.88,(6,-0.13,2.24+raise_z),(0,-0.13,2.24+raise_z)),
        ('naiwa_face_three_quarter_close.png',0.95,(4.5,-6,2.24+raise_z),(0,-0.13,2.24+raise_z)),
        ('naiwa_hand_close.png',0.65,(0.77,-5,1.10),(0.77,0,1.10)),
        ('naiwa_foot_close.png',0.75,(1.6,-3,1.45),(0.35,-0.15,0.13)),
        ('naiwa_foot_side_close.png',0.62,(5,-0.15,0.16),(0.35,-0.15,0.16)),
        ('naiwa_neck_front_close.png',0.82,(0,-7,2.25),(0,0,2.25)),
        ('naiwa_neck_side_close.png',0.82,(6,-0.13,2.25),(0,-0.13,2.25))]:
        render(name,loc,target,scale)
    layer.material_override=material('Neutral grey neck diagnostic',(0.42,0.42,0.42),0.90)
    render('naiwa_neck_gray_three_quarter.png',(4.5,-6,2.33),(0,-0.13,2.33),0.94)
    layer.material_override=None
    print(revision_tag+'_DELIVERABLES_OK',len(character),'meshes',len(rig.data.bones),'bones')
