import bpy,sys
from pathlib import Path
from mathutils import Vector
out=Path(__file__).resolve().parent
sys.path.insert(0,str(out))
from neck_extension_sculpt import extend_neck
bpy.ops.wm.open_mainfile(filepath=str(out/'iterations/before_raised_head_neck_revision/Naiwa_Character.blend'))
extend_neck(bpy.data.objects['Naiwa unified golden body'],bpy.data.objects['Head and torso silhouette only'])
scene=bpy.context.scene
scene.render.resolution_x=900
scene.render.resolution_y=900
cam=scene.camera
for name,loc,target,scale in [
 ('neck_draft_side.png',(6,-.13,2.25),(0,-.13,2.25),.82),
 ('neck_draft_three_quarter.png',(6,-4.3,2.55),(0,0,1.31),3.45)]:
 cam.location=loc
 cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler()
 cam.data.ortho_scale=scale
 scene.render.filepath=str(out/name)
 bpy.ops.render.render(write_still=True)
print('NECK_DRAFT_OK')
