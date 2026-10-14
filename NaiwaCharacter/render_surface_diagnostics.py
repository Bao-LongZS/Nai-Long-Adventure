"""Consistent soft neutral lighting for before/after geometric inspection."""
import bpy
import math
from mathutils import Vector

def configure_soft_lighting(scene):
    for obj in bpy.data.objects:
        if obj.type=='LIGHT': obj.hide_render=True
    for name,loc,power in [('Surface soft key',(3,-4,5),550),('Surface soft fill',(-3,-1,3.5),250)]:
        obj=bpy.data.objects.get(name)
        if obj is None:
            data=bpy.data.lights.new(name,'AREA')
            obj=bpy.data.objects.new(name,data); scene.collection.objects.link(obj)
        obj.data.energy=power; obj.data.size=4.5; obj.data.color=(1,1,1)
        obj.hide_render=False
        obj.location=loc; obj.rotation_euler=(Vector((0,0,2.2))-obj.location).to_track_quat('-Z','Y').to_euler()
    world=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
    world.inputs['Color'].default_value=(0.18,0.18,0.18,1)
    world.inputs['Strength'].default_value=0.5
    scene.view_settings.view_transform='Standard'; scene.view_settings.exposure=-0.4

def render_neutral_views(out,prefix,neutral=True):
    scene=bpy.context.scene
    rig=bpy.data.objects['Naiwa_Rig']
    rig.animation_data.action=bpy.data.actions['Naiwa_Idle']
    scene.frame_set(1)
    scene.render.resolution_x=1000; scene.render.resolution_y=1000
    scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG'
    scene.view_settings.view_transform='Standard'
    scene.view_settings.exposure=-0.4
    layer=bpy.context.view_layer
    layer.material_override=None
    if neutral:
        grey=bpy.data.materials.new('Soft neutral surface inspection')
        grey.use_nodes=True
        bs=next(n for n in grey.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
        bs.inputs['Base Color'].default_value=(0.38,0.38,0.38,1)
        bs.inputs['Roughness'].default_value=1
        bs.inputs['Specular IOR Level'].default_value=0.0
        layer.material_override=grey
    configure_soft_lighting(scene)
    cam=scene.camera
    cam.data.type='ORTHO'; cam.data.ortho_scale=1.03; cam.data.lens=50
    for view,loc in [('side',(7,-0.12,2.29)),('front',(0,-7.12,2.29)),('three_quarter',(7*math.sin(math.radians(35)),-0.12-7*math.cos(math.radians(35)),2.29))]:
        cam.location=loc
        cam.rotation_euler=(Vector((0,-0.12,2.29))-cam.location).to_track_quat('-Z','Y').to_euler()
        scene.render.filepath=str(out/(prefix+'_'+view+'.png'))
        bpy.ops.render.render(write_still=True)
