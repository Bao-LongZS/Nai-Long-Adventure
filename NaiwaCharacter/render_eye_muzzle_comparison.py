"""Fixed camera/light protocol for six before/after views."""
import bpy,math,json
from mathutils import Vector
from render_surface_diagnostics import configure_soft_lighting

def render_six(out,prefix,groups=('full','head')):
    out.mkdir(parents=True,exist_ok=True)
    scene=bpy.context.scene
    rig=bpy.data.objects.get('Naiwa_Rig')
    if rig and rig.animation_data:rig.animation_data.action=bpy.data.actions['Naiwa_Idle']
    scene.frame_set(1);bpy.context.view_layer.update()
    scene.render.resolution_x=900;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
    bpy.context.view_layer.material_override=None
    configure_soft_lighting(scene)
    shell=bpy.data.objects.get('Head and torso silhouette only')
    if shell:shell.hide_render=True
    for side in ('L','R'):
        bpy.data.objects[side+' concentric iris and pupil'].hide_render=False
        bpy.data.objects[side+' eye glint'].hide_render=False
    bpy.data.objects['Short closed oral seam'].hide_render=False
    cam=scene.camera;cam.data.type='ORTHO';cam.data.lens=50
    for group in groups:
        target=(0,-.12,1.31) if group=='full' else (0,-.12,2.37)
        cam.data.ortho_scale=3.45 if group=='full' else .85
        for view,deg in [('front',0),('side',90),('45',45)]:
            ang=math.radians(deg)
            cam.location=(7*math.sin(ang),target[1]-7*math.cos(ang),target[2])
            cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler()
            scene.render.filepath=str(out/(prefix+'_'+group+'_'+view+'.png'))
            bpy.ops.render.render(write_still=True)
    (out/'camera_protocol.json').write_text(json.dumps({'projection':'ORTHO','lens_mm':50,'distance':7,
        'full_scale':3.45,'head_scale':.85,'full_target':[0,-.12,1.31],'head_target':[0,-.12,2.37],
        'views_degrees':{'front':0,'side':90,'45':45},'exposure':-.4,'key':550,'fill':250,'area_size':4.5,
        'before_after_identical_camera_framing_lighting_exposure':True},indent=2),encoding='utf8')
