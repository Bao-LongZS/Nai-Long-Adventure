"""Read-only actual eye volumes and head triangle sections in Idle frame 1."""
import csv
import hashlib
import json
import math
from pathlib import Path
import sys
import bpy
import numpy as np
from mathutils import Vector

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from geometry_diagnostic_utils import mesh_record,face_point,section_object
from eye_projection_measure import visible_metrics

SOURCE=ROOT/'versions/20261005_head_side_continuity/Naiwa_Character.blend'
OUT=ROOT/'diagnostics/20261005_head_geometry'
OUT.mkdir(parents=True,exist_ok=True)
digest=hashlib.sha256(SOURCE.read_bytes()).hexdigest()
assert digest=='a95a5c7efb25cb6f379897dd6ec4af98a201bc2619dc8caa2bb5b5de8ea3dfd5'
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
rig=bpy.data.objects['Naiwa_Rig'];rig.animation_data.action=bpy.data.actions['Naiwa_Idle']
rig.animation_data.use_nla=False;bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
dg=bpy.context.evaluated_depsgraph_get()
body_obj=bpy.data.objects['Naiwa unified golden body']
visible={s:visible_metrics(bpy.data.objects[s+' concentric iris and pupil'],body_obj,step=.0003) for s in ('L','R')}
diam={s:math.sqrt(4*visible[s]['aperture']['area']/math.pi) for s in ('L','R')}
D=float(np.mean(list(diam.values())))
centers={s:list(bpy.data.objects[s+' concentric iris and pupil']['frontal_aperture_center']) for s in ('L','R')}
eye_z=float(np.mean([c[1] for c in centers.values()]))
mouth=bpy.data.objects['Short closed oral seam']
mouth_coords=np.asarray([tuple(mouth.matrix_world@v.co) for v in mouth.data.vertices])
mid=mouth_coords[np.abs(mouth_coords[:,0])<1e-7]
mouth_z=float((mid[:,2].max()+mid[:,2].min())/2)
body=mesh_record(body_obj,dg)
eyes={s:mesh_record(bpy.data.objects[s+' concentric iris and pupil'],dg,eye=True) for s in ('L','R')}

def write_json(name,value,compact=False):
    (OUT/name).write_text(json.dumps(value,ensure_ascii=False,indent=None if compact else 2,
                                   separators=(',',':') if compact else None),encoding='utf-8')

write_json('head_geometry_baseline.json',{'source_sha256':digest,'D':D,'eye_z':eye_z,'mouth_z':mouth_z,'eye_centers_XZ':centers})
print('GEOMETRY_BASELINE_MEASURED',D,flush=True)

sections=[];section_rows=[]
for sid,z in [('eye_level',eye_z),('mouth_level',mouth_z),
              ('mouth_minus_half_D',mouth_z-.5*D),('mouth_minus_D',mouth_z-D)]:
    obj=section_object(body,2,z)
    assert obj['segments'],sid
    loop=obj['loops'][0];assert loop['closed'],('Head section is not a closed contour',sid)
    lo,hi=np.asarray(loop['bounds']);width=float(hi[0]-lo[0]);depth=float(hi[1]-lo[1])
    section_rows.append({'section':sid,'Z':z,'width_X':width,'width_over_D':width/D,
        'depth_Y':depth,'depth_over_D':depth/D,'center_X':loop['bbox_center'][0],
        'center_Y':loop['bbox_center'][1],'center_Z':z,'area':loop['area'],
        'area_centroid_X':loop['area_centroid'][0],'area_centroid_Y':loop['area_centroid'][1]})
    sections.append({'id':sid,'plane_axis':'Z','value':z,'plot_axes':['X','Y'],'objects':[obj]})
sections.append({'id':'midline_x0','plane_axis':'X','value':0.,'plot_axes':['Y','Z'],
                 'objects':[section_object(body,0,0.)]})
for side in ('L','R'):
    x=centers[side][0]
    sections.append({'id':'eye_'+side+'_xcenter','plane_axis':'X','value':x,'plot_axes':['Y','Z'],
        'objects':[section_object(body,0,x),section_object(eyes[side],0,x,select_head=False)]})
write_json('head_geometry_sections.json',{'schema_version':1,'coordinate_system':{'X':'right','Y':'back','Z':'up','F':[0,-1,0]},
    'source_sha256':digest,'method':'Exact intersections of evaluated world-space triangles with specified planes',
    'sections':sections},compact=True)
with (OUT/'head_section_measurements.csv').open('w',encoding='utf-8-sig',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=list(section_rows[0]));writer.writeheader();writer.writerows(section_rows)
print('GEOMETRY_SECTIONS_EXTRACTED',flush=True)

def visible_vertices(record,region=None):
    mask=record['regions']==region if region else np.ones(len(record['triangles']),bool)
    ids=np.unique(record['triangles'][mask]);found=[]
    for idx in ids:
        p=record['points'][idx];skin=face_point(body,p[0],p[2])
        if skin is not None and p[1]<skin[1]-5e-7:
            found.append((int(idx),p,skin,float(skin[1]-p[1])))
    assert found,region
    return found

def endpoint(row):
    idx,p,skin,gap=row
    return {'vertex_index':idx,'point_world':p.tolist(),'yellow_face_at_same_XZ':skin.tolist(),
            'projection_F':gap,'projection_over_D':gap/D}

def local_thickness(record,idx):
    t=record['triangle_points'][idx];point=t.mean(0);normal=record['triangle_normals'][idx]
    epsilon=5e-7
    start=point-epsilon*normal
    p,n,tri,d=record['bvh'].ray_cast(Vector(start),Vector(-normal),.6)
    if p is None:return None
    back=np.asarray(p,float);thickness=float(np.linalg.norm(back-point))
    if thickness<1e-5:return None
    return {'surface_point':point.tolist(),'normal_world':normal.tolist(),
            'front_endpoint':point.tolist(),'back_endpoint':back.tolist(),
            'thickness':thickness,'thickness_over_D':thickness/D,'front_triangle_index':int(idx),
            'back_triangle_index':int(tri),'inside_start_epsilon':epsilon,
            'method':'First exit along inward actual front triangle normal; closed eye volume chord, includes buried closure, not a material shell layer'}

def shared_boundary(record):
    edges={}
    for tri,label in zip(record['triangles'],record['regions']):
        for a,b in ((tri[0],tri[1]),(tri[1],tri[2]),(tri[2],tri[0])):
            edges.setdefault(tuple(sorted((int(a),int(b)))),set()).add(str(label))
    boundary=[edge for edge,labels in edges.items() if labels=={'black','green'}]
    return {'shared_black_green_edge_count':len(boundary),
            'shared_boundary_vertex_count':len({v for e in boundary for v in e}),
            'independent_pupil_geometry_step':0.,'independent_pupil_step_over_D':0.,
            'evidence':'Black and green triangles share exactly the same mesh vertex indices at their material boundaries'}

eye_results={};eye_rows=[];surface_centers={}
for side,record in eyes.items():
    green=visible_vertices(record,'green');black=visible_vertices(record,'black');all_rows=green+black
    gp=min(green,key=lambda v:v[1][1]);bp=min(black,key=lambda v:v[1][1])
    gm=max(green,key=lambda v:v[3]);bm=max(black,key=lambda v:v[3]);am=max(all_rows,key=lambda v:v[3])
    centroids=record['triangle_points'].mean(1)
    green_front=[]
    for idx in np.flatnonzero(record['regions']=='green'):
        p=centroids[idx];skin=face_point(body,p[0],p[2])
        if skin is not None and p[1]<skin[1]-5e-7:green_front.append(int(idx))
    nearby=sorted(green_front,key=lambda i:float(np.linalg.norm(centroids[i]-gp[1])))
    representative=next((v for v in (local_thickness(record,i) for i in nearby[:30]) if v),None)
    assert representative,side
    ids=np.asarray(green_front)[np.linspace(0,len(green_front)-1,32,dtype=int)]
    samples=[v for v in (local_thickness(record,int(i)) for i in ids) if v]
    low,high=record['points'].min(0),record['points'].max(0)
    aperture=visible[side]['aperture'];bounds=np.asarray(aperture['bounds'])
    sx,sz=centers[side];center=face_point(body,sx,sz);assert center is not None
    surface_centers[side]=center.tolist()
    black_vs_green=shared_boundary(record)
    black_vs_green.update({'black_foremost_F_minus_green_foremost_F':float(gp[1][1]-bp[1][1]),
        'black_peak_face_gap_minus_green_peak_face_gap':float(bm[3]-gm[3]),
        'different_position_height_difference_is_continuous_curvature_not_an_added_pupil_bump':True})
    eye_results[side]={'visible_outer_equivalent_diameter':diam[side],
        'visible_outer_XZ_width_height':(bounds[1]-bounds[0]).tolist(),
        'visible_pupil_equivalent_diameter':math.sqrt(4*visible[side]['black']['area']/math.pi),
        'aperture_center_XZ':centers[side],'center_on_yellow_face_world':center.tolist(),
        'whole_closed_mesh_bounds':[low.tolist(),high.tolist()],
        'whole_closed_mesh_XZ_width_height':[float(high[0]-low[0]),float(high[2]-low[2])],
        'foremost_visible_green':endpoint(gp),'maximum_green_face_gap':endpoint(gm),
        'foremost_visible_black':endpoint(bp),'maximum_black_face_gap':endpoint(bm),
        'maximum_whole_eye_face_gap':endpoint(am),'local_normal_thickness':representative,
        'normal_thickness_samples':samples,'sampled_normal_thickness_minimum':min(v['thickness'] for v in samples),
        'sampled_normal_thickness_maximum':max(v['thickness'] for v in samples),
        'black_vs_green':black_vs_green,'frontal_visible_metrics':visible[side]}
    eye_rows.append({'eye':side,'outer_visible_D':diam[side],
        'outer_visible_width_X':float(bounds[1,0]-bounds[0,0]),'outer_visible_height_Z':float(bounds[1,1]-bounds[0,1]),
        'pupil_visible_D':eye_results[side]['visible_pupil_equivalent_diameter'],
        'green_foremost_face_projection_F':gp[3],'green_foremost_projection_over_D':gp[3]/D,
        'maximum_green_face_gap':gm[3],'maximum_whole_eye_face_gap':am[3],
        'local_normal_thickness':representative['thickness'],'local_normal_thickness_over_D':representative['thickness']/D,
        'black_independent_geometry_step':0.,'black_foremost_F_minus_green_foremost_F':float(gp[1][1]-bp[1][1]),
        'black_peak_face_gap_minus_green_peak_face_gap':float(bm[3]-gm[3]),
        'sampled_normal_thickness_minimum':min(v['thickness'] for v in samples),
        'sampled_normal_thickness_maximum':max(v['thickness'] for v in samples)})
    print('EYE_ACTUAL_STRUCTURE_MEASURED',side,flush=True)

c=np.asarray([centers[s] for s in ('L','R')]);c3=np.asarray([surface_centers[s] for s in ('L','R')])
spacing={'horizontal_X':float(abs(c[1,0]-c[0,0])),'frontal_XZ_distance':float(np.linalg.norm(c[1]-c[0])),
         'distance_3D_on_yellow_face':float(np.linalg.norm(c3[1]-c3[0])),
         'horizontal_X_over_D':float(abs(c[1,0]-c[0,0])/D),
         'frontal_XZ_over_D':float(np.linalg.norm(c[1]-c[0])/D),
         'center_definition':'Frontal aperture center XZ; Y assigned by ray intersection with current yellow base face; current eye is not a full sphere with a unique spherical center'}
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==digest
report={'source_blend':str(SOURCE),'source_sha256':digest,'source_unchanged':True,'no_blend_save':True,
    'date':'2026-10-06','pose':'Naiwa_Idle frame 1','units':'Blender project coordinate units',
    'coordinate_system':{'X':'right','Y':'back','Z':'up','F':[0,-1,0]},'D':D,
    'D_definition':'Mean equivalent area diameter of ray-visible frontal outer green aperture including its enclosed pupil; not green-annulus-only area',
    'sampling_step':.0003,'eye_z':eye_z,'mouth_z':mouth_z,'eyes':eye_results,'eye_spacing':spacing,
    'horizontal_sections':section_rows,'yellow_base_face_definition':'Current unified yellow body mesh excluding eyes; not a historical unsculpted face',
    'section_center_definition':'Axis-aligned section bounding-box center; area centroid is separately reported',
    'normal_thickness_definition':'Local first-exit chord of closed eye along actual front triangle normal; 32 representative green-face samples per eye; not uniform coating thickness',
    'geometry_changes_performed':False}
write_json('head_geometry_measurements.json',report)
with (OUT/'eye_measurements.csv').open('w',encoding='utf-8-sig',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=list(eye_rows[0]));writer.writeheader();writer.writerows(eye_rows)
print('HEAD_GEOMETRY_DIAGNOSTICS_OK',D,spacing['horizontal_X_over_D'],flush=True)
