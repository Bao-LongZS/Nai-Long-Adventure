"""Load the verified result into the user's Blender and leave a useful side view."""
import bpy, json
from pathlib import Path
from mathutils import Vector
OUT=Path(__file__).resolve().parent
report=json.loads((OUT/'longitudinal_surface_validation.json').read_text(encoding='utf-8'))
assert report['independent_validation']=='passed'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'Naiwa_Character.blend'),load_ui=False)
area=bpy.context.area
if area is not None:
    area.type='VIEW_3D'
def configure_view():
    scene=bpy.context.scene
    scene.frame_set(1)
    body=bpy.data.objects['Naiwa unified golden body']
    bpy.ops.object.select_all(action='DESELECT')
    body.select_set(True); bpy.context.view_layer.objects.active=body
    shell=bpy.data.objects.get('Head and torso silhouette only')
    if shell:
        shell.hide_render=True; shell.hide_set(True)
    for screen in bpy.data.screens:
        for a in screen.areas:
            if a.type=='VIEW_3D':
                space=a.spaces.active
                if space.type!='VIEW_3D': continue
                space.overlay.show_overlays=False
                space.shading.type='SOLID'
                space.shading.light='STUDIO'
                space.shading.studiolight_rotate_z=0.25
                space.shading.color_type='SINGLE'
                space.shading.single_color=(0.55,0.55,0.55)
                space.shading.show_cavity=False
                space.shading.show_shadows=False
                space.region_3d.view_perspective='ORTHO'
                space.region_3d.view_location=(0,-0.10,1.34)
                space.region_3d.view_distance=4.6
                space.region_3d.view_rotation=Vector((1,0,0)).to_track_quat('Z','Y')
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Naiwa_Character.blend'))
    (OUT/'blender_surface_opened.json').write_text(json.dumps({'loaded_file':bpy.data.filepath,'surface_faired':bool(body.get('longitudinal_surface_faired')),'view':'neutral solid side orthographic'},indent=2),encoding='utf-8')
    print('SURFACE_RESULT_OPENED_IN_BLENDER',bpy.data.filepath)
    return None
bpy.app.timers.register(configure_view,first_interval=0.5)
