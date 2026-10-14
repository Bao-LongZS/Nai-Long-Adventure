from pathlib import Path
import shutil,ast
out=Path(__file__).resolve().parent
module=out/'neck_direction_sculpt.py'
text=module.read_text(encoding='utf-8-sig')
text=text.replace("delta*max(0.0,math.sin(angle))**4*ease(BASE_Z,BASE_Z+0.05,z)*upper_feather",
                  "delta*ease(0.0,1.0,max(0.0,math.sin(angle)))*ease(BASE_Z,BASE_Z+0.05,z)*upper_feather")
module.write_text(text,encoding='utf-8')
old=(out/'revise_neck_blend.py').read_text(encoding='utf-8-sig')
rendering=old[old.index('scene = bpy.context.scene'):]
prefix='''import bpy,sys,json
from pathlib import Path
from mathutils import Vector
out=Path(__file__).resolve().parent
sys.path.insert(0,str(out))
from connection_sculpt import section
from neck_direction_sculpt import reshape_neck_direction,FORWARD_Y,BASE_Z,REAR_HEAD_Z,FRONT_HEAD_Z
baseline=out/'iterations/before_forward_neck_direction_revision/Naiwa_Character.blend'
bpy.ops.wm.open_mainfile(filepath=str(baseline))
body=bpy.data.objects['Naiwa unified golden body']
shell=bpy.data.objects['Head and torso silhouette only']
before=[v.co.copy() for v in body.data.vertices]
zs=[1.98,2.04,2.08,2.12,2.16,2.20,2.22,2.255,2.30,2.325,2.335,2.35,2.37,2.405,2.44]
old_sections=[section(body,z) for z in zs]
old_height=max(v.z for v in before)
HEAD_RAISE=float(body.get('head_rigid_raise',0.14))
result=reshape_neck_direction(body,shell)
assert all(v.co.z==before[v.index].z for v in body.data.vertices)
head_error=max((body.data.vertices[i].co-before[i]-Vector((0,FORWARD_Y,0))).length for i in result['rigid_head_indices'])
assert head_error<1e-6
report={
 'revision':'Forward head and upper neck with continuous throat and sloping nape',
 'head_forward_y':FORWARD_Y,
 'all_vertex_heights_exactly_preserved':True,
 'height_before':old_height,'height_after':max(v.co.z for v in body.data.vertices),
 'neck_direction_height_range':[BASE_Z,REAR_HEAD_Z],
 'head_front_boundary_z':FRONT_HEAD_Z,
 'rigid_head_vertex_count':len(result['rigid_head_indices']),
 'rigid_head_translation_max_error':head_error,
 'neck_vertex_count':len(result['neck_indices']),
 'fixed_vertex_count':len(result['fixed_indices']),
 'width_turn_goal':result['width_turn_goal'],
 'front_throat_goal':result['front_throat_goal'],
 'sections':[{'height':z,'before':list(a),'after':list(section(body,z))}
              for z,a in zip(zs,old_sections)]
}
(out/'neck_direction_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')

'''
rendering=rendering.replace('BROAD_NECK_DELIVERABLES_OK','FORWARD_NECK_DELIVERABLES_OK')
(out/'revise_neck_direction.py').write_text(prefix+rendering,encoding='utf-8')
build=out/'build_naiwa.py'
text=build.read_text(encoding='utf-8-sig')
if 'from neck_direction_sculpt import reshape_neck_direction' not in text:
    needle='blend_neck_seams(fused, body_only)'
    assert needle in text
    text=text.replace(needle,needle+'\n\nfrom neck_direction_sculpt import reshape_neck_direction\nreshape_neck_direction(fused, body_only)')
build.write_text(text,encoding='utf-8')
check=out/'iterations/forward_neck_direction_rebuild_check'
check.mkdir(exist_ok=True)
for name in ('build_naiwa.py','connection_sculpt.py','head_volume_sculpt.py','neck_extension_sculpt.py','neck_blend_sculpt.py','neck_direction_sculpt.py','render_details.py'):
    ast.parse((out/name).read_text(encoding='utf-8-sig'),filename=name)
    shutil.copy2(out/name,check/name)
ast.parse((out/'revise_neck_direction.py').read_text(encoding='utf-8-sig'))
print('FORWARD_NECK_DRIVER_READY')
