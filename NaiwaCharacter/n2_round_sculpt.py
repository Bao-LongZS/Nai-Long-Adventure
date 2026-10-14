"""Constrained smoothing of the final N2 XY surface, retaining every source Z.

Frozen star-shaped periodic sections describe the actual head/neck surface,
not an ellipse. ADMM minimizes fidelity plus anisotropic bending of final XY,
with protection and displacement constraints applied during every solve step.
"""
import math
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from n2_round_mouth_curvature import refine_mouth_curvature, evaluate_mouth_correction

D0 = .117244812742
BODY = 'Naiwa unified golden body'
SHELL = 'Head and torso silhouette only'

def array(items, prop, width, dtype=np.float32):
    result = np.empty(len(items)*width, dtype=dtype)
    items.foreach_get(prop,result)
    return result.reshape(-1,width)

def points(obj):
    p = array(obj.data.vertices,'co',3).astype(float)
    m = np.asarray(obj.matrix_world,float)
    return p@m[:3,:3].T+m[:3,3]

def ease(a,b,x):
    t = np.clip((x-a)/(b-a),0,1)
    return t**3*(10+t*(-15+6*t))

def mouth_release(distance):
    # C2 Beta(3,10) cumulative release over the same true-distance interval.
    # The symmetric quintic left too little freedom just beyond the fixed skin
    # and forced the final curve into a short rebound beside the oral seam.
    t=np.clip((distance-.012)/.028,0,1)
    return np.clip(sum(math.comb(12,j)*t**j*(1-t)**(12-j) for j in range(3,13)),0,1)

def make_context(body):
    p = points(body)
    body.data.calc_loop_triangles()
    tri = array(body.data.loop_triangles,'vertices',3,np.int32)
    normals = array(body.data.vertices,'normal',3).astype(float)
    surface = BVHTree.FromPolygons(p.tolist(),tri.tolist(),all_triangles=True)
    weights = np.asarray([sum(g.weight for g in v.groups if body.vertex_groups[g.group].name.startswith(('UpperArm.','Forearm.'))) for v in body.data.vertices])
    seeds = (weights>=.5)&(p[:,2]>=1.75)
    patch = tri[seeds[tri].any(axis=1)]
    shoulder = BVHTree.FromPolygons(p.tolist(),patch.tolist(),all_triangles=True)
    mouth = bpy.data.objects['Short closed oral seam']
    mouth.data.calc_loop_triangles()
    mp = points(mouth)
    mt = array(mouth.data.loop_triangles,'vertices',3,np.int32)
    oral = BVHTree.FromPolygons(mp.tolist(),mt.tolist(),all_triangles=True)
    eyes=[]
    for side in ('L','R'):
        ep = points(bpy.data.objects[side+' concentric iris and pupil'])
        center0 = ep.mean(0);q=ep-center0
        fit=np.linalg.lstsq(np.column_stack((2*q,np.ones(len(q)))),np.sum(q*q,axis=1),rcond=None)[0]
        c=center0+fit[:3];radius=math.sqrt(fit[3]+np.dot(fit[:3],fit[:3]))
        eyes.append((c,radius))
    return dict(points=p,triangles=tri,normals=normals,surface=surface,
                shoulder=shoulder,oral=oral,eyes=eyes,shoulder_seeds=seeds)

def zones(p,ctx):
    z=p[:,2]
    front=ease(2.20,2.28,z)*(1-ease(2.44,2.48,z))
    front*=1-ease(1.8*D0,1.8*D0+.045,np.abs(p[:,0]))
    rear=ease(2.20,2.28,z)*(1-ease(2.43,2.50,z))
    side=ease(2.035,2.07,z)*(1-ease(2.28,2.325,z))
    center=np.interp(z,ctx['zs'],ctx['centers'])
    radial=p[:,1]-center
    rho=np.hypot(p[:,0],radial)
    sy=radial/np.maximum(rho,1e-8)
    front*=1-ease(-.65,-.20,sy)
    rear*=ease(.10,.60,sy)
    side*=ease(-.55,-.12,sy)
    region=np.maximum.reduce((front,rear,side))
    active=np.flatnonzero(region>0)
    mouth=np.ones(len(p));shoulder=np.ones(len(p))
    for i in active:
        mouth[i]=mouth_release(ctx['oral'].find_nearest(Vector(p[i]))[3])
        shoulder[i]=ease(.012,.062,ctx['shoulder'].find_nearest(Vector(p[i]))[3])
    signed=np.minimum(*[np.linalg.norm(p-c,axis=1)-r for c,r in ctx['eyes']])
    eye=ease(.026,.071,signed)
    protection=mouth*shoulder*eye
    front*=protection;rear*=protection;side*=protection
    region=np.maximum.reduce((front,rear,side))
    # Shared feasible set handles overlapping regions in one solve.
    total=.0145*front+.0075*rear+.0038*side
    total=np.minimum(total,.0145*region)
    rear_only=(rear>0)&(front==0)&(side==0)
    total[rear_only]=np.minimum(total[rear_only],.0075*rear[rear_only])
    side_band=(z<=2.28)&(sy>=-.20)
    total[side_band]=np.minimum(total[side_band],.0038*region[side_band])
    normal_cap=np.where((sy>.10)&(z>=2.28),.0058,.0145)*region
    normal_cap[side_band]=.0038*region[side_band]
    return dict(front=front,rear=rear,side=side,region=region,mouth_guard=mouth,
                shoulder_guard=shoulder,eye_guard=eye,total_limit=total,
                x_limit=.0038*region,normal_limit=normal_cap,side_band=side_band,
                rear_region=(sy>.10)&(z>=2.28)&(z<=2.50))

def freeze_grid(ctx,step=.00125,ntheta=192):
    zs=np.linspace(2.005,2.535,round((2.535-2.005)/step)+1)
    centers=[]
    for z in zs:
        f=ctx['surface'].ray_cast(Vector((0,-3,float(z))),Vector((0,1,0)),6)[0]
        r=ctx['surface'].ray_cast(Vector((0,3,float(z))),Vector((0,-1,0)),6)[0]
        assert f is not None and r is not None
        centers.append((f.y+r.y)/2)
    ctx.update(zs=zs,centers=np.asarray(centers))
    theta=np.arange(ntheta)*2*np.pi/ntheta
    grid=np.empty((len(zs),ntheta,3));normal=np.empty_like(grid)
    for i,(z,center) in enumerate(zip(zs,centers)):
        for j,t in enumerate(theta):
            direction=Vector((math.cos(t),math.sin(t),0))
            hit,_,face,_=ctx['surface'].ray_cast(Vector((0,float(center),float(z))),direction,3)
            assert hit is not None,('Missing radial source surface',z,t)
            grid[i,j]=hit
            ids=ctx['triangles'][face];a,b,c=ctx['points'][ids]
            v0=b-a;v1=c-a;v2=np.asarray(hit)-a
            d00=v0@v0;d01=v0@v1;d11=v1@v1;d20=v2@v0;d21=v2@v1
            den=d00*d11-d01*d01
            u=(d11*d20-d01*d21)/den;v=(d00*d21-d01*d20)/den
            n=(1-u-v)*ctx['normals'][ids[0]]+u*ctx['normals'][ids[1]]+v*ctx['normals'][ids[2]]
            normal[i,j]=n/max(np.linalg.norm(n),1e-10)
    return grid,normal,theta

def project(delta,normals,mask):
    d=delta.copy()
    d[:,0]=np.clip(d[:,0],-mask['x_limit'],mask['x_limit'])
    length=np.linalg.norm(d,axis=1)
    d*=np.minimum(1,mask['total_limit']/np.maximum(length,1e-20))[:,None]
    n=normals[:,:2];nn=np.sum(n*n,axis=1)
    dot=np.sum(d*n,axis=1);clip=np.clip(dot,-mask['normal_limit'],mask['normal_limit'])
    d-=(dot-clip)[:,None]*n/np.maximum(nn[:,None],1e-20)
    # Normal-plane projection can slightly expand X. Repeated convex projection
    # keeps both restrictions without scaling an already-solved field by a mask.
    for _ in range(3):
        d[:,0]=np.clip(d[:,0],-mask['x_limit'],mask['x_limit'])
        length=np.linalg.norm(d,axis=1)
        d*=np.minimum(1,mask['total_limit']/np.maximum(length,1e-20))[:,None]
        dot=np.sum(d*n,axis=1);clip=np.clip(dot,-mask['normal_limit'],mask['normal_limit'])
        d-=(dot-clip)[:,None]*n/np.maximum(nn[:,None],1e-20)
    d[mask['region']==0]=0
    return d

def smooth_final(grid,normals,ctx,sigma_z=.013,sigma_arc=.014,front_inset=.0048,rear_fill=.0045,iterations=180,rho=3.):
    nz,nt,_=grid.shape
    p=grid.reshape(-1,3)
    mask=zones(p,ctx)
    target=grid[:,:,:2].copy()
    prior=(front_inset*np.exp(-.5*((p[:,2]-2.359)/.022)**2)*mask['front']+
           rear_fill*np.exp(-.5*((p[:,2]-2.380)/.026)**2)*mask['rear'])
    target[:,:,1]+=prior.reshape(nz,nt)
    base=grid[:,:,:2]
    v=target.copy();dual=np.zeros_like(v)
    dz=float(ctx['zs'][1]-ctx['zs'][0]);length=2*(nz-1)
    kz=2*np.pi*np.fft.fftfreq(length,dz)
    ka=2*np.pi*np.fft.rfftfreq(nt,2*np.pi/nt)/.30
    bending=(sigma_z**2*kz[:,None]**2+sigma_arc**2*ka[None,:]**2)**2
    denominator=1+rho+bending
    def reflect(a):return np.concatenate((a,a[-2:0:-1]),axis=0)
    def solve(rhs):
        return np.stack([np.fft.irfft2(np.fft.rfft2(reflect(rhs[:,:,c]))/denominator,s=(length,nt))[:nz] for c in (0,1)],axis=2)
    for i in range(iterations):
        u=solve(target+rho*(v-dual))
        old=v
        v=base+project((u+dual-base).reshape(-1,2),normals.reshape(-1,3),mask).reshape(nz,nt,2)
        dual+=u-v
        primal=float(np.max(np.linalg.norm(u-v,axis=2)))
        change=float(np.max(np.linalg.norm(v-old,axis=2)))
        if (i+1)%200==0:
            print('R1_SOLVER_PROGRESS',i+1,'primal=',primal,'change=',change,flush=True)
        if i>=40 and primal<3e-7 and change<2e-8:break
    delta=v-base
    def energy(field):
        f=reflect(field)
        return sum(float(np.sum(bending*np.abs(np.fft.rfft2(f[:,:,c]))**2)) for c in (0,1))
    report={'method':'ADMM final periodic-section XY bending + source/prior fidelity; constrained feasible projection within each iteration',
        'parameterization':'actual frozen star-shaped ray sections; not circles or ellipses',
        'sigma_Z':sigma_z,'sigma_arc':sigma_arc,'mean_arc_radius_scale':.30,
        'front_initial_inset':front_inset,'front_center_Z':2.359,'front_sigma_Z':.022,
        'rear_initial_fill':rear_fill,'rear_center_Z':2.380,'rear_sigma_Z':.026,
        'rho':rho,'iterations':i+1,'primal_residual':primal,'last_change':change,
        'strict_solver_tolerance_reached':bool(primal<3e-7 and change<2e-8),
        'acceptance_policy':'finite feasible solve; actual saved-surface curvature and visual diagnostics determine acceptance, not convergence labels',
        'source_final_surface_bending_energy':energy(base),'result_final_surface_bending_energy':energy(v),
        'grid_shape':[nz,nt],'grid_step_Z':dz,'mouth_exact_distance':.012,'mouth_fade_end':.040,
        'mouth_release':'asymmetric C2 Beta(3,10) CDF on true source oral triangle distance; included in constrained final-surface solve',
        'shoulder_exact_distance':.012,'shoulder_fade_end':.062,'eye_exact_sphere_distance':.026,'eye_fade_end':.071,
        'boundary_condition':'source XY fixed outside smooth feasible region; periodic theta, reflected numerical Z extension',
        'Z_policy':'all original float32 vertex Z copied exactly'}
    return delta,mask,report

def interpolate(p,ctx,field):
    z=p[:,2];center=np.interp(z,ctx['zs'],ctx['centers'])
    theta=np.mod(np.arctan2(p[:,1]-center,p[:,0]),2*np.pi)
    zi=np.clip((z-ctx['zs'][0])/(ctx['zs'][1]-ctx['zs'][0]),0,len(ctx['zs'])-1)
    lo=np.minimum(np.floor(zi).astype(int),len(ctx['zs'])-2);fz=zi-lo
    th=theta/(2*np.pi)*field.shape[1];a=np.floor(th).astype(int)%field.shape[1];b=(a+1)%field.shape[1];ft=th-np.floor(th)
    low=field[lo,a]*(1-ft[:,None])+field[lo,b]*ft[:,None]
    high=field[lo+1,a]*(1-ft[:,None])+field[lo+1,b]*ft[:,None]
    return low*(1-fz[:,None])+high*fz[:,None]

def sculpt(options):
    body=bpy.data.objects[BODY]
    ctx=make_context(body)
    grid,normals,theta=freeze_grid(ctx)
    field,grid_masks,solver=smooth_final(grid,normals,ctx,**options)
    reports={};saved=None;mouth_report=None
    for name in (BODY,SHELL):
        obj=bpy.data.objects.get(name)
        if obj is None:continue
        old=array(obj.data.vertices,'co',3)
        p=points(obj);n=array(obj.data.vertices,'normal',3).astype(float)
        mask=zones(p,ctx)
        global_d=project(interpolate(p,ctx,field),n,mask)
        if name==BODY:
            d,mouth_report=refine_mouth_curvature(p,n,ctx['triangles'],global_d,mask)
            print('R1_ACTUAL_MOUTH_CURVATURE',mouth_report['sections'],flush=True)
        else:
            d=global_d+evaluate_mouth_correction(p,mask,mouth_report)
        # Numerical convex feasibility projection only; no post-solve mask.
        d=project(d,n,mask)
        new=old.copy()
        assert np.allclose(np.asarray(obj.matrix_world),np.eye(4),atol=1e-10)
        new[:,:2]=(old[:,:2].astype(float)+d).astype(np.float32)
        new[mask['region']==0]=old[mask['region']==0]
        assert np.array_equal(new[:,2],old[:,2])
        actual=new.astype(float)-old.astype(float)
        obj.data.vertices.foreach_set('co',new.ravel());obj.data.update()
        reports[name]={'changed_vertices':int(np.count_nonzero(np.linalg.norm(actual,axis=1))),
            'maximum_total_displacement':float(np.linalg.norm(actual,axis=1).max()),
            'maximum_abs_X':float(np.abs(actual[:,0]).max()),'maximum_abs_Z':0.,
            'maximum_abs_source_normal_projection':float(np.abs(np.sum(actual*n,axis=1)).max()),
            'has_custom_normals_before':bool(obj.data.has_custom_normals),
            'custom_normals_action':'none; preserve source normals representation and update geometry'}
        if name==BODY:
            saved=dict(points_world=p,source_normals=n,triangles=ctx['triangles'],delta=actual,
                       global_delta_xy=global_d,actual_mouth_correction_xy=d-global_d,**mask,
                       grid_source=grid,grid_result_xy=grid[:,:,:2]+field,grid_delta_xy=field,
                       grid_normals=normals,grid_Z=ctx['zs'],grid_theta=theta,grid_center_Y=ctx['centers'],
                       **{'grid_mask_'+k:val for k,val in grid_masks.items()})
    shell=bpy.data.objects.get(SHELL)
    if shell:shell.hide_render=True
    bpy.context.view_layer.update()
    return {'solver':solver,'mouth_curvature':mouth_report,'surfaces':reports,'D0':D0,
        'shell_policy':'same frozen global field and same jointly fitted mouth control field; source Z exact; hidden'},saved
