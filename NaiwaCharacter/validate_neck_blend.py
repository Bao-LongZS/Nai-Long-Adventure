"""Verify broad seam geometry, protected parts, retained neck core and normals."""
import bpy,sys,json
import numpy as np
from pathlib import Path
out=Path(__file__).resolve().parent
sys.path.insert(0,str(out))
from connection_sculpt import section
bpy.ops.wm.open_mainfile(filepath=str(out/'Naiwa_Character.blend'))
names=['Naiwa unified golden body','L concentric iris and pupil','R concentric iris and pupil',
       'Short closed oral seam','L eye glint','R eye glint','Naiwa_Rig']
current=[bpy.data.objects[n] for n in names]
with bpy.data.libraries.load(str(out/'iterations/before_broad_neck_blending_revision/Naiwa_Character.blend'),link=False) as (source,target):
    target.objects=names
previous=target.objects
verification_collection=bpy.data.collections.new('Temporary previous-iteration verification')
bpy.context.scene.collection.children.link(verification_collection)
for obj in previous:
    verification_collection.objects.link(obj)
bpy.context.view_layer.update()
body,old_body=current[0],previous[0]
rig,old_rig=current[-1],previous[-1]
def values(obj,property_name='co'):
    data=np.empty(len(obj.data.vertices)*3,dtype=np.float32)
    obj.data.vertices.foreach_get(property_name,data)
    return data.reshape((-1,3))
def topology(obj):
    data=np.empty(len(obj.data.loops),dtype=np.int32)
    obj.data.loops.foreach_get('vertex_index',data)
    return data
def colors(obj,name):
    attribute=obj.data.color_attributes[name]
    data=np.empty(len(attribute.data)*4,dtype=np.float32)
    attribute.data.foreach_get('color',data)
    return data
old,new=values(old_body),values(body)
assert old.shape==new.shape and np.isfinite(new).all()
assert body.matrix_world==old_body.matrix_world
fixed=(old[:,2]<=1.94)|(old[:,2]>=2.44)
core=(old[:,2]>=2.18)&(old[:,2]<=2.255)
shoulder=(old[:,2]<=2.06)&(np.abs(old[:,0])>=0.40)&(old[:,1]>=-0.12)&(old[:,1]<=0.19)
assert np.array_equal(old[fixed|core|shoulder],new[fixed|core|shoulder]), 'Protected surface moved'
for before,after in zip(previous[:-1],current[:-1]):
    assert np.array_equal(topology(before),topology(after)), 'Mesh topology changed'
    assert len(before.data.polygons)==len(after.data.polygons)
    if after is not body:
        assert np.array_equal(values(before),values(after)), 'Facial shape or relative position moved'
        matrix_error=float(np.max(np.abs(np.array(before.matrix_world)-np.array(after.matrix_world))))
        print('FEATURE_TRANSFORM_ERROR',after.name,matrix_error)
        assert before.matrix_basis==after.matrix_basis, 'Feature local transform changed'
        assert before.matrix_world==after.matrix_world, 'Feature world transform changed'
    assert all(m.object is rig for m in after.modifiers if m.type=='ARMATURE')
for a,b in [(old_body,body)]+list(zip(previous[1:3],current[1:3])):
    name='NaiwaColor' if b is body else 'EyeColor'
    assert np.array_equal(colors(a,name),colors(b,name)), 'Vertex colors changed'
assert len(rig.data.bones)==10
assert {'Naiwa_Idle','Naiwa_Walk','Naiwa_Laugh','Naiwa_Jump'}<=set(bpy.data.actions.keys())
bone_names=set(rig.data.bones.keys())
for name in bone_names:
    assert rig.data.bones[name].matrix_local==old_rig.data.bones[name].matrix_local
for before,after in zip(previous[:-1],current[:-1]):
    old_groups={g.index:g.name for g in before.vertex_groups if g.name in bone_names}
    new_groups={g.index:g.name for g in after.vertex_groups if g.name in bone_names}
    for a,b in zip(before.data.vertices,after.data.vertices):
        wa={old_groups[g.group]:g.weight for g in a.groups if g.group in old_groups}
        wb={new_groups[g.group]:g.weight for g in b.groups if g.group in new_groups}
        assert wa==wb, 'Bone weight changed'
# Independent cross-section measurements, not a reproduction of the blend mask.
zs=np.arange(1.96,2.4376,0.0025)
old_rows=np.array([section(old_body,float(z)) for z in zs])
new_rows=np.array([section(body,float(z)) for z in zs])
def curvature(rows):
    result=np.full_like(rows,np.nan)
    for i,z in enumerate(zs):
        mask=np.abs(zs-z)<=0.018
        if np.sum(mask)<8:
            continue
        for k in range(3):
            a,b,c=np.polyfit(zs[mask]-z,rows[mask,k],2)
            result[i,k]=abs(2*a)/(1+b*b)**1.5
    return result
old_k,new_k=curvature(old_rows),curvature(new_rows)
seams={}
for name,z in [('upper_chest',2.08),('head_root',2.325)]:
    span=np.abs(zs-z)<=0.045
    peaks_old=np.nanmax(old_k[span],axis=0)
    peaks_new=np.nanmax(new_k[span],axis=0)
    seams[name]={'height':z,'curvature_peak_before':peaks_old.tolist(),
                 'curvature_peak_after':peaks_new.tolist(),
                 'columns':['side_width','front','rear']}
    print('SEAM_CURVATURE',name,seams[name])
core_sections=[]
for z in (2.195,2.22,2.24):
    a,b=np.array(section(old_body,z)),np.array(section(body,z))
    error=float(np.max(np.abs(a-b)))
    assert error<0.00002, 'Narrow neck cross section changed'
    core_sections.append({'height':z,'before':a.tolist(),'after':b.tolist(),'max_error':error})
normals=values(body,'normal')
assert np.isfinite(normals).all()
normal_error=float(np.max(np.abs(np.linalg.norm(normals,axis=1)-1)))
assert normal_error<0.00001, 'Non-unit or degenerate vertex normal'
assert not body.data.has_custom_normals
assert all(p.use_smooth for p in body.data.polygons)
assert not any(e.use_edge_sharp for e in body.data.edges)
face_normals=np.empty(len(body.data.polygons)*3,dtype=np.float32)
body.data.polygons.foreach_get('normal',face_normals)
assert np.isfinite(face_normals).all()
assert float(np.min(np.linalg.norm(face_normals.reshape((-1,3)),axis=1)))>0.99
neck_vertices=(new[:,2]>=2.105)&(new[:,2]<=2.405)
centers=np.interp(new[neck_vertices,2],zs,(new_rows[:,1]+new_rows[:,2])/2)
radial=np.column_stack((new[neck_vertices,0],new[neck_vertices,1]-centers,np.zeros(int(np.sum(neck_vertices)))))
orientation=np.sum(normals[neck_vertices]*radial,axis=1)
assert float(np.min(orientation))>0, 'Inward-facing neck normals'
path=out/'neck_blend_validation.json'
report=json.loads(path.read_text(encoding='utf-8-sig'))
report.update({
 'independent_validation':'passed','protected_vertices':int(np.sum(fixed|shoulder)),
 'protected_position_error':0.0,'fixed_neck_middle_vertex_count':int(np.sum(core)),
 'five_facial_features_preserved':True,'topology_bones_weights_colors_preserved':True,
 'neck_core_sections':core_sections,'seam_curvature':seams,
 'normals':{'custom_normals':False,'sharp_edges':0,'all_faces_smooth':True,
            'maximum_unit_length_error':normal_error,'minimum_outward_radial_dot':float(np.min(orientation))}
})
path.write_text(json.dumps(report,indent=2),encoding='utf-8')
print('BROAD_NECK_INDEPENDENT_VALIDATION_OK','core=',int(np.sum(core)),'normal_error=',normal_error)


