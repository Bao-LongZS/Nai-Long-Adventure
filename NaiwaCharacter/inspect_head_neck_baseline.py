"""Read fixed spherical A and record head/neck/shoulder protection evidence."""
import bpy,sys,json,hashlib
from pathlib import Path
import numpy as np
from mathutils import Vector
from mathutils.kdtree import KDTree
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from geometry_diagnostic_utils import mesh_record,section_object,face_point
SOURCE=ROOT/'versions/20261006_spherical_eyes/candidate_A/Naiwa_Character.blend'
SHA='f251f5a0fc00fedf1df76fcc6286bc40e102406a536cb6c676a93e176cb71ca0'
OUT=ROOT/'versions/20261006_head_neck_refinement';OUT.mkdir(exist_ok=True)
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==SHA
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
rig=bpy.data.objects['Naiwa_Rig'];rig.animation_data.action=bpy.data.actions['Naiwa_Idle'];rig.animation_data.use_nla=False
bpy.context.scene.frame_set(1);bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get()
body=bpy.data.objects['Naiwa unified golden body'];record=mesh_record(body,dg)
points=record['points'];weights=[]
for vertex in body.data.vertices:
    weights.append({body.vertex_groups[g.group].name:float(g.weight) for g in vertex.groups})
arm_names=[n for n in {n for w in weights for n in w} if any(k in n.lower() for k in ('arm','hand','finger','shoulder'))]
arm_weight=np.asarray([sum(w.get(n,0) for n in arm_names) for w in weights])
torso_weight=np.asarray([w.get('Torso',0) for w in weights])
rows=[]
for z in np.arange(1.80,2.405,.025):
    selected=(points[:,2]>=z)&(points[:,2]<z+.025)
    arm=selected&(arm_weight>.005);torso=selected&(arm_weight<=.005)
    def bounds(mask):return [points[mask].min(0).tolist(),points[mask].max(0).tolist()] if mask.any() else None
    rows.append({'z_range':[float(z),float(z+.025)],'vertex_count':int(selected.sum()),
        'arm_weight_above_0_005_count':int(arm.sum()),'arm_bounds':bounds(arm),'torso_bounds':bounds(torso),
        'arm_total_weight_range':[float(arm_weight[selected].min()),float(arm_weight[selected].max())] if selected.any() else None,
        'arm_threshold_bounds':{str(t):bounds(selected&(arm_weight>=t)) for t in (.05,.1,.2,.3,.5,.7)},
        'dominant_arm_bounds':bounds(selected&(arm_weight>torso_weight))})
mesh_records={}
tree=KDTree(len(points))
for i,p in enumerate(points):tree.insert(Vector(p),i)
tree.balance()
for obj in bpy.data.objects:
    if obj.type!='MESH':continue
    p=np.asarray([tuple(obj.matrix_world@v.co) for v in obj.data.vertices])
    mesh_records[obj.name]={'vertex_count':len(p),'polygon_count':len(obj.data.polygons),
        'bounds':[p.min(0).tolist(),p.max(0).tolist()],
        'hide_render':obj.hide_render,'hide_viewport':obj.hide_viewport,'hide_get':obj.hide_get(),
        'modifiers':[(m.name,m.type,m.object.name if m.type=='ARMATURE' and m.object else None) for m in obj.modifiers],
        'vertex_groups':list(obj.vertex_groups.keys())}
    if obj.name=='Head and torso silhouette only':
        distances=np.asarray([tree.find(Vector(q))[2] for q in p])
        matches=np.asarray([tree.find(Vector(q))[1] for q in p],int)
        mesh_records[obj.name]['nearest_unified_body_vertex']={'maximum_distance':float(distances.max()),
            'mean_distance':float(distances.mean()),'matches_below_1e_6':int((distances<1e-6).sum()),
            'one_to_one':len(set(matches.tolist()))==len(matches)}
        mesh_records[obj.name]['matching_indices']=matches.tolist()
sections=[]
section_weight_samples=[]
for z in (2.03,2.12,2.22,2.31,2.35,2.38,2.40,2.52,2.60):
    front=face_point(record,0.,z)
    rear,_,_,_=record['bvh'].ray_cast(Vector((0,3,z)),Vector((0,-1,0)),6)
    sec=section_object(record,2,z)
    lo=np.asarray(sec['loops'][0]['bounds'][0]);hi=np.asarray(sec['loops'][0]['bounds'][1])
    sections.append({'z':z,'front':float(front[1]),'rear':float(rear.y),
        'depth':float(rear.y-front[1]),'x_width':float(hi[0]-lo[0]),
        'full_section':sec})
    for label,q in [('front',front),('rear',np.asarray(rear)),('left',lo),('right',hi)]:
        nearest=tree.find(Vector(q));i=nearest[1]
        section_weight_samples.append({'z':z,'sample':label,'vertex':i,'coordinates':points[i].tolist(),
            'arm_total_weight':float(arm_weight[i]),'weights':weights[i]})
out={'source':str(SOURCE),'sha256':SHA,'meshes':mesh_records,'arm_weight_group_names':arm_names,
    'arm_weight_and_torso_bins':rows,
    'bones':{b.name:{'head':list(b.head_local),'tail':list(b.tail_local),'parent':b.parent.name if b.parent else None} for b in rig.data.bones},
    'sections':sections,'top_anchors':{'maximum_z':float(points[:,2].max()),
        'top_vertex_indices':np.flatnonzero(points[:,2]>=points[:,2].max()-1e-6).tolist()},
    'non_Torso_weight_in_neck':int(sum(any(n!='Torso' and w>.005 for n,w in weights[i].items()) for i in np.flatnonzero((points[:,2]>2.03)&(points[:,2]<2.38)))),
    'source_unchanged':hashlib.sha256(SOURCE.read_bytes()).hexdigest()==SHA}
out['section_weight_samples']=section_weight_samples
(OUT/'baseline_head_neck_inspection.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
print('HEAD_NECK_BASELINE_INSPECTION_OK')
print(json.dumps({k:v for k,v in out.items() if k not in ('meshes','sections','arm_weight_and_torso_bins')},ensure_ascii=False))
