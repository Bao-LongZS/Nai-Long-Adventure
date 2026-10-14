"""Independently verify head translation, fixed heights and the new neck proportions."""
import bpy,sys,json
import numpy as np
from pathlib import Path
out=Path(__file__).resolve().parent
sys.path.insert(0,str(out))
from connection_sculpt import section
from neck_direction_sculpt import rigid_head_indices
bpy.ops.wm.open_mainfile(filepath=str(out/'Naiwa_Character.blend'))
names=['Naiwa unified golden body','L concentric iris and pupil','R concentric iris and pupil',
       'Short closed oral seam','L eye glint','R eye glint','Naiwa_Rig']
current=[bpy.data.objects[n] for n in names]
with bpy.data.libraries.load(str(out/'iterations/before_forward_neck_direction_revision/Naiwa_Character.blend'),link=False) as (source,target):
    target.objects=names
previous=target.objects
collection=bpy.data.collections.new('Temporary preceding-iteration verification')
bpy.context.scene.collection.children.link(collection)
for obj in previous:
    collection.objects.link(obj)
bpy.context.view_layer.update()
body,old_body=current[0],previous[0]
rig,old_rig=current[-1],previous[-1]
delta=np.array((0,-0.055,0))
def values(obj,property_name='co'):
    data=np.empty(len(obj.data.vertices)*3,dtype=np.float32)
    obj.data.vertices.foreach_get(property_name,data)
    return data.reshape((-1,3))
def topology(obj):
    data=np.empty(len(obj.data.loops),dtype=np.int32)
    obj.data.loops.foreach_get('vertex_index',data)
    return data
def colors(obj,name):
    attr=obj.data.color_attributes[name]
    data=np.empty(len(attr.data)*4,dtype=np.float32)
    attr.data.foreach_get('color',data)
    return data
def world_coordinates(obj):
    matrix=np.array(obj.matrix_world)
    return values(obj).astype(np.float64)@matrix[:3,:3].T+matrix[:3,3]
old,new=values(old_body),values(body)
assert old.shape==new.shape and np.isfinite(new).all()
assert np.array_equal(old[:,2],new[:,2]), 'Height or neck length changed'
assert body.matrix_world==old_body.matrix_world
fixed=(old[:,2]<=1.98)
shoulder=(old[:,2]<=2.06)&(np.abs(old[:,0])>=0.40)&(old[:,1]>=-0.12)&(old[:,1]<=0.19)
assert np.array_equal(old[fixed|shoulder],new[fixed|shoulder]), 'Chest, shoulder or limbs moved'
head=np.array(rigid_head_indices(old_body),dtype=np.int32)
head_error=float(np.max(np.linalg.norm(new[head]-old[head]-delta,axis=1)))
assert head_error<1e-6, 'Head shape or volume changed'
assert np.array_equal(old[head,0],new[head,0])
# Additional anatomical samples independent of the angled boundary classifier.
forehead=(old[:,2]>=2.345)&(old[:,1]<=-0.39)
rear_and_crown=old[:,2]>=2.405
for mask in (forehead,rear_and_crown):
    assert float(np.max(np.linalg.norm(new[mask]-old[mask]-delta,axis=1)))<1e-6
features={}
for before,after in zip(previous[1:6],current[1:6]):
    assert np.array_equal(topology(before),topology(after))
    assert np.array_equal(values(before)[:,2],values(after)[:,2])
    assert np.array_equal(np.array(before.matrix_world)[:3,:3],np.array(after.matrix_world)[:3,:3])
    error=float(np.max(np.linalg.norm(world_coordinates(after)-world_coordinates(before)-delta,axis=1)))
    assert error<1e-6, 'Facial feature shape, direction or relative position changed'
    features[after.name]=error
for before,after in zip(previous[:-1],current[:-1]):
    assert np.array_equal(topology(before),topology(after))
    assert len(before.data.polygons)==len(after.data.polygons)
    assert all(m.object is rig for m in after.modifiers if m.type=='ARMATURE')
for a,b in [(old_body,body)]+list(zip(previous[1:3],current[1:3])):
    name='NaiwaColor' if b is body else 'EyeColor'
    assert np.array_equal(colors(a,name),colors(b,name))
assert len(rig.data.bones)==10
assert {'Naiwa_Idle','Naiwa_Walk','Naiwa_Laugh','Naiwa_Jump'}<=set(bpy.data.actions.keys())
bone_names=set(rig.data.bones.keys())
for name in bone_names:
    assert rig.data.bones[name].matrix_local==old_rig.data.bones[name].matrix_local
for before,after in zip(previous[:-1],current[:-1]):
    og={g.index:g.name for g in before.vertex_groups if g.name in bone_names}
    ng={g.index:g.name for g in after.vertex_groups if g.name in bone_names}
    for a,b in zip(before.data.vertices,after.data.vertices):
        assert {og[g.group]:g.weight for g in a.groups if g.group in og}=={ng[g.group]:g.weight for g in b.groups if g.group in ng}
zs=[1.98,2.04,2.08,2.16,2.20,2.22,2.255,2.30,2.335,2.37,2.405]
rows=[]
for z in zs:
    a=np.array(section(old_body,z)); b=np.array(section(body,z))
    rows.append({'height':z,'before':a.tolist(),'after':b.tolist(),
                 'centre_y_change':float(((b[1]+b[2])-(a[1]+a[2]))/2)})
lookup={row['height']:row for row in rows}
assert lookup[2.22]['after'][0]<lookup[2.22]['before'][0]*0.94, 'Upper neck not narrowed'
assert lookup[2.22]['after'][0]>0.25 and lookup[2.30]['after'][0]>0.25, 'Neck became too thin'
old_flare=lookup[2.22]['before'][0]-lookup[2.30]['before'][0]
new_flare=lookup[2.22]['after'][0]-lookup[2.30]['after'][0]
assert 0<=new_flare<old_flare*0.65, 'Upper neck still immediately flares toward chest'
assert abs(lookup[2.04]['centre_y_change'])<abs(lookup[2.30]['centre_y_change']), 'Head forward offset did not diminish downward'
dense=np.array([section(body,float(z)) for z in np.linspace(2.08,2.405,131)])
assert np.all(np.diff(dense[:,0])<0.00015), 'Width forms a collar or waist ring'
assert np.all(np.diff(dense[:,2])<0.00015), 'Rear contour contains a projecting bump'
normals=values(body,'normal')
assert np.isfinite(normals).all()
normal_error=float(np.max(np.abs(np.linalg.norm(normals,axis=1)-1)))
assert normal_error<1e-5
assert not body.data.has_custom_normals
assert all(p.use_smooth for p in body.data.polygons)
assert not any(e.use_edge_sharp for e in body.data.edges)
neck=(new[:,2]>=2.105)&(new[:,2]<=2.405)
sample_z=np.linspace(2.08,2.405,131)
centres=np.interp(new[neck,2],sample_z,(dense[:,1]+dense[:,2])/2)
radial=np.column_stack((new[neck,0],new[neck,1]-centres,np.zeros(int(np.sum(neck)))))
outward=float(np.min(np.sum(normals[neck]*radial,axis=1)))
assert outward>0, 'Inward neck normal'
path=out/'neck_direction_validation.json'
report=json.loads(path.read_text(encoding='utf-8-sig'))
report.update({
 'independent_validation':'passed',
 'all_vertex_heights_exactly_preserved':True,
 'head_size_and_face_direction_preserved':True,
 'rigid_head_translation_max_error':head_error,
 'protected_vertex_count':int(np.sum(fixed|shoulder)),
 'protected_position_error':0.0,
 'facial_feature_translation_errors':features,
 'topology_bones_weights_colors_preserved':True,
 'independent_sections':rows,
 'upper_neck_flare_before':old_flare,'upper_neck_flare_after':new_flare,
 'side_width_and_rear_contour_monotonic':True,
 'normals':{'unit_length_max_error':normal_error,'minimum_outward_dot':outward,
            'custom_normals':False,'sharp_edges':0,'all_faces_smooth':True}
})
path.write_text(json.dumps(report,indent=2),encoding='utf-8')
print('FORWARD_NECK_INDEPENDENT_VALIDATION_OK','head_error=',head_error,
      'height_error=0.0','feature_errors=',features,'upper_neck_flare=',(old_flare,new_flare))
