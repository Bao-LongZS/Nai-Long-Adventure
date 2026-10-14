"""Independently fit saved raw/evaluated eyes and compare pinned non-eye data."""
import bpy,sys,json,math,hashlib
from pathlib import Path
import numpy as np
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from geometry_diagnostic_utils import mesh_record,section_object,face_point,components
from eye_projection_measure import visible_metrics
from validate_lip_eye_refinement import attribute,coordinates,weight_hash
SOURCE=ROOT/'versions/20261005_head_side_continuity/Naiwa_Character.blend'
OUT=ROOT/'versions/20261006_spherical_eyes'
SHA='a95a5c7efb25cb6f379897dd6ec4af98a201bc2619dc8caa2bb5b5de8ea3dfd5'
D=.11724481274201151;S=.23462580179330159

from saved_delivery_metadata import preserved_links, differences

def connected_subset(triangles,selected):
    nodes=set(map(int,np.flatnonzero(selected)));adj={i:set() for i in nodes}
    for face in triangles:
        kept=[int(i) for i in face if int(i) in nodes]
        for i in kept:adj[i].update(j for j in kept if j!=i)
    counts=[]
    while nodes:
        todo=[nodes.pop()];count=0
        while todo:
            i=todo.pop();count+=1
            unseen=adj[i]&nodes;nodes-=unseen;todo.extend(unseen)
        counts.append(count)
    return sorted(counts,reverse=True)

def glint_contact(glint,center,radius,dg):
    rec=mesh_record(glint,dg);dist=np.linalg.norm(rec['points']-center,axis=1)-radius
    # A glint is a separate small solid. Its boundary must cross the fitted
    # globe, so the visible part is attached rather than a hovering shell.
    assert float(dist.min())<0<float(dist.max()),(glint.name,'Glint misses globe',dist.min(),dist.max())
    crossing=dist[rec['triangles']]
    count=int(np.count_nonzero((crossing.min(1)<=0)&(crossing.max(1)>=0)))
    assert count>0
    origin=np.asarray(glint.matrix_world.translation,float)
    glint_axis=np.asarray(glint.matrix_world.to_3x3()@Vector((0,1,0)),float)
    glint_axis/=np.linalg.norm(glint_axis)
    normal=(origin-center)/np.linalg.norm(origin-center)
    assert np.dot(glint_axis,normal)>.99999,'Glint normal is not aligned to sphere'
    return {'minimum_signed_vertex_sphere_distance':float(dist.min()),
        'maximum_signed_vertex_sphere_distance':float(dist.max()),
        'crossing_triangle_count':count,'origin_signed_sphere_distance':float(np.linalg.norm(origin-center)-radius),
        'local_Y_axis_dot_sphere_normal':float(np.dot(glint_axis,normal)),
        'separate_mesh_excluded_from_sphere_and_aperture_measurements':True}

def open_pose(path):
    bpy.ops.wm.open_mainfile(filepath=str(path));rig=bpy.data.objects['Naiwa_Rig']
    rig.animation_data.action=bpy.data.actions['Naiwa_Idle'];rig.animation_data.use_nla=False
    bpy.context.scene.frame_set(1);bpy.context.view_layer.update()

def untouched():
    out={};bones=set(bpy.data.objects['Naiwa_Rig'].data.bones.keys())
    for o in bpy.data.objects:
        if o.type!='MESH' or o.name.startswith(('L concentric','R concentric','L eye glint','R eye glint')):continue
        out[o.name]={'p':hashlib.sha256(coordinates(o).tobytes()).hexdigest(),
            'e':hashlib.sha256(attribute(o.data.edges,'vertices',2,np.int32).tobytes()).hexdigest(),
            'l':hashlib.sha256(attribute(o.data.loops,'vertex_index',1,np.int32).tobytes()).hexdigest(),
            'w':weight_hash(o,bones),'m':[list(r) for r in o.matrix_world],
            'c':{a.name:hashlib.sha256(attribute(a.data,'color',4,np.float32).tobytes()).hexdigest() for a in o.data.color_attributes}}
    rig=bpy.data.objects['Naiwa_Rig'];out['rig']={'bones':[(b.name,list(b.head_local),list(b.tail_local),[list(r) for r in b.matrix_local],b.parent.name if b.parent else None) for b in rig.data.bones],
        'actions':[(a.name,list(a.frame_range)) for a in bpy.data.actions]}
    return out

def fit_sphere(points):
    origin=points.mean(0);p=points-origin
    a=np.column_stack((2*p,np.ones(len(p))));v=np.sum(p*p,axis=1)
    sol=np.linalg.lstsq(a,v,rcond=None)[0];center=origin+sol[:3];radius=math.sqrt(sol[3]+np.dot(sol[:3],sol[:3]))
    residual=np.abs(np.linalg.norm(points-center,axis=1)-radius)
    assert residual.max()/radius<=1e-4
    return {'fitted_center':center.tolist(),'fitted_radius':radius,'maximum_absolute_radius_residual':float(residual.max()),
        'maximum_relative_radius_residual':float(residual.max()/radius)},center,radius

def integrity(rec,C):
    tris=rec['triangles'];tp=rec['triangle_points'];edges={};canonical=set()
    for face in tris:
        f=tuple(sorted(map(int,face)));assert f not in canonical;canonical.add(f)
        for a,b in zip(face,np.roll(face,-1)):
            key=tuple(sorted((int(a),int(b))));edges[key]=edges.get(key,0)+1
    assert set(edges.values())=={2}
    area=np.linalg.norm(np.cross(tp[:,1]-tp[:,0],tp[:,2]-tp[:,0]),axis=1)
    assert area.min()>1e-12
    dots=np.sum(rec['triangle_normals']*(tp.mean(1)-C),axis=1);assert dots.min()>0
    # All vertices inside every outward triangle supporting half-space proves
    # a convex closed mesh, excluding nonadjacent self intersections.
    points=rec['points']-C;planes=np.sum(rec['triangle_normals']*(tp[:,0]-C),axis=1)
    max_out=0.
    for start in range(0,len(planes),256):
        deviations=rec['triangle_normals'][start:start+256]@points.T-planes[start:start+256,None]
        max_out=max(max_out,float(deviations.max()))
    float32_tolerance=1e-5*float(np.mean(np.linalg.norm(points,axis=1)))
    assert max_out<float32_tolerance,(max_out,float32_tolerance)
    return {'watertight_all_triangulated_edges_incidence_2':True,'duplicate_triangles':0,
        'degenerate_triangles':0,'outward_normals':True,'convex_halfspace_max_violation':max_out,
        'convex_halfspace_float32_tolerance':float32_tolerance,
        'no_obvious_self_intersection_closed_convex_within_float32_tolerance':True,
        'Euler_characteristic':len(points)-len(edges)+len(tris)}

def center_cut(record,axis,value):
    # Center planes coincide with stored UV rings. Float32 world Z near 2.5
    # has 2.4e-7 quantization; snap coincident endpoints before joining loops.
    epsilon=5e-7;segs=[];seen=set()
    for idx,t in enumerate(record['triangle_points']):
        dist=t[:,axis]-value;dist[np.abs(dist)<epsilon]=0
        if dist.min()>0 or dist.max()<0:continue
        pts=[]
        for i,j in ((0,1),(1,2),(2,0)):
            if dist[i]==0:pts.append(t[i].copy())
            if dist[i]*dist[j]<0:pts.append(t[i]+(t[j]-t[i])*dist[i]/(dist[i]-dist[j]))
        uniq={tuple(np.round(p,7)):p for p in pts};pts=list(uniq.values())
        if len(pts)!=2:continue
        for p in pts:p[axis]=value
        key=tuple(sorted(tuple(np.round(p,7)) for p in pts))
        if key[0]==key[1] or key in seen:continue
        seen.add(key);segs.append({'points':[p.tolist() for p in pts],'region':str(record['regions'][idx]),'triangle_index':idx})
    comps=components(segs,[a for a in range(3) if a!=axis])
    loops=[{k:v for k,v in c.items() if k!='ids'} for c in comps]
    return {'name':record['name'],'segments':segs,'loops':loops,'coincident_plane_epsilon':epsilon}

assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==SHA
open_pose(SOURCE);baseline=untouched();baseline_links=preserved_links()
baseline_glints={s:{'coords':hashlib.sha256(coordinates(bpy.data.objects[s+' eye glint']).tobytes()).hexdigest(),
    'weights':weight_hash(bpy.data.objects[s+' eye glint'],set(bpy.data.objects['Naiwa_Rig'].data.bones.keys()))}
    for s in ('L','R')}
source_palette={s:set(tuple(c.color) for c in bpy.data.objects[s+' concentric iris and pupil'].data.color_attributes['EyeColor'].data) for s in ('L','R')}
original={s:visible_metrics(bpy.data.objects[s+' concentric iris and pupil'],bpy.data.objects['Naiwa unified golden body'],step=.0003) for s in ('L','R')}
summary={'source_sha256':SHA,'source_measurements':original,'candidates':{}}
for candidate in ('A','B'):
    dest=OUT/('candidate_'+candidate);open_pose(dest/'Naiwa_Character.blend')
    assert untouched()==baseline,'Head/body/mouth/rig changed'
    link_differences=differences(baseline_links,preserved_links())
    (OUT/('candidate_'+candidate)/'saved_metadata_differences.json').write_text(json.dumps(link_differences,indent=2),encoding='utf-8')
    assert not link_differences,('Object relationships, modifiers, materials or animation curves changed',link_differences[:8])
    build=json.loads((dest/'spherical_eye_build_report.json').read_text())
    dg=bpy.context.evaluated_depsgraph_get();body=bpy.data.objects['Naiwa unified golden body']
    bodyrec=mesh_record(body,dg);results={};sections=[]
    for side in ('L','R'):
        eye=bpy.data.objects[side+' concentric iris and pupil']
        raw=np.asarray([tuple(eye.matrix_world@v.co) for v in eye.data.vertices],float)
        fit,C,R=fit_sphere(raw);evaluated=mesh_record(eye,dg,eye=True)
        eval_fit,EC,ER=fit_sphere(evaluated['points'])
        assert raw.shape==evaluated['points'].shape
        evaluation_error=float(np.max(np.linalg.norm(raw-evaluated['points'],axis=1)))
        assert evaluation_error<1e-6,'Idle modifiers move eye away from raw sphere'
        assert np.linalg.norm(C-np.array(build['eyes'][side]['sphere_center']))<1e-6
        assert abs(R-build['eyes'][side]['R'])<1e-6
        topo=integrity(evaluated,EC)
        assert topo['Euler_characteristic']==2
        eye.data.calc_loop_triangles()
        raw_triangles=attribute(eye.data.loop_triangles,'vertices',3,np.int32).reshape((-1,3))
        assert np.array_equal(raw_triangles,evaluated['triangles']),'Idle modifier changes eye topology'
        raw_tp=raw[raw_triangles]
        raw_normals=np.cross(raw_tp[:,1]-raw_tp[:,0],raw_tp[:,2]-raw_tp[:,0])
        raw_normals/=np.linalg.norm(raw_normals,axis=1)[:,None]
        raw_topology=integrity({'triangles':raw_triangles,'triangle_points':raw_tp,
            'triangle_normals':raw_normals,'points':raw},C)
        assert raw_topology['Euler_characteristic']==2
        cuts=[]
        for axis in range(3):
            cut=center_cut(evaluated,axis,float(EC[axis]))
            assert len(cut['loops'])==1 and cut['loops'][0]['closed'],(side,axis,[(len(c['points3d']),c['closed']) for c in cut['loops']])
            p=np.asarray(cut['loops'][0]['points3d']);rad=np.linalg.norm(p-EC,axis=1)
            error=float(np.max(np.abs(rad-ER))/ER);assert error<1e-4,error
            cuts.append({'axis':'XYZ'[axis],'plane_value':float(EC[axis]),'maximum_relative_circle_endpoint_residual':error})
            sections.append({'side':side,'plane_axis':'XYZ'[axis],'value':float(EC[axis]),'C':EC.tolist(),'R':ER,'object':cut})
        stats=visible_metrics(eye.evaluated_get(dg),body.evaluated_get(dg),step=.0003)
        d=math.sqrt(4*stats['aperture']['area']/math.pi);ratio=math.sqrt(stats['black']['area']/stats['aperture']['area'])
        assert abs(d/D-1)<=.03;assert abs(stats['aperture']['center'][1]-original[side]['aperture']['center'][1])<=.02*D
        assert abs(ratio-.53)<=.02
        colors=eye.data.color_attributes['EyeColor'];labels=[]
        assert set(tuple(c.color) for c in colors.data)==source_palette[side],'Eye palette differs from fixed source'
        edge_regions={}
        for poly in eye.data.polygons:
            region='black' if colors.data[poly.loop_start].color[1]<.04 else 'green'
            for key in poly.edge_keys:edge_regions.setdefault(tuple(sorted(key)),set()).add(region)
        shared=[e for e,r in edge_regions.items() if len(r)==2];assert len(shared)==96
        boundary_ids=sorted(set(i for e in shared for i in e))
        boundary=raw[boundary_ids]
        # A world-forward pupil cap has its common black/green latitude in one
        # Y plane. Shared vertex indices establish zero geometric separation.
        latitude_error=float(np.ptp(boundary[:,1]))
        assert latitude_error<1e-6
        assert len(set(tuple(p.vertices) for p in eye.data.polygons))==len(eye.data.polygons)
        assert len(np.unique(np.round(raw,7),axis=0))==len(raw),'Duplicate sphere vertices'
        eye_weights=[[(eye.vertex_groups[g.group].name,g.weight) for g in v.groups] for v in eye.data.vertices]
        assert all(w==[('Torso',1.0)] for w in eye_weights)
        exposed=buried=0;min_gap=math.inf;gaps=[];posterior_margins=[]
        for p in evaluated['points']:
            skin=face_point(bodyrec,p[0],p[2])
            assert skin is not None,'Eye extends outside body frontal footprint'
            gap=skin[1]-p[1]
            gaps.append(gap)
            if gap>0:exposed+=1;min_gap=min(min_gap,gap)
            else:buried+=1
            posterior,_,_,_=bodyrec['bvh'].ray_cast(Vector((float(p[0]),3.,float(p[2]))),Vector((0,-1,0)),6)
            assert posterior is not None
            posterior_margins.append(float(posterior.y-p[1]))
        assert exposed>100 and buried>100,'Installation did not intersect body'
        contacts=evaluated['bvh'].overlap(bodyrec['bvh'])
        assert len(contacts)>0,'Eye surface floats without body contact'
        assert min(posterior_margins)>0,'Back globe protrudes through rear of body'
        exposure_components=connected_subset(evaluated['triangles'],np.asarray(gaps)>0)
        assert len(exposure_components)==1,'Exposed front is disconnected'
        glint=bpy.data.objects[side+' eye glint']
        assert hashlib.sha256(coordinates(glint).tobytes()).hexdigest()==baseline_glints[side]['coords']
        assert weight_hash(glint,set(bpy.data.objects['Naiwa_Rig'].data.bones.keys()))==baseline_glints[side]['weights']
        highlight=glint_contact(glint,EC,ER,dg)
        results[side]={'raw_world_fit':fit,'raw_mesh_integrity':raw_topology,
            'Idle_frame1_evaluated_world_fit':eval_fit,'integrity':topo,'raw_Idle_eye_topology_exact':True,
            'maximum_raw_to_Idle_frame1_vertex_displacement':evaluation_error,
            'orthogonal_center_sections':cuts,'frontal_metrics':stats,'actual_visible_D':d,'actual_pupil_ratio':ratio,
            'shared_black_green_edges':len(shared),'black_independent_step':0.,'effective_weights':'Torso 1.0',
            'shared_black_green_boundary_Y_plane_range':latitude_error,'glint_contact':highlight,
            'source_eye_colors_exact':True,
            'installation':{'exposed_vertices':exposed,'buried_vertices':buried,'minimum_positive_vertex_face_gap':min_gap,
                'visible_ray_minimum_skin_gap':stats['visible_surface_gap']['minimum'],
                'eye_body_surface_triangle_contacts':len(contacts),'exposed_vertex_connected_component_sizes':exposure_components,
                'minimum_eye_vertex_to_posterior_body_margin':min(posterior_margins)}}
    spacing=results['R']['frontal_metrics']['aperture']['center'][0]-results['L']['frontal_metrics']['aperture']['center'][0]
    assert abs(spacing/S-1)<=.02
    report={'candidate':candidate,'passed':True,'non_eye_meshes_rig_exact':True,
        'parents_material_links_modifier_settings_and_animation_curves_exact':True,
        'eye_and_glint_object_names_preserved':True,'source_action_curve_counts':{n:len(a['curves']) for n,a in baseline_links['actions'].items()},
        'actual_spacing':spacing,'eyes':results}
    (dest/'validation_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    (dest/'sphere_center_sections.json').write_text(json.dumps(sections,separators=(',',':')),encoding='utf-8')
    summary['candidates'][candidate]=report
summary['source_unchanged']=hashlib.sha256(SOURCE.read_bytes()).hexdigest()==SHA
(OUT/'spherical_eyes_validation.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
print('SPHERICAL_EYES_VALIDATION_OK')
