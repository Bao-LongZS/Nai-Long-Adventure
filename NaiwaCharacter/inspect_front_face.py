import bpy,sys,json
from pathlib import Path
import numpy as np
from mathutils import Vector
OUT=Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=str(OUT/'iterations/before_continuous_front_face/Naiwa_Character.blend'))
body=bpy.data.objects['Naiwa unified golden body']
rows=[]
for z in np.arange(2.18,2.445,.01):
    ys=[]
    for x in (0,.025,.055,.09,.14):
        hit,p,_,_=body.ray_cast(Vector((x,-2,float(z))),Vector((0,1,0)))
        ys.append(float(p.y) if hit else None)
    rows.append([round(float(z),3),*ys])
print('FRONT_FACE_SECTIONS',json.dumps(rows))
