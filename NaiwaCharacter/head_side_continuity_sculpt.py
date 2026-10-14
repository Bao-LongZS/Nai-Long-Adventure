"""Redistribute the existing side sections with C2 boundary jets; freeze features."""
import math
import bpy
import numpy as np
from mathutils import Vector
from longitudinal_surface_sculpt import coordinates, ease


def quintic_profile(zs, values, lo, hi):
    jets=[]
    for boundary in (lo,hi):
        ids=np.abs(zs-boundary)<.012
        fit=np.polyfit(zs[ids]-boundary,values[ids],3)
        jets.append((float(np.interp(boundary,zs,values)),float(fit[-2]),float(2*fit[-3])))
    a,da,dda=jets[0]; b,db,ddb=jets[1]; h=hi-lo
    coefficients=np.zeros(6); coefficients[:3]=a,da*h,.5*dda*h*h
    coefficients[3:]=np.linalg.solve(np.array([[1,1,1],[3,4,5],[6,12,20]],float),
        [b-coefficients[:3].sum(),db*h-coefficients[1]-2*coefficients[2],ddb*h*h-2*coefficients[2]])
    target=np.polynomial.polynomial.polyval(np.clip((zs-lo)/h,0,1),coefficients)
    return target,{'boundary_jets':jets,'coefficients_in_normalized_z':coefficients.tolist()}


def eye_guard(x,z,rec):
    distances=[np.sqrt((x-c[0])**2+(z-c[1])**2) for c in rec['eye_centers']]
    return ease(.60*rec['D'],.90*rec['D'],np.minimum(*distances))


def section_data(body,zs):
    p=coordinates(body).astype(float)
    edges=np.empty(len(body.data.edges)*2,dtype=np.int32)
    body.data.edges.foreach_get('vertices',edges);edges=edges.reshape(-1,2)
    a,b=p[edges[:,0]],p[edges[:,1]]
    selected=(np.minimum(a[:,2],b[:,2])<=zs.max())&(np.maximum(a[:,2],b[:,2])>=zs.min())
    a,b=a[selected],b[selected]
    rows=[]
    for z in zs:
        crossed=(((a[:,2]<=z)&(b[:,2]>=z))|((b[:,2]<=z)&(a[:,2]>=z)))&(np.abs(b[:,2]-a[:,2])>1e-12)
        aa,bb=a[crossed],b[crossed]
        section=aa+(bb-aa)*((z-aa[:,2])/(bb[:,2]-aa[:,2]))[:,None]
        u=(section[:,0]+section[:,1])/math.sqrt(2)
        support=section[np.argmin(u)]
        hit,f,_,_=body.ray_cast(Vector((0,-2,float(z))),Vector((0,1,0)));assert hit
        hit,r,_,_=body.ray_cast(Vector((0,2,float(z))),Vector((0,-1,0)));assert hit
        rows.append([float(section[:,0].max()),float(-u.min()),support[0],support[1],f.y,r.y])
    return np.asarray(rows)


def fix_side_continuity(body,rec):
    assert not body.get('head_side_continuity_revision'), 'Reopen the fixed latest-version baseline before every run.'
    old=coordinates(body)
    normals=np.empty(len(body.data.vertices)*3,dtype=np.float32)
    body.data.vertices.foreach_get('normal',normals);normals=normals.reshape(-1,3)
    D=rec['D'];lo=rec['correction_z_low'];hi=rec['correction_z_high']
    zs=np.asarray([r['z'] for r in rec['samples']])
    rows=section_data(body,zs)
    target0,fit0=quintic_profile(zs,rows[:,0],lo,hi)
    # Re-fit entire longitudinal sections rather than placing a surface bump.
    # The narrow numerical gate only guarantees zero displacement and zero
    # first/second derivatives at the two untouched boundary sections.
    gate=ease(lo,lo+.12*D,zs)*(1-ease(hi-.12*D,hi,zs))
    delta0=(target0-rows[:,0])*gate
    z=old[:,2].astype(float);x=old[:,0].astype(float)
    width=np.interp(z,zs,rows[:,0]);front=np.interp(z,zs,rows[:,4]);rear=np.interp(z,zs,rows[:,5])
    q=np.abs(x)/width;ny=(old[:,1]-(front+rear)/2)/((rear-front)/2)
    lateral=ease(.38,.82,q)*(1-ease(.12,.70,ny))*eye_guard(x,z,rec)
    # Carry the same corrected section through the side wall and upper cheek.
    # There is no independent oblique lobe or extra angular displacement.
    dx=np.sign(x)*lateral*q*np.interp(z,zs,delta0)
    dx[(z<=lo)|(z>=hi)]=0
    limit=.12*D
    scale=min(1.0,limit/max(float(np.max(np.abs(dx))),1e-12))
    dx*=scale
    new=old.copy();active=dx!=0;new[active,0]+=dx[active]
    displacement=new-old
    changed=np.any(new!=old,axis=1)
    assert np.array_equal(new[:,1:],old[:,1:]), 'Front/throat Y and all Z must remain exact.'
    lengths=np.linalg.norm(displacement,axis=1)
    assert float(lengths.max())<=limit+1e-7
    body.data.vertices.foreach_set('co',new.ravel());body.data.update()
    body['head_side_continuity_revision']=True
    body['head_revision']='Continuous upper-cheek section transition; eyes, mouth, throat, crown and shoulders fixed'
    return {'D':D,'requested_Z_range':[lo,hi],
        'actual_modified_bounds_xyz':[old[changed].min(axis=0).tolist(),old[changed].max(axis=0).tolist()],
        'changed_vertices':int(changed.sum()),'maximum_euclidean_displacement':float(lengths.max()),
        'maximum_normal_displacement':float(np.max(np.abs(np.sum(displacement*normals,axis=1)))),
        'limit_0_12_D':limit,'global_limit_scale':scale,'Y_and_Z_exact':True,
        'front_section_fit':fit0,
        'eye_attachment_guard_radii':[.60*D,.90*D],
        'profiles':[{'z':float(z),'old_halfwidth':float(a),'goal_halfwidth':float(b)}
                    for z,a,b in zip(zs,rows[:,0],rows[:,0]+delta0)]}
