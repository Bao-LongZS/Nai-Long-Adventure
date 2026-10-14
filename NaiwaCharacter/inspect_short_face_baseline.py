import bpy,sys,json
from pathlib import Path
import numpy as np
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT))
from connection_sculpt import section
from longitudinal_surface_sculpt import coordinates
bpy.ops.wm.open_mainfile(filepath=str(OUT/'iterations/before_short_round_face_revision/Naiwa_Character.blend'))
body=bpy.data.objects['Naiwa unified golden body']
features={}
for name in ['L concentric iris and pupil','R concentric iris and pupil','Short closed oral seam']:
    o=bpy.data.objects[name]; xyz=coordinates(o)
    features[name]={'bounds':[xyz.min(axis=0).tolist(),xyz.max(axis=0).tolist()],
                    'center':((xyz.min(axis=0)+xyz.max(axis=0))/2).tolist(),
                    'properties':{k:str(o[k]) for k in o.keys()}}
print(json.dumps({'features':features,'sections':[[float(z),*section(body,float(z))] for z in np.arange(1.98,2.601,0.02)]},indent=2))
