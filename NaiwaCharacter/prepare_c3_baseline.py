"""One read-only E2 inspection; immutable caches for the C3 experiment."""
import argparse, hashlib, json, sys, shutil
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
from geometry_diagnostic_utils import array, mesh_record, intersections, components
from head_neck_validation_utils import fingerprint, fit_sphere
from saved_delivery_metadata import preserved_links, scalar
from validate_n2_round_refinement import rig_snapshot
from r1_validation_utils import normal_snapshot
from eye_projection_measure import visible_metrics

BODY='Naiwa unified golden body'
MOUTH='Short closed oral seam'
SOURCE=HERE/'versions/20261007_H2_E2_overall_refinement/candidate_E2/Naiwa_Character.blend'
SOURCE_SHA='317f62278d63217636bf393f831e2fb41ce21acfefbe21beed6ad6d4dc75f322'
POSES={'Idle1':('Naiwa_Idle',1),'Walk9':('Naiwa_Walk',9),'Laugh7':('Naiwa_Laugh',7),'Jump6':('Naiwa_Jump',6)}

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest()
def dump(path,value):path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def q(t):
    t=np.clip(t,0,1);return t*t*t*(10+t*(-15+6*t))
def raw(obj):
    local=array(obj.data.vertices,'co',3,np.float32)
    m=np.asarray(obj.matrix_world,float)
    world=local.astype(float)@m[:3,:3].T+m[:3,3]
    obj.data.calc_loop_triangles()
    tri=array(obj.data.loop_triangles,'vertices',3,np.int32)
    return local,world,tri
def evaluated(obj,dg):
    ev=obj.evaluated_get(dg);mesh=ev.to_mesh(preserve_all_data_layers=True,depsgraph=dg)
    try:
        p=array(mesh.vertices,'co',3,np.float32).astype(float);m=np.asarray(ev.matrix_world,float)
        p=p@m[:3,:3].T+m[:3,3];mesh.calc_loop_triangles()
        return p,array(mesh.loop_triangles,'vertices',3,np.int32)
    finally:ev.to_mesh_clear()
def record(p,t):
    tp=p[t];n=np.cross(tp[:,1]-tp[:,0],tp[:,2]-tp[:,0]);n/=np.maximum(np.linalg.norm(n,axis=1)[:,None],1e-30)
    return {'points':p,'triangles':t,'triangle_points':tp,'triangle_normals':n,'regions':np.full(len(t),'body',object),'bvh':BVHTree.FromPolygons(p.tolist(),t.tolist(),all_triangles=True)}
def nearest_dist(points,tree):return np.asarray([tree.find_nearest(Vector(p))[3] for p in points],float)
def seg_distance(p,segs):
    d=np.full(len(p),np.inf)
    for a,b in segs:
        ab=b-a;v=np.clip((p-a)@ab/max(float(ab@ab),1e-30),0,1)
        d=np.minimum(d,np.linalg.norm(p-(a+v[:,None]*ab),axis=1))
    return d
def inside(p,tri,tol=1e-7):
    a,b,c=tri;v0=b-a;v1=c-a;v2=p-a
    aa=v0@v0;bb=v0@v1;cc=v1@v1;dd=v2@v0;ee=v2@v1;den=aa*cc-bb*bb
    if abs(den)<1e-25:return False
    u=(cc*dd-bb*ee)/den;v=(aa*ee-bb*dd)/den
    return u>=-tol and v>=-tol and u+v<=1+tol
def triangle_line(a,b):
    found=[]
    for t,o in ((a,b),(b,a)):
        normal=np.cross(o[1]-o[0],o[2]-o[0]);norm=np.linalg.norm(normal)
        if norm<1e-20:continue
        normal/=norm;ds=(t-o[0])@normal
        for i,j in ((0,1),(1,2),(2,0)):
            if abs(ds[i])<1e-10 and inside(t[i],o):found.append(t[i])
            if ds[i]*ds[j]<0:
                p=t[i]+(t[j]-t[i])*ds[i]/(ds[i]-ds[j])
                if inside(p,o):found.append(p)
    if len(found)<2:return None
    pp=np.asarray(found);dist=np.linalg.norm(pp[:,None]-pp[None,:],axis=2);i,j=np.unravel_index(np.argmax(dist),dist.shape)
    return np.asarray([pp[i],pp[j]]) if dist[i,j]>1e-9 else None
def contact_segments(body_rec,mouth_rec):
    pairs=mouth_rec['bvh'].overlap(body_rec['bvh']);segs=[];bodyids=[];mouthids=[]
    for mi,bi in pairs:
        seg=triangle_line(mouth_rec['triangle_points'][mi],body_rec['triangle_points'][bi])
        if seg is not None:segs.append(seg);bodyids.extend(body_rec['triangles'][bi]);mouthids.extend(mouth_rec['triangles'][mi])
    return np.asarray(segs,float).reshape((-1,2,3)),np.unique(bodyids).astype(np.int32),np.unique(mouthids).astype(np.int32)
def ray(tree,origin,direction):
    p,n,i,d=tree.ray_cast(Vector(origin),Vector(direction),6)
    return (np.asarray(p,float),np.asarray(n,float)) if p is not None else (np.full(3,np.nan),np.full(3,np.nan))

def cache_eye_boundaries(root):
    """Supplemental actual intersection data, with no protocol/mask changes."""
    out=root/'baseline';target=out/'eye_contact_boundaries.npz'
    if target.exists():raise RuntimeError('Eye boundary cache already exists; immutable')
    snap=json.loads((out/'snapshot.json').read_text(encoding='utf-8'));a=np.load(out/'baseline_arrays.npz')
    bpre=snap['objects'][BODY]['array_prefix'];br=record(a[bpre+'_raw_world'],a[bpre+'_triangles']);result={};meta={'source_sha256':SOURCE_SHA,'state':'Raw','definition':'actual baseline eye/body triangle intersection, with actual source triangle vertices and fixed frontal visibility selection','sides':{}}
    for side in ('L','R'):
        name=side+' concentric iris and pupil';pre=snap['objects'][name]['array_prefix'];er=record(a[pre+'_raw_world'],a[pre+'_triangles']);segments,bids,eids=contact_segments(br,er)
        visible=[]
        for seg in segments:
            center=seg.mean(0);bh,_=ray(br['bvh'],(center[0],-3,center[2]),(0,1,0));eh,_=ray(er['bvh'],(center[0],-3,center[2]),(0,1,0))
            visible.append(bool(np.isfinite(bh).all() and np.isfinite(eh).all() and center[1]<=bh[1]+.001 and center[1]<=eh[1]+.001))
        visible=np.asarray(visible,bool);result[side+'_segments']=segments;result[side+'_body_vertex_ids']=bids;result[side+'_eye_vertex_ids']=eids;result[side+'_frontal_visible_boundary_segments']=segments[visible]
        meta['sides'][side]={'body_object':BODY,'eye_object':name,'actual_contact_segment_count':len(segments),'frontal_visible_boundary_segment_count':int(visible.sum()),'body_vertex_id_count':len(bids),'eye_vertex_id_count':len(eids),'body_contact_vertices_all_frozen':bool(a['body_frozen_mask'][bids].all()),'visibility_epsilon_world':.001,'visible_equal_area_metrics':'baseline/snapshot.json.eye_metrics uses independently fixed .0003 world-zero frontal rays'}
    np.savez_compressed(target,**result);meta['arrays_sha256']=sha(target);dump(out/'eye_contact_boundaries.json',meta)
    manifest=json.loads((out/'manifest.json').read_text(encoding='utf-8'));manifest['files'][target.name]=sha(target);manifest['files']['eye_contact_boundaries.json']=sha(out/'eye_contact_boundaries.json');dump(out/'manifest.json',manifest)
    print('C3_EYE_BOUNDARIES_OK',json.dumps(meta,ensure_ascii=False),flush=True)

def finalize_cache(root,revise_anatomical_interval=False):
    """Finish cache-only definitions after initial anatomical conflict audit.

    This opens no source or candidate .blend. It is allowed only before the
    first candidate and before the protocol is marked frozen.
    """
    out=root/'baseline';snap=json.loads((out/'snapshot.json').read_text(encoding='utf-8'));protocol=json.loads((root/'measurement_protocol.json').read_text(encoding='utf-8'))
    assert not snap.get('protocol_frozen') or revise_anatomical_interval,'C3 baseline definitions already frozen'
    assert not list(root.glob('candidate_*/*.blend')),'Cannot revise a mask after candidate generation'
    old=np.load(out/'baseline_arrays.npz');arrays={k:old[k] for k in old.files};old.close()
    pre=snap['objects'][BODY]['array_prefix'];p=arrays[pre+'_raw_world'];t=arrays[pre+'_triangles'];scope=protocol['scope']
    if revise_anatomical_interval:
        # Diagnostic-only correction authorized before any candidate. The
        # endpoint stays fixed, making the release wider instead of narrower.
        scope['mouth_freeze_distance']=float(snap['contacts']['maximum_contact_vertex_curve_distance']+1e-6)
        snap['contacts']['freeze_distance']=scope['mouth_freeze_distance']
        snap['contacts']['freeze_definition']='maximum distance of actual contact-triangle body vertices to true intersection curve, plus 1e-6 world numerical margin; all exact contact vertex IDs independently frozen'
    # Shoulder-root Z is 1.875. Keep the entire preexisting belly paint reaching
    # Z1.813 untouched by starting the shoulder release at Z1.84.
    scope['lower_release_Z']=1.84;scope['lower_full_Z']=1.93
    zweight=q((p[:,2]-1.84)/.09)*q((scope['highest_crown_Z']-p[:,2])/.025)
    mw=q((arrays['body_mouth_distance']-scope['mouth_freeze_distance'])/(scope['mouth_release_end']-scope['mouth_freeze_distance']))
    ew=q((arrays['body_eye_distance']-scope['eye_freeze_surface_distance'])/(scope['eye_release_surface_distance']-scope['eye_freeze_surface_distance']))
    lw=1-q((arrays['body_arm_weight']-.18)/.32)
    weight=zweight*mw*ew*lw;weight[arrays['body_contact_vertex_ids']]=0.;weight[arrays['body_crown_vertex_ids']]=0.
    _,coincident_inverse,coincident_counts=np.unique(p,axis=0,return_inverse=True,return_counts=True)
    group_min=np.full(len(coincident_counts),np.inf);np.minimum.at(group_min,coincident_inverse,weight)
    previous_weight=weight.copy();weight=group_min[coincident_inverse];arrays['body_coincident_group_ids']=coincident_inverse.astype(np.int32)
    snap['coincident_vertex_scope']={'exact_world_groups_with_multiple_vertices':int((coincident_counts>1).sum()),'vertices_whose_scope_weight_was_reduced':int((weight<previous_weight).sum()),'definition':'all exact-world-coincident source vertices share minimum scope weight; any frozen member freezes entire group'}
    arrays['body_scope_weight']=weight;arrays['body_release_weight']=np.where((weight>0)&(weight<1),weight,0);arrays['body_frozen_mask']=weight==0
    normal=np.zeros_like(p);tp=p[t];cross=np.cross(tp[:,1]-tp[:,0],tp[:,2]-tp[:,0])
    for j in range(3):np.add.at(normal,t[:,j],cross)
    normal/=np.maximum(np.linalg.norm(normal,axis=1)[:,None],1e-30);arrays['body_vertex_world_normals']=normal;arrays['body_world_normals']=normal
    grids=np.load(out/'surface_grid.npz');gd={k:grids[k] for k in grids.files};grids.close()
    ids=gd['nearest_body_vertex_ids'];grid=gd['grid'];segs=arrays['mouth_contact_segments'];md=seg_distance(grid.reshape((-1,3)),segs).reshape(grid.shape[:2]);ed=np.full(grid.shape[:2],np.inf)
    for fit in snap['eye_fits'].values():ed=np.minimum(ed,np.abs(np.linalg.norm(grid-np.asarray(fit['center']),axis=2)-fit['radius']))
    gd['weight_grid']=q((grid[:,:,2]-1.84)/.09)*q((scope['highest_crown_Z']-grid[:,:,2])/.025)*(1-q((arrays['body_arm_weight'][ids]-.18)/.32))*q((md-scope['mouth_freeze_distance'])/(scope['mouth_release_end']-scope['mouth_freeze_distance']))*q((ed-scope['eye_freeze_surface_distance'])/(scope['eye_release_surface_distance']-scope['eye_freeze_surface_distance']))
    np.savez_compressed(out/'surface_grid.npz',**gd)
    np.savez_compressed(out/'baseline_arrays.npz',**arrays)
    np.savez_compressed(out/'body_field_input.npz',world=p,local=arrays[pre+'_raw_local'],triangles=t,vertex_world_normals=normal,weight=weight,eye_local=arrays['body_eye_local_mask'],frozen=arrays['body_frozen_mask'],centers=gd['centers'],zs=gd['zs'])
    snap['scope'].update(scope_vertex_count=int((weight>0).sum()),frozen_vertex_count=int((weight==0).sum()),release_vertex_count=int(((weight>0)&(weight<1)).sum()))
    protocol['protection_conflicts']={'initial_lower_release_conflict':'initial shoulder bone based Z1.735 release overlapped belly paint upper Z1.813; identified before any candidate and corrected once to Z1.84/1.93','mouth_underturn_not_fully_frozen':True,'notes':'actual triangle-contact vertices and small necessary neighborhood frozen, plus fixed 0.035 world release; full masks frozen before first candidate; no subsequent release shortening'}
    reference_file=root/'c3_reference_contour_protocol.json'
    if reference_file.exists():protocol['projection']['reference_contour_protocol_file']=str(reference_file);protocol['projection']['reference_contour_protocol_sha256']=sha(reference_file)
    original_profiles=np.load(out/'longitudinal_front.npz')
    protocol['sections']['longitudinal_Z_ranges']={str(float(x)):[1.95,float(min(2.62,profile[np.isfinite(profile).all(1),2].max()-.015))] for x,profile in zip(original_profiles['xs'],original_profiles['points'])}
    protocol['sections']['longitudinal_Z_range_definition']='once-fixed baseline front-ray support per X plane, ending 0.015 world below last measured head surface or Z2.62, whichever is lower; no candidate-specific cropping'
    protocol['sections']['measurement_helper_file']=str(HERE/'c3_measurement_utils.py');protocol['sections']['measurement_helper_sha256']=sha(HERE/'c3_measurement_utils.py')
    if revise_anatomical_interval:
        central_contact=arrays['mouth_contact_segments'].reshape((-1,3));central_contact=central_contact[np.abs(central_contact[:,0])<.002]
        assert len(central_contact)>20,'Insufficient real central mouth contact samples'
        upper=float(np.median(central_contact[:,2]));protocol['sections']['mouth_underturn_Z_range']=[2.31,upper]
        protocol['sections']['mouth_underturn_anatomical_definition']='baseline lower-jaw entry at fixed Z2.31 through measured central mouth/body contact-curve median Z; the previous trial Z2.15..2.40 included the full front-neck concavity and was diagnosed before candidate generation'
        protocol['sections']['natural_front_neck_Z_range']=[2.05,2.31]
        protocol['sections']['central_mouth_contact_Z_bounds']=[float(central_contact[:,2].min()),float(central_contact[:,2].max())]
        protocol['protection_conflicts']['mouth_freeze_definition_correction']='before any candidate: initial 0.009604122 radius unnecessarily added one median edge to actual contact-face vertex bound; corrected once to bound+1e-6=0.005172620 while retaining release endpoint 0.044604122, widening the release; true mouth-contact face vertices remain independently frozen'
        focal={}
        for label,vid in (('central_peak',293458),('X0_08_peak',225368),('X0_12_peak',225385)):
            focal[label]={'original_vertex_index':vid,'mouth_curve_distance':float(arrays['body_mouth_distance'][vid]),'scope_weight':float(weight[vid]),'world_position':p[vid].tolist()}
        protocol['protection_conflicts']['focal_turn_anchors']={'samples':focal,'constraint':'central peak remains nearly immovable within the real-contact release; target distributes turn downward and into side cheeks, without requiring removal of true contact-adjacent peak'}
    from c3_measurement_utils import measure_sections
    summaries={}
    for state in ('Raw','Idle1'):
        pp=p if state=='Raw' else arrays[pre+'_Idle1_world'];tt=t if state=='Raw' else arrays[pre+'_Idle1_triangles']
        measure=measure_sections(pp,tt,protocol);dump(out/('measurements_'+state+'.json'),measure)
        summaries[state]={x:{k:v for k,v in rec.items() if k!='samples'} for x,rec in measure['longitudinal'].items()}
    snap['section_measurement_summary']=summaries
    # Actual distances and weights at fixed front-profile points make mouth
    # protection/target-turn overlap reviewable before the field is fitted.
    fronts=np.load(out/'longitudinal_front.npz');rows=[]
    from mathutils.kdtree import KDTree
    kd=KDTree(len(p))
    for i,pt in enumerate(p):kd.insert(Vector(pt),i)
    kd.balance()
    for x,profile in zip(fronts['xs'],fronts['points']):
        for pt in profile:
            if not np.isfinite(pt).all() or not 2.12<=pt[2]<=2.43:continue
            vid=kd.find(Vector(pt))[1];rows.append([x,*pt,float(weight[vid]),float(arrays['body_mouth_distance'][vid]),vid])
    np.savetxt(out/'mouth_underturn_protection_audit.csv',rows,delimiter=',',header='plane_X,world_X,world_Y,world_Z,nearest_vertex_weight,mouth_curve_distance,nearest_original_vertex_index',comments='',fmt='%.12g')
    dump(root/'measurement_protocol.json',protocol)
    snap['array_file_sha256']=sha(out/'baseline_arrays.npz');snap['measurement_protocol_sha256']=sha(root/'measurement_protocol.json');snap['protocol_frozen']=True
    dump(out/'snapshot.json',snap)
    np.savetxt(out/'body_scope_release_frozen.csv',np.column_stack((np.arange(len(p)),weight,arrays['body_release_weight'],arrays['body_frozen_mask'].astype(int),arrays['body_eye_local_mask'].astype(int),arrays['body_mouth_distance'])),delimiter=',',header='original_vertex_index,scope_weight,release_weight,frozen,eye_local,mouth_curve_distance',comments='',fmt=['%d','%.12g','%.12g','%d','%d','%.12g'])
    assert sha(SOURCE)==SOURCE_SHA
    dump(out/'manifest.json',{'source_unchanged':True,'source_sha256':SOURCE_SHA,'protocol_frozen':True,'files':{f.name:sha(f) for f in out.iterdir() if f.is_file() and f.name!='manifest.json'}})
    print('C3_CACHE_FINALIZED',json.dumps({'scope':snap['scope'],'central_Raw':summaries['Raw']['0.0'],'root':str(root)},ensure_ascii=False),flush=True)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--root',type=Path,required=True);ap.add_argument('--finalize-cache',action='store_true');ap.add_argument('--revise-anatomical-interval',action='store_true');ap.add_argument('--cache-eye-boundaries',action='store_true')
    args=ap.parse_args(sys.argv[sys.argv.index('--')+1:]);root=args.root.resolve();out=root/'baseline'
    if args.cache_eye_boundaries:return cache_eye_boundaries(root)
    if args.finalize_cache:return finalize_cache(root,args.revise_anatomical_interval)
    assert sha(SOURCE)==SOURCE_SHA,'Pinned E2 hash differs'
    if (out/'snapshot.json').exists():raise RuntimeError('Existing baseline snapshot; do not reopen E2 or overwrite')
    out.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE),load_ui=False)
    meshes=sorted((o for o in bpy.data.objects if o.type=='MESH'),key=lambda o:o.name)
    assert BODY in bpy.data.objects and MOUTH in bpy.data.objects
    snap={'schema_version':1,'source':str(SOURCE),'source_sha256':SOURCE_SHA,'objects':{},'preserved_links':preserved_links(),'rig_snapshot':rig_snapshot(),'scene_custom_properties':{k:scalar(v) for k,v in bpy.context.scene.items()},'poses':POSES}
    arrays={};records={}
    for i,obj in enumerate(meshes):
        pre=f'obj{i:02d}';local,p,t=raw(obj);records[obj.name]=record(p,t)
        arrays[pre+'_raw_local']=local;arrays[pre+'_raw_world']=p;arrays[pre+'_triangles']=t
        snap['objects'][obj.name]={'array_prefix':pre,'fingerprint':fingerprint(obj),'transform':np.asarray(obj.matrix_world,float).tolist(),'custom_properties':{k:scalar(v) for k,v in obj.items()},'mesh_custom_properties':{k:scalar(v) for k,v in obj.data.items()},'attributes':[{'name':a.name,'domain':a.domain,'type':a.data_type} for a in obj.data.attributes],'normal_settings':normal_snapshot(obj),'bounds_world':[p.min(0).tolist(),p.max(0).tolist()]}
    p=records[BODY]['points'];t=records[BODY]['triangles'];br=records[BODY];mr=records[MOUTH]
    # Early caches let the field preparer inspect real saved geometry while this one read continues.
    np.savez_compressed(out/'preliminary_raw.npz',body_world=p,body_local=arrays[snap['objects'][BODY]['array_prefix']+'_raw_local'],body_triangles=t,mouth_world=mr['points'],mouth_triangles=mr['triangles'])
    dump(out/'preliminary_objects.json',snap)
    front_z=np.arange(1.90,2.651,.001)
    xs=np.asarray([-.18,-.12,-.08,-.04,0,.04,.08,.12,.18])
    front=np.asarray([[ray(br['bvh'],(x,-3,z),(0,1,0))[0] for z in front_z] for x in xs])
    np.savez_compressed(out/'longitudinal_front.npz',xs=xs,zs=front_z,points=front)
    print('C3_EARLY_PROFILE_READY',str(out),flush=True)
    segs,bodyids,mouthids=contact_segments(br,mr)
    assert len(segs),'No actual mouth/body contact intersection; explicit diagnosis required'
    arrays['mouth_contact_segments']=segs;arrays['body_contact_vertex_ids']=bodyids;arrays['mouth_contact_vertex_ids']=mouthids
    dist=seg_distance(p,segs);arrays['body_mouth_distance']=dist
    # Local edge spacing and true triangle contact determine necessary frozen neighborhood.
    edges=array(bpy.data.objects[BODY].data.edges,'vertices',2,np.int32)
    near_edge=np.isin(edges,bodyids).any(1)
    contact_spacing=float(np.median(np.linalg.norm(p[edges[near_edge,1]]-p[edges[near_edge,0]],axis=1)))
    frozen_distance=float(dist[bodyids].max()+contact_spacing)
    release_end=frozen_distance+max(.035,6*contact_spacing)
    mouth_weight=q((dist-frozen_distance)/(release_end-frozen_distance))
    crown_z=float(p[:,2].max());crownids=np.flatnonzero(p[:,2]>=crown_z-1e-7)
    arm=np.asarray([sum(g.weight for g in v.groups if bpy.data.objects[BODY].vertex_groups[g.group].name.startswith(('UpperArm.','Forearm.'))) for v in bpy.data.objects[BODY].data.vertices])
    shoulder_heads=[float(b.head_local.z) for b in bpy.data.objects['Naiwa_Rig'].data.bones if b.name.startswith('UpperArm.')]
    shoulder_z=float(np.mean(shoulder_heads));low=shoulder_z-.14;low_full=shoulder_z+.015
    zweight=q((p[:,2]-low)/(low_full-low))*q((crown_z-p[:,2])/.025)
    limbweight=1-q((arm-.18)/(.5-.18))
    # Freeze actual eye/skin contact neighborhood, with a wide fixed release.
    eye_distance=np.full(len(p),np.inf);eye_fits={}
    for side in ('L','R'):
        fit=fit_sphere(records[side+' concentric iris and pupil']['points']);eye_fits[side]=fit
        c=np.asarray(fit['center']);eye_distance=np.minimum(eye_distance,np.abs(np.linalg.norm(p-c,axis=1)-fit['radius']))
    eye_freeze=max(contact_spacing*1.5,.0015);eye_release=.027
    eyeweight=q((eye_distance-eye_freeze)/(eye_release-eye_freeze));eye_local=eye_distance<=eye_release
    weight=zweight*limbweight*mouth_weight*eyeweight
    weight[crownids]=0.;weight[bodyids]=0.;frozen=weight==0
    arrays.update(body_scope_weight=weight,body_release_weight=np.where((weight>0)&(weight<1),weight,0),body_frozen_mask=frozen,body_eye_local_mask=eye_local,body_crown_vertex_ids=crownids.astype(np.int32),body_arm_weight=arm,body_eye_distance=eye_distance)
    # The unmodified hidden silhouette object is deliberately outside C3.
    snap['scope']={'object':BODY,'scope_vertex_count':int((weight>0).sum()),'frozen_vertex_count':int(frozen.sum()),'release_vertex_count':int(((weight>0)&(weight<1)).sum()),'all_other_meshes_frozen':True,'priority':['domain exterior and distal limbs','actual mouth contact neighborhood','actual eye contact neighborhood','crown highest anchors','smooth release','head neck shoulder deformation']}
    snap['contacts']={'method':'actual triangle-triangle surface intersection, not mouth center or bounds','mouth_contact_segment_count':len(segs),'body_contact_vertex_ids_array':'body_contact_vertex_ids','mouth_contact_vertex_ids_array':'mouth_contact_vertex_ids','segments_array':'mouth_contact_segments','contact_local_median_edge_length':contact_spacing,'freeze_distance':frozen_distance,'release_end':release_end,'maximum_contact_vertex_curve_distance':float(dist[bodyids].max()),'prior_saved_protection_definition':'No complete executable mouth-protection definition in E2 metadata; historic parameter text is not adopted as a mask'}
    snap['eye_fits']=eye_fits
    snap['eye_metrics']={}
    for name,(action,frame) in POSES.items():
        rig=bpy.data.objects['Naiwa_Rig'];rig.animation_data.action=bpy.data.actions[action];rig.animation_data.use_nla=False;rig.data.pose_position='POSE';bpy.context.scene.frame_set(frame);bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get()
        for obj in meshes:
            pre=snap['objects'][obj.name]['array_prefix'];ep,et=evaluated(obj,dg)
            arrays[pre+'_'+name+'_world']=ep;arrays[pre+'_'+name+'_triangles']=et
        if name=='Idle1':
            for side in ('L','R'):snap['eye_metrics'][side]=visible_metrics(bpy.data.objects[side+' concentric iris and pupil'].evaluated_get(dg),bpy.data.objects[BODY].evaluated_get(dg),step=.0003)
        print('C3_BASELINE_STATE',name,flush=True)
    Ds=[float(np.sqrt(4*snap['eye_metrics'][s]['aperture']['area']/np.pi)) for s in ('L','R')];D=float(np.mean(Ds))
    # Actual raw radial sections; a shared center(z) is inferred from front/rear rays.
    zs=np.arange(low,2.641,.005);theta=np.arange(128)*2*np.pi/128
    centers=[];grid=[];normals=[]
    for z in zs:
        fp,_=ray(br['bvh'],(0,-3,z),(0,1,0));rp,_=ray(br['bvh'],(0,3,z),(0,-1,0));c=(fp+rp)/2;centers.append(c)
        grow=[];nrow=[]
        for th in theta:
            direction=np.asarray([np.sin(th),-np.cos(th),0.]);pt,no=ray(br['bvh'],c+direction*3,-direction);grow.append(pt);nrow.append(no)
        grid.append(grow);normals.append(nrow)
    grid=np.asarray(grid);normals=np.asarray(normals);centers=np.asarray(centers)
    gd=seg_distance(grid.reshape((-1,3)),segs).reshape(grid.shape[:2]);ge=np.full(grid.shape[:2],np.inf)
    for fit in eye_fits.values():ge=np.minimum(ge,np.abs(np.linalg.norm(grid-np.asarray(fit['center']),axis=2)-fit['radius']))
    # Grid masks match the anatomical distance and Z definitions; arm weight interpolated from nearest real surface vertex.
    from mathutils.kdtree import KDTree
    kd=KDTree(len(p))
    for i,pt in enumerate(p):kd.insert(Vector(pt),i)
    kd.balance();ids=np.asarray([kd.find(Vector(pt))[1] for pt in grid.reshape((-1,3))]).reshape(grid.shape[:2])
    gw=q((grid[:,:,2]-low)/(low_full-low))*q((crown_z-grid[:,:,2])/.025)*(1-q((arm[ids]-.18)/.32))*q((gd-frozen_distance)/(release_end-frozen_distance))*q((ge-eye_freeze)/(eye_release-eye_freeze))
    np.savez_compressed(out/'surface_grid.npz',grid=grid,normals=normals,zs=zs,theta=theta,centers=centers,weight_grid=gw,nearest_body_vertex_ids=ids)
    protocol={'schema_version':1,'baseline_source_sha256':SOURCE_SHA,'coordinates':{'system':'world','up':[0,0,1],'left_right':[1,0,0],'anatomical_front':[0,-1,0],'evidence':'mouth and visible-eye surface lie toward negative world Y; rig spine follows positive Z; two eye centers differ along X','all_world_matrices':{n:v['transform'] for n,v in snap['objects'].items()}},'states':{'Raw':'same saved local vertices transformed in float64; armature not evaluated','Idle1':{'action':'Naiwa_Idle','frame':1,'pose_position':'POSE','NLA':False},'deformation_checks':POSES},'scale':{'definition':'mean frontal ray-visible eye aperture equal-area diameter','D':D,'left_D':Ds[0],'right_D':Ds[1],'historical_D_mean':.117114,'sampling_step':.0003,'comparison':'new fixed .0003 world-zero sampling E2/candidate; historical values background only'},'scope':{'object':BODY,'lower_release_Z':low,'lower_full_Z':low_full,'highest_crown_Z':crown_z,'crown_release_depth':.025,'limb_freeze_arm_weight':.5,'limb_release_arm_weight':.18,'mouth_freeze_distance':frozen_distance,'mouth_release_end':release_end,'mouth_distance_definition':'shortest 3D world Euclidean distance to baseline actual mouth/body intersection segments','eye_freeze_surface_distance':eye_freeze,'eye_release_surface_distance':eye_release,'all_other_meshes_frozen':True,'weight_formula':'quintic Z boundary * quintic distal-arm release * quintic mouth-contact release * quintic eye-contact release; crown/contact anchors exact zero','arrays':'baseline/baseline_arrays.npz','indices':'original mesh vertex indices, paired with object names via snapshot array_prefix'},'sections':{'longitudinal_X':xs.tolist(),'Z_range':[1.95,2.62],'Z_sampling_step':.0005,'mouth_underturn_Z_range':[2.15,2.40],'resample_arc_step':.0005,'polynomial_degree':3,'fit_half_window_world':.009,'angle80_definition':'unwrap tangent angle then accumulate absolute angle changes; shortest contiguous arc containing 80% total absolute turn in fixed mouth-underturn anatomical interval','historical_protocol_confirmed':False,'historical_0_091_and_29_30':'background only; no direct numeric comparison'},'projection':{'head':{'type':'ORTHO','yaw':[0,45,90],'pitch':0,'target':[0,-.11999999731779099,2.369999885559082],'distance':7,'scale':.85,'resolution':[900,1000]},'reference':{'type':'ORTHO','yaw':50,'pitch':-2.5,'target':[0,-.12,2.425],'distance':7,'scale':.86,'resolution':[900,900],'sensor_fit':'HORIZONTAL'},'registration':{'scale':.41175526202447044,'translation':[245.38813136509236,-17.159665982101984],'formula':'reference_pixel = scale * model_pixel + translation','reference_image':'figure_1_3.jpg 900x1130, original unmodified'},'scan_rows_reference_pixels':[90,180,220,260,300],'neck_error_rows_reference_pixels':[220,260,300],'error_algorithm':'mean absolute horizontal difference of left/right registered body contours at fixed original reference scan rows; candidate/E2 same silhouette segmentation and registration','units':'world for geometry, reference original-image pixels for contour'},'budgets':{'A':.04*D,'B':.08*D,'eye_local':.025*D,'definition':'final same-index same-Raw-state world Euclidean difference relative E2; eye cap is included in overall budget'},'freeze_priority':snap['scope']['priority'],'eye_thresholds':{'diameter_fraction':.01,'center_shift_fraction_of_each_eye_D':.01,'axis_ratio_abs':.02},'protection_conflicts':{'mouth_underturn_not_fully_frozen':True,'notes':'explicit per-section distance/weight inspection stored in baseline; no iterative shortening of release band'}}
    dump(root/'measurement_protocol.json',protocol)
    np.savez_compressed(out/'baseline_arrays.npz',**arrays)
    snap['array_file']='baseline_arrays.npz';snap['array_file_sha256']=sha(out/'baseline_arrays.npz');snap['measurement_protocol_sha256']=sha(root/'measurement_protocol.json')
    dump(out/'snapshot.json',snap)
    # Clear human-readable original vertex index lists, complementing compact exact arrays.
    np.savetxt(out/'body_scope_release_frozen.csv',np.column_stack((np.arange(len(p)),weight,arrays['body_release_weight'],frozen.astype(int),eye_local.astype(int),dist)),delimiter=',',header='original_vertex_index,scope_weight,release_weight,frozen,eye_local,mouth_curve_distance',comments='',fmt=['%d','%.12g','%.12g','%d','%d','%.12g'])
    assert sha(SOURCE)==SOURCE_SHA
    dump(out/'manifest.json',{'source_unchanged':True,'source_sha256':SOURCE_SHA,'files':{f.name:sha(f) for f in out.iterdir() if f.is_file() and f.name!='manifest.json'}})
    print('C3_BASELINE_OK',json.dumps({'D':D,'scope':snap['scope'],'mouth':snap['contacts'],'root':str(root)},ensure_ascii=False),flush=True)

if __name__=='__main__':main()
