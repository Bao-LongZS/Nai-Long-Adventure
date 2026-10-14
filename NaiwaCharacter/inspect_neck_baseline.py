import bpy
from pathlib import Path
import sys
out=Path(__file__).resolve().parent
sys.path.insert(0,str(out))
from connection_sculpt import section
bpy.ops.wm.open_mainfile(filepath=str(out/'Naiwa_Character.blend'))
body=bpy.data.objects['Naiwa unified golden body']
for z in (1.98,2.0,2.01,2.10,2.16,2.18,2.185,2.19,2.20,2.205,2.21,2.22,2.325):
    print('BASE_NECK_SECTION',z,section(body,z))
mouth=bpy.data.objects['Short closed oral seam']
print('MOUTH_Z_RANGE',min(v.co.z for v in mouth.data.vertices),max(v.co.z for v in mouth.data.vertices))

