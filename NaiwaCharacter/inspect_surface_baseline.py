import bpy, sys, json, math
from pathlib import Path
import numpy as np
from mathutils import Vector
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT))
from connection_sculpt import section
bpy.ops.wm.open_mainfile(filepath=str(OUT/'iterations/before_longitudinal_surface_revision/Naiwa_Character.blend'))
body=bpy.data.objects['Naiwa unified golden body']
co=np.empty(len(body.data.vertices)*3,dtype=np.float32)
body.data.vertices.foreach_get('co',co); co=co.reshape((-1,3))
zs=np.linspace(1.94,2.61,336)
profile=np.array([section(body,float(z)) for z in zs])
rows=[]
for z in np.arange(2.02,2.60,0.02):
    rows.append([round(float(z),3)]+list(section(body,float(z))))
report={'vertex_count':len(co),'bounds':[co.min(axis=0).tolist(),co.max(axis=0).tolist()],
        'modifiers':[(m.name,m.type) for m in body.modifiers],
        'custom_normals':body.data.has_custom_normals,
        'sections':rows,'properties':{k:str(body[k]) for k in body.keys()}}
(OUT/'iterations/before_longitudinal_surface_revision/surface_inspection.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
np.savez(OUT/'iterations/before_longitudinal_surface_revision/profiles.npz',z=zs,profile=profile)
print(json.dumps(report,indent=2))
