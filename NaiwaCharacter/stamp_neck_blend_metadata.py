import bpy
from pathlib import Path
out=Path(__file__).resolve().parent
for path in (out/'Naiwa_Character.blend',out/'iterations/broad_neck_blend_rebuild_check/Naiwa_Character.blend'):
    bpy.ops.wm.open_mainfile(filepath=str(path))
    body=bpy.data.objects['Naiwa unified golden body']
    assert body.get('neck_seams_blended')
    if body.get('preserved_body_below_z')!=1.94 or body.get('preserved_crown_above_z')!=2.44:
        body['preserved_body_below_z']=1.94
        body['preserved_crown_above_z']=2.44
        bpy.ops.wm.save_as_mainfile(filepath=str(path))
print('BROAD_NECK_METADATA_OK')
