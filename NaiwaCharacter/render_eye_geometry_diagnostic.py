"""Render original closed eye curvature with temporary gray materials only."""
import hashlib
import json
import math
from pathlib import Path
import sys
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from render_head_reshape_comparison import prepare_scene, gray_override

SOURCE=ROOT/'versions/20261005_head_side_continuity/Naiwa_Character.blend'
OUT=ROOT/'diagnostics/20261005_head_geometry'
OUT.mkdir(parents=True,exist_ok=True)
digest=hashlib.sha256(SOURCE.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
scene,engine=prepare_scene()
record=json.loads((SOURCE.parent/'baseline_dimensions.json').read_text())
D=record['D']
eye=bpy.data.objects['R concentric iris and pupil']
cx,cz=eye['frontal_aperture_center']
hit,point,normal,index=eye.ray_cast(Vector((cx,-2,cz)),Vector((0,1,0)))
assert hit
target=Vector((cx,point.y+.04*D,cz))
scene.render.resolution_x=1200
scene.render.resolution_y=1200
scene.render.resolution_percentage=100
cam=scene.camera
cam.data.type='ORTHO'
cam.data.ortho_scale=2.20*D
cam.data.clip_start=.001
protocol=[]
with gray_override(scene):
    for name,yaw,pitch in [('front',0,0),('oblique',50,8)]:
        theta,phi=math.radians(yaw),math.radians(pitch)
        cam.location=target+3*Vector((math.sin(theta)*math.cos(phi),-math.cos(theta)*math.cos(phi),math.sin(phi)))
        cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
        filename='eye_gray_'+name+'_close.png'
        scene.render.filepath=str(OUT/filename)
        bpy.context.view_layer.update()
        bpy.ops.render.render(write_still=True)
        protocol.append({'filename':filename,'yaw_from_F_degrees':yaw,'camera_elevation_degrees':pitch,
                         'target':list(target),'ortho_scale':cam.data.ortho_scale})
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==digest
(OUT/'eye_gray_camera_protocol.json').write_text(json.dumps({'source':str(SOURCE),
    'source_sha256':digest,'source_unchanged':True,'D':D,'engine':engine,'resolution':[1200,1200],
    'gray_material':'temporary plain gray; pupil is same continuous mesh',
    'cameras':protocol,'view_transform':scene.view_settings.view_transform,
    'exposure':scene.view_settings.exposure},indent=2),encoding='utf-8')
print('EYE_GEOMETRY_GRAY_RENDER_OK')
