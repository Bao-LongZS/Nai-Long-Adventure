"""Independently inspect the saved face/neck revision and its preserved rig."""
import bpy,sys,json,hashlib
import numpy as np
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT))
from longitudinal_surface_sculpt import coordinates
from connection_sculpt import section
target=OUT/'Naiwa_Character.blend'
if '--draft' in sys.argv: target=OUT/'iterations/short_round_face_draft/Naiwa_Character.blend'
bpy.ops.wm.open_mainfile(filepath=str(target))
names=['Naiwa unified golden body','Head and torso silhouette only','L concentric iris and pupil','R concentric iris and pupil','L eye glint','R eye glint','Short closed oral seam','Naiwa_Rig']
current=[bpy.data.objects[n] for n in names]
with bpy.data.libraries.load(str(OUT/'iterations/before_short_round_face_revision/Naiwa_Character.blend'),link=False) as (a,b): b.objects=names
previous=b.objects
collection=bpy.data.collections.new('Temporary short-face independent comparison')
bpy.context.scene.collection.children.link(collection)
for obj in previous: collection.objects.link(obj); obj.hide_render=True
bpy.context.scene.frame_set(1); bpy.context.view_layer.update()
body=current[0]; old=coordinates(previous[0]); new=coordinates(body)
assert old.shape==new.shape and np.isfinite(new).all()
assert np.array_equal(old[:,2],new[:,2]),'Height/neck length changed'
protected=(old[:,2]<=1.98)|(old[:,2]>=2.54)|((old[:,2]<=2.09)&(np.abs(old[:,0])>=0.395)&(old[:,1]>=-0.12)&(old[:,1]<=0.19))
assert np.array_equal(old[protected],new[protected]),'Crown, upper-chest root or limbs moved'
assert np.array_equal(old[old[:,2]>=2.425,0],new[old[:,2]>=2.425,0]),'Basic upper-head width changed'
def attribute(attr,prop,size,dtype):
    v=np.empty(len(attr)*size,dtype=dtype); attr.foreach_get(prop,v); return v
for a,b in zip(previous[:7],current[:7]):
    assert np.array_equal(attribute(a.data.loops,'vertex_index',1,np.int32),attribute(b.data.loops,'vertex_index',1,np.int32))
for a,b in [(previous[0],body),(previous[2],current[2]),(previous[3],current[3])]:
    name='NaiwaColor' if b is body else 'EyeColor'
    assert np.array_equal(attribute(a.data.color_attributes[name].data,'color',4,np.float32),attribute(b.data.color_attributes[name].data,'color',4,np.float32))
rig=current[-1]; old_rig=previous[-1]; assert len(rig.data.bones)==10
for name in rig.data.bones:
    assert name.matrix_local==old_rig.data.bones[name.name].matrix_local
assert {'Naiwa_Idle','Naiwa_Walk','Naiwa_Laugh','Naiwa_Jump'}<=set(bpy.data.actions.keys())
def weights(obj):
    groups={g.index:g.name for g in obj.vertex_groups if g.name in rig.data.bones}
    h=hashlib.sha256()
    for v in obj.data.vertices:
        for g in v.groups:
            if g.group in groups: h.update(f'{v.index}:{groups[g.group]}:{g.weight}'.encode())
    return h.hexdigest()
for a,b in zip(previous[:7],current[:7]): assert weights(a)==weights(b)
eyes={}
for side,a,b in [('L',previous[2],current[2]),('R',previous[3],current[3])]:
    p=coordinates(a).astype(float); q=coordinates(b).astype(float)
    pa=p.mean(axis=0); qa=q.mean(axis=0)
    u,s,v=np.linalg.svd((p-pa).T@(q-qa)); rotation=u@v
    error=float(np.max(np.linalg.norm((p-pa)@rotation+qa-q,axis=1)))
    assert error<1e-6,'Eye sphere dimensions or iris proportions changed'
    old_pupil=np.array(a['frontal_pupil_center']); new_pupil=np.array(b['frontal_pupil_center'])
    assert np.array_equal(old_pupil[[0,2]],new_pupil[[0,2]]),'Projected pupil layout changed'
    eyes[side]={'rigid_fit_error':error,'pupil_x_z':new_pupil[[0,2]].tolist()}
mouth=current[6]; old_mouth=coordinates(previous[6]); new_mouth=coordinates(mouth)
assert np.array_equal(old_mouth[:,0],new_mouth[:,0]),'Mouth width changed'
mouth_gap=[]
for p in new_mouth:
    hit,surface,_,_=body.ray_cast(Vector((float(p[0]),-2,float(p[2]))),Vector((0,1,0)))
    assert hit; mouth_gap.append(surface.y-float(p[1]))
assert max(abs(g-0.0007) for g in mouth_gap)<1e-6,'Mouth detached from the face'
assert np.ptp(new_mouth[:,2])<np.ptp(old_mouth[:,2])*0.60,'Mouth is not a shallow narrow seam'
zs=np.linspace(2.01,2.52,256)
before=np.array([section(previous[0],float(z)) for z in zs])
after=np.array([section(body,float(z)) for z in zs])
assert np.all(np.diff(after[:,0])<0.00015),'Neck forms a waist/collar'
assert np.all(np.diff(after[:,2])<0.00015),'Repeated nape bulges remain'
front_old=section(previous[0],2.359)[1]; front_new=section(body,2.359)[1]
assert 0.025<front_old-front_new<0.05,'Front face extension is not modest'
throat=section(body,2.185)[1]
assert throat-front_new>0.055,'Lower face/throat relationship remains too flat'
assert throat-section(previous[0],2.185)[1]<0.009,'Throat was cut into a deep groove'
assert section(body,2.23)[2]<section(previous[0],2.23)[2]-0.015,'Nape volume was not redistributed'
def slope(a,b): return (section(body,b)[0]-section(body,a)[0])/(b-a)
chest_slope=slope(2.06,2.16); face_slope=slope(2.24,2.31)
assert abs(chest_slope)>abs(face_slope)*1.4,'Width flare did not accelerate toward chest'
affected=(new[:,2]>=1.98)&(new[:,2]<=2.54)
points=new[affected]
duplicates=len(points)-len(np.unique(np.round(points,7),axis=0))
faces=[p for p in body.data.polygons if 1.98<=p.center.z<=2.54]
polygons=[tuple(p.vertices) for p in faces]
duplicate_faces=len(polygons)-len(set(tuple(sorted(p)) for p in polygons))
bvh=BVHTree.FromPolygons([Vector(p) for p in new],polygons,all_triangles=False,epsilon=0)
crossings=[]
for a,b in bvh.overlap(bvh):
    if a>=b or set(polygons[a]).intersection(polygons[b]): continue
    crossings.append([a,b])
normals=attribute(body.data.vertices,'normal',3,np.float32).reshape((-1,3))
normal_error=float(np.max(np.abs(np.linalg.norm(normals[affected],axis=1)-1)))
outward=(np.abs(new[:,0])>0.1)&affected
outward_x=float(np.min(normals[outward,0]*np.sign(new[outward,0])))
integrity={'duplicate_vertices_at_1e_7':duplicates,'duplicate_faces':duplicate_faces,
           'degenerate_faces':sum(p.area<1e-12 for p in faces),'nonadjacent_intersections':len(crossings),
           'normal_unit_error':normal_error,'minimum_outward_x':outward_x,
           'custom_normals':body.data.has_custom_normals,'sharp_edges':sum(e.use_edge_sharp for e in body.data.edges),
           'all_faces_smooth':all(p.use_smooth for p in body.data.polygons)}
assert duplicates==duplicate_faces==integrity['degenerate_faces']==len(crossings)==0,integrity
assert normal_error<1e-5 and outward_x>0,integrity
assert not integrity['custom_normals'] and integrity['sharp_edges']==0 and integrity['all_faces_smooth']
report={'independent_validation':'passed','all_body_heights_exact':True,'crown_root_limbs_exact':True,
        'upper_head_width_exact':True,'topology_weights_colors_bones_preserved':True,
        'height':float(new[:,2].max()),'face_extension':float(front_old-front_new),
        'throat_recession_from_face':float(throat-front_new),'mouth_seam_height':float(np.ptp(new_mouth[:,2])),
        'mouth_surface_gap':float(np.mean(mouth_gap)),'chest_width_slope':chest_slope,'lower_face_width_slope':face_slope,
        'eye_fitting':eyes,'surface_integrity':integrity,
        'sections':[{'z':float(z),'before':a.tolist(),'after':b.tolist()} for z,a,b in zip(zs[::10],before[::10],after[::10])]}
(target.parent/'short_round_face_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('SHORT_ROUND_FACE_INDEPENDENT_VALIDATION_OK',json.dumps({k:v for k,v in report.items() if k!='sections'}))
