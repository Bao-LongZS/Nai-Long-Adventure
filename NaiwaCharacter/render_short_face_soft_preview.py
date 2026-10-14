import bpy,sys,math
from pathlib import Path
from mathutils import Vector
OUT=Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=str(OUT/'Naiwa_Character.blend'))
scene=bpy.context.scene
for o in bpy.data.objects:
    if o.type=='LIGHT': o.hide_render=True
for name,location,power in [('Delivery soft key',(3,-4,5),550),('Delivery soft fill',(-3,-1,3.5),250)]:
    light=bpy.data.lights.new(name,'AREA'); light.energy=power; light.size=4.5
    obj=bpy.data.objects.new(name,light); scene.collection.objects.link(obj)
    obj.location=location; obj.rotation_euler=(Vector((0,0,2.2))-obj.location).to_track_quat('-Z','Y').to_euler()
world=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
world.inputs['Color'].default_value=(0.18,0.18,0.18,1); world.inputs['Strength'].default_value=0.5
scene.view_settings.view_transform='Standard'; scene.view_settings.exposure=-0.4
scene.render.resolution_x=900; scene.render.resolution_y=1000; scene.render.resolution_percentage=100
cam=scene.camera; cam.data.type='ORTHO'; cam.data.lens=50; cam.data.ortho_scale=3.45
for name,angle in [('front',0),('side',90),('three_quarter',35)]:
    cam.location=(7*math.sin(math.radians(angle)),-7*math.cos(math.radians(angle)),1.31)
    cam.rotation_euler=(Vector((0,0,1.31))-cam.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(OUT/'iterations/short_round_face_draft'/('color_soft_'+name+'.png'))
    bpy.ops.render.render(write_still=True)
