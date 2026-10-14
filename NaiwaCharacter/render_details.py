import bpy
from pathlib import Path
from mathutils import Vector

out = Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=str(out / 'Naiwa_Character.blend'))
scene = bpy.context.scene
scene.frame_set(1)
head_raise=float(bpy.data.objects['Naiwa unified golden body'].get('head_rigid_raise',0.0))
scene.render.resolution_x = 900
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
camera = scene.camera
for name, scale, location, target in [
    ('naiwa_face_close.png', 0.95, (0, -7, 2.26), (0, 0, 2.26)),
    ('naiwa_face_side_close.png', 0.88, (6, -0.13, 2.24), (0, -0.13, 2.24)),
    ('naiwa_face_three_quarter_close.png', 0.95, (4.5, -6, 2.24), (0, -0.13, 2.24)),
    ('naiwa_hand_close.png', 0.65, (0.77, -5, 1.10), (0.77, 0, 1.10)),
    ('naiwa_foot_close.png', 0.75, (1.6, -3.0, 1.45), (0.35, -0.15, 0.13)),
    ('naiwa_foot_side_close.png', 0.62, (5.0, -0.15, 0.16), (0.35, -0.15, 0.16)),
    ('naiwa_neck_front_close.png', 0.82, (0,-7,2.25), (0,0,2.25)),
    ('naiwa_neck_side_close.png', 0.82, (6,-0.13,2.25), (0,-0.13,2.25)),
]:
    if name.startswith('naiwa_face'):
        location=tuple(v+(head_raise if i==2 else 0) for i,v in enumerate(location))
        target=tuple(v+(head_raise if i==2 else 0) for i,v in enumerate(target))
    camera.data.ortho_scale = scale
    camera.location = location
    camera.rotation_euler = (Vector(target) - camera.location).to_track_quat('-Z', 'Y').to_euler()
    scene.render.filepath = str(out / name)
    bpy.ops.render.render(write_still=True)
print('NAIWA_DETAILS_OK')
