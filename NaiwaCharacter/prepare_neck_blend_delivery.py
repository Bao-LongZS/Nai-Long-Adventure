from pathlib import Path
out=Path(__file__).resolve().parent
module=out/'neck_blend_sculpt.py'
text=module.read_text(encoding='utf-8-sig').replace("        smooth.iterations=400\n        bpy.ops.object.modifier_apply",
"        smooth.iterations=400\n        while obj.modifiers.find(smooth.name)>0:\n            bpy.ops.object.modifier_move_up(modifier=smooth.name)\n        bpy.ops.object.modifier_apply")
module.write_text(text,encoding='utf-8')
old=(out/'revise_neck_extension.py').read_text(encoding='utf-8-sig')
rendering=old[old.index('scene = bpy.context.scene'):]
prefix='''import bpy,sys,json
from pathlib import Path
from mathutils import Vector
out=Path(__file__).resolve().parent
sys.path.insert(0,str(out))
from connection_sculpt import section
from neck_blend_sculpt import blend_neck_seams,BLEND_MIN_Z,CORE_LOW_Z,CORE_HIGH_Z,REAR_TOP_Z
baseline=out/'iterations/before_broad_neck_blending_revision/Naiwa_Character.blend'
bpy.ops.wm.open_mainfile(filepath=str(baseline))
body=bpy.data.objects['Naiwa unified golden body']
before=[v.co.copy() for v in body.data.vertices]
HEAD_RAISE=float(body.get('head_rigid_raise',0.14))
report=blend_neck_seams(body,bpy.data.objects['Head and torso silhouette only'])
fixed=[i for i,p in enumerate(before) if p.z<=BLEND_MIN_Z or p.z>=REAR_TOP_Z]
core=[i for i,p in enumerate(before) if CORE_LOW_Z<=p.z<=CORE_HIGH_Z]
assert all(body.data.vertices[i].co==before[i] for i in fixed+core)
report.update({
 'revision':'Broad blending across both raised-neck seams',
 'blend_height_range':[BLEND_MIN_Z,REAR_TOP_Z],
 'fixed_neck_middle_height_range':[CORE_LOW_Z,CORE_HIGH_Z],
 'fixed_neck_middle_vertex_count':len(core),
 'height':max(v.co.z for v in body.data.vertices)
})
(out/'neck_blend_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')

'''
rendering=rendering.replace("print('NECK_EXTENSION_DELIVERABLES_OK'", "view_layer.material_override = flat_material('Neutral grey neck diagnostic', (0.42,0.42,0.42), 0.90)\nrender('naiwa_neck_gray_three_quarter.png', (4.5,-6,2.33), (0,-0.13,2.33), 0.94)\nview_layer.material_override = None\nprint('BROAD_NECK_DELIVERABLES_OK'")
(out/'revise_neck_blend.py').write_text(prefix+rendering,encoding='utf-8')
build=out/'build_naiwa.py'
text=build.read_text(encoding='utf-8-sig')
needle='extend_neck(fused, body_only)'
assert needle in text
text=text.replace(needle,needle+"\n\nfrom neck_blend_sculpt import blend_neck_seams\nblend_neck_seams(fused, body_only)")
build.write_text(text,encoding='utf-8')
print('BROAD_NECK_DRIVER_AND_BUILD_READY')
