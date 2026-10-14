"""Independent H2 measurements from actual saved native triangles.

H2 intentionally releases the R1 XYZ/old midline locks. Only real fixed
features and narrow contact surfaces are reconstructed here.
"""
import numpy as np
from mathutils import Vector
from geometry_diagnostic_utils import section_object
from head_neck_validation_utils import raw_record, fit_sphere, sections
from r1_validation_utils import (BODY, SHELL, vertex_normals,
    exact_local_coordinates, normal_snapshot, normal_delta_measurement)

D0=.117244812742
PLANES=(-.18,-.12,-.08,-.04,0.,.04,.08,.12,.18)


def actual_contacts(points, eye_meshes, mouth_obj, mouth_exact=.004, eye_exact=.003):
    mouth_record=raw_record(mouth_obj)
    fits={side:fit_sphere(p) for side,p in eye_meshes.items()}
    mouth_distance=np.full(len(points),np.inf)
    scoped=np.flatnonzero((points[:,2]>2.24)&(points[:,2]<2.50)&(points[:,1]<0.))
    for index in scoped:
        mouth_distance[index]=mouth_record['bvh'].find_nearest(Vector(points[index]))[3]
    eye_contact=np.zeros(len(points),bool)
    signed={}
    for side,fit in fits.items():
        dist=np.linalg.norm(points-np.asarray(fit['center']),axis=1)-fit['radius']
        signed[side]=dist
        # This is the true sphere/skin intersection line. Interior embedded
        # globe skin is not frozen as an entire .026 installation distance disk.
        eye_contact|=np.abs(dist)<=eye_exact
    top=points[:,2].max()
    return {'mouth_true_contact_band':mouth_distance<=mouth_exact,
        'eye_true_sphere_intersection_band':eye_contact,
        'crown_highest_anchor':points[:,2]>=top-1e-7}, {
        'mouth_distance':mouth_distance,'eye_signed_distances':signed,
        'eye_sphere_fits':fits,'mouth_exact_distance':mouth_exact,'eye_exact_distance':eye_exact,
        'crown_highest_Z':float(top)}


def horizontal_samples(record,z0=1.98,z1=2.64,step=.002):
    """Actual triangle intersections, including left/right outer profiles."""
    tp=record['triangle_points']
    lows=tp[:,:,2].min(1)
    highs=tp[:,:,2].max(1)
    rows=[]
    for z in np.arange(z0,z1+step/2,step):
        selected=(lows<=z)&(highs>=z)
        tri=tp[selected]
        hits=[]
        for i,j in ((0,1),(1,2),(2,0)):
            a,b=tri[:,i],tri[:,j]
            da,db=a[:,2]-z,b[:,2]-z
            crossing=(da*db<=0)&(np.abs(da-db)>1e-14)
            a,b,da,db=a[crossing],b[crossing],da[crossing],db[crossing]
            if len(a):
                hits.append(a+(b-a)*(da/(da-db))[:,None])
        if not hits:
            continue
        p=np.concatenate(hits)
        left=p[np.argmin(p[:,0])]
        right=p[np.argmax(p[:,0])]
        front=record['bvh'].ray_cast(Vector((0.,-3.,float(z))),Vector((0.,1.,0.)),6.)[0]
        rear=record['bvh'].ray_cast(Vector((0.,3.,float(z))),Vector((0.,-1.,0.)),6.)[0]
        rows.append({'Z':float(z),'width_X':float(right[0]-left[0]),
            'left_point':left.tolist(),'right_point':right.tolist(),
            'front_Y':float(front.y) if front is not None else None,
            'rear_Y':float(rear.y) if rear is not None else None})
    return rows


def extended_sections(record):
    result=sections(record)
    result['longitudinal']=[{'plane_X':x,'body':section_object(record,0,x,select_head=True)} for x in PLANES]
    samples=[]
    for x in PLANES:
        rows=[]
        for z in np.arange(1.95,2.646,.0005):
            f=record['bvh'].ray_cast(Vector((float(x),-3.,float(z))),Vector((0.,1.,0.)),6.)[0]
            r=record['bvh'].ray_cast(Vector((float(x),3.,float(z))),Vector((0.,-1.,0.)),6.)[0]
            if f is not None and r is not None:
                rows.append([float(z),float(f.y),float(r.y)])
        samples.append({'plane_X':float(x),'columns':['Z','front_Y','rear_Y'],'step_Z':.0005,'rows':rows})
    result['longitudinal_ray_samples']=samples
    result['outer_profile_actual_triangle_samples']=horizontal_samples(record)
    result['outer_profile_Z_step']=.002
    return result


def compare_section_widths(old,new,limit=.05):
    rows=[]
    for a,b in zip(old['outer_profile_actual_triangle_samples'],new['outer_profile_actual_triangle_samples']):
        assert abs(a['Z']-b['Z'])<1e-10
        fraction=b['width_X']/a['width_X']-1.
        assert abs(fraction)<=limit+1e-6,('H2 actual horizontal width change',a['Z'],fraction,limit)
        rows.append({'Z':a['Z'],'R1_width_X':a['width_X'],'H2_width_X':b['width_X'],
            'fraction_change':fraction,'delta_front_Y':b['front_Y']-a['front_Y'] if a['front_Y'] is not None and b['front_Y'] is not None else None,
            'delta_rear_Y':b['rear_Y']-a['rear_Y'] if a['rear_Y'] is not None and b['rear_Y'] is not None else None})
    return {'width_change_limit':limit,'records':rows,'maximum_fraction_change':max(abs(r['fraction_change']) for r in rows),
        'old_midline_endpoint_locks_removed':True,'old_Z2_12_2_22_2_31_are_records_only':True}
