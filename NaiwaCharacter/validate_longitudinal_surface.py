"""Independent geometric, rig and surface integrity comparison of saved scenes."""
import bpy,sys,json,math,hashlib
import numpy as np
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT))
from longitudinal_surface_sculpt import coordinates,thickness_field
from connection_sculpt import section
target=OUT/'Naiwa_Character.blend'
if '--draft' in sys.argv:
    target=OUT/'iterations/longitudinal_surface_draft/Naiwa_Character.blend'
bpy.ops.wm.open_mainfile(filepath=str(target))
body=bpy.data.objects['Naiwa unified golden body']
names=['Naiwa unified golden body','Head and torso silhouette only','L concentric iris and pupil','R concentric iris and pupil','L eye glint','R eye glint','Short closed oral seam','Naiwa_Rig']
current=[bpy.data.objects[n] for n in names]
with bpy.data.libraries.load(str(OUT/'iterations/before_longitudinal_surface_revision/Naiwa_Character.blend'),link=False) as (a,b):
    b.objects=names
previous=b.objects
comparison=bpy.data.collections.new('Temporary surface baseline comparison')
bpy.context.scene.collection.children.link(comparison)
for obj in previous:
    comparison.objects.link(obj)
    obj.hide_render=True
bpy.context.scene.frame_set(1)
bpy.context.view_layer.update()
old=coordinates(previous[0]); new=coordinates(body)
assert old.shape==new.shape and np.isfinite(new).all()
assert np.array_equal(old[:,1:],new[:,1:]),'Side projection or height changed'
fixed=(old[:,2]<=1.98)|(old[:,2]>=2.585)|((old[:,2]<=2.09)&(np.abs(old[:,0])>=0.395)&(old[:,1]>=-0.12)&(old[:,1]<=0.19))
assert np.array_equal(old[fixed],new[fixed]),'Protected crown, shoulder or body moved'
landmarks={}
candidate=np.flatnonzero((old[:,2]>=2.30)&(old[:,2]<=2.405)&(np.abs(old[:,0])<0.012))
landmarks['lower_face_front']=int(candidate[np.argmin(old[candidate,1])])
candidate=np.flatnonzero((old[:,2]>=2.08)&(old[:,2]<=2.30)&(np.abs(old[:,0])<0.012)&(old[:,1]<-0.20))
landmarks['throat_inner']=int(candidate[np.argmax(old[candidate,1])])
landmarks['crown']=int(np.argmax(old[:,2]))
candidate=np.flatnonzero(old[:,2]<=1.98)
landmarks['neck_root']=int(candidate[np.argmax(old[candidate,2])])
assert all(np.array_equal(old[i],new[i]) for i in landmarks.values()),'Locked landmark moved'
def attribute(obj,attr,prop,size,dtype):
    data=np.empty(len(attr)*size,dtype=dtype); attr.foreach_get(prop,data); return data
for a,b in zip(previous[:7],current[:7]):
    assert a.matrix_world==b.matrix_world
    assert np.array_equal(attribute(a,a.data.loops,'vertex_index',1,np.int32),attribute(b,b.data.loops,'vertex_index',1,np.int32))
for a,b in zip(previous[2:7],current[2:7]):
    assert np.array_equal(coordinates(a),coordinates(b)),'Facial feature moved'
for a,b in [(previous[0],body),(previous[2],current[2]),(previous[3],current[3])]:
    name='NaiwaColor' if b is body else 'EyeColor'
    assert np.array_equal(attribute(a,a.data.color_attributes[name].data,'color',4,np.float32),attribute(b,b.data.color_attributes[name].data,'color',4,np.float32))
rig,old_rig=current[-1],previous[-1]
assert len(rig.data.bones)==10
for name in rig.data.bones.keys():
    assert rig.data.bones[name].matrix_local==old_rig.data.bones[name].matrix_local
def weight_hash(obj):
    names={g.index:g.name for g in obj.vertex_groups if g.name in rig.data.bones}
    h=hashlib.sha256()
    for v in obj.data.vertices:
        for g in v.groups:
            if g.group in names: h.update(f'{v.index}:{names[g.group]}:{g.weight}'.encode())
    return h.hexdigest()
assert weight_hash(previous[0])==weight_hash(body),'Bone weights changed'
assert {'Naiwa_Idle','Naiwa_Walk','Naiwa_Laugh','Naiwa_Jump'}<=set(bpy.data.actions.keys())
def integrity(obj):
    xyz=coordinates(obj)
    affected=(xyz[:,2]>=1.98)&(xyz[:,2]<=2.585)
    selected=xyz[affected]
    duplicate_vertices=len(selected)-len(np.unique(np.round(selected,7),axis=0))
    faces=[p for p in obj.data.polygons if 1.98<=p.center.z<=2.585]
    signatures=[tuple(sorted(p.vertices)) for p in faces]
    duplicate_faces=len(signatures)-len(set(signatures))
    normal=attribute(obj,obj.data.vertices,'normal',3,np.float32).reshape((-1,3))
    normal_error=float(np.max(np.abs(np.linalg.norm(normal[affected],axis=1)-1)))
    outward=(np.abs(xyz[:,0])>0.1)&affected
    minimum_outward_x=float(np.min(normal[outward,0]*np.sign(xyz[outward,0])))
    # Detect true surface crossings; neighbors sharing a vertex are expected.
    polygons=[tuple(p.vertices) for p in faces]
    bvh=BVHTree.FromPolygons([Vector(p) for p in xyz],polygons,all_triangles=False,epsilon=0.0)
    intersect=[]
    for a,b in bvh.overlap(bvh):
        if a>=b or set(polygons[a]).intersection(polygons[b]): continue
        intersect.append([a,b])
    return {'duplicate_vertices_at_1e_7':duplicate_vertices,'duplicate_faces':duplicate_faces,
            'degenerate_faces':sum(p.area<1e-12 for p in faces),
            'nonadjacent_intersections':len(intersect),'normal_unit_error':normal_error,
            'minimum_outward_x':minimum_outward_x,'custom_normals':obj.data.has_custom_normals,
            'sharp_edges':sum(e.use_edge_sharp for e in obj.data.edges),
            'all_faces_smooth':all(p.use_smooth for p in obj.data.polygons)}
check=integrity(body)
assert check['duplicate_vertices_at_1e_7']==0
assert check['duplicate_faces']==0 and check['degenerate_faces']==0
assert check['nonadjacent_intersections']==0,check
assert check['normal_unit_error']<1e-5 and check['minimum_outward_x']>0
assert not check['custom_normals'] and check['sharp_edges']==0 and check['all_faces_smooth']
zs,us,old_sections,old_field=thickness_field(previous[0])
_,_,new_sections,new_field=thickness_field(body)
mask=(zs>=2.16)&(zs<=2.505)
cols=(us>=-0.5)&(us<=0.55)
step=zs[1]-zs[0]
def variation(field):
    slope=np.gradient(field,step,axis=0)
    curvature=np.gradient(slope,step,axis=0)
    variation=np.gradient(curvature,step,axis=0)
    return float(np.sqrt(np.mean(variation[mask][:,cols]**2)))
variation_before=variation(old_field); variation_after=variation(new_field)
print('CURVATURE_VARIATION',variation_before,variation_after,check,flush=True)
assert variation_after<variation_before,'Longitudinal thickness variation did not improve'
report={
    'independent_validation':'passed','baseline':'actual open Blender scene',
    'all_y_z_exactly_preserved':True,'protected_position_error':0.0,
    'facial_features_exactly_preserved':True,'topology_colors_bones_weights_preserved':True,
    'height_before':float(old[:,2].max()),'height_after':float(new[:,2].max()),
    'max_x_change':float(np.max(np.abs(new[:,0]-old[:,0]))),
    'changed_vertex_count':int(np.count_nonzero(new[:,0]!=old[:,0])),
    'locked_landmarks':{name:{'vertex':i,'before':old[i].tolist(),'after':new[i].tolist()} for name,i in landmarks.items()},
    'thickness_curvature_variation_before':variation_before,
    'thickness_curvature_variation_after':variation_after,
    'curvature_variation_reduction_percent':100*(1-variation_after/variation_before),
    'surface_integrity':check,
    'sections':[{'z':float(z),'before':list(section(previous[0],float(z))),'after':list(section(body,float(z)))} for z in np.arange(2.10,2.56,0.025)]}
(target.parent/'longitudinal_surface_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('LONGITUDINAL_SURFACE_INDEPENDENT_VALIDATION_OK',json.dumps({k:v for k,v in report.items() if k!='sections'}))
