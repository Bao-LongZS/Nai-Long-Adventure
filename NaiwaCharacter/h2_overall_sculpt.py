"""Joint R1 head/neck/shoulder target, with narrow real feature constraints.

One coarse tensor-product cubic B-spline target is fit to frozen actual radial
sections. All shape priors enter that target together. A periodic constrained
final-surface solve connects it to the exact mouth/sphere contact boundaries;
no sequential region sculpt or post-solve deformation-mask multiplication.
"""
import math
import bpy
import numpy as np
from mathutils import Vector
from n2_round_sculpt import array, points, ease, make_context, interpolate
from h2_integrated_turn import integrated_front_curves

BODY='Naiwa unified golden body'
SHELL='Head and torso silhouette only'
D0=.117244812742


def freeze_grid(ctx, step=.0025, ntheta=144):
    top=float(ctx['points'][:,2].max())
    zs=np.linspace(1.94,top-.0013,round((top-.0013-1.94)/step)+1)
    centers=[]; widths=[]
    for z in zs:
        f=ctx['surface'].ray_cast(Vector((0,-3,float(z))),Vector((0,1,0)),6)[0]
        r=ctx['surface'].ray_cast(Vector((0,3,float(z))),Vector((0,-1,0)),6)[0]
        assert f is not None and r is not None
        centers.append((f.y+r.y)/2)
    ctx.update(zs=zs,centers=np.asarray(centers),top=top)
    theta=np.arange(ntheta)*2*np.pi/ntheta
    grid=np.empty((len(zs),ntheta,3)); normal=np.empty_like(grid)
    for i,(z,center) in enumerate(zip(zs,centers)):
        for j,t in enumerate(theta):
            hit,_,face,_=ctx['surface'].ray_cast(Vector((0,float(center),float(z))),Vector((math.cos(t),math.sin(t),0)),3)
            assert hit is not None,('source radial section miss',z,t)
            grid[i,j]=hit
            ids=ctx['triangles'][face]; a,b,c=ctx['points'][ids]
            v0=b-a;v1=c-a;v2=np.asarray(hit)-a
            d00=v0@v0;d01=v0@v1;d11=v1@v1;d20=v2@v0;d21=v2@v1
            den=d00*d11-d01*d01
            u=(d11*d20-d01*d21)/den;v=(d00*d21-d01*d20)/den
            n=(1-u-v)*ctx['normals'][ids[0]]+u*ctx['normals'][ids[1]]+v*ctx['normals'][ids[2]]
            normal[i,j]=n/max(np.linalg.norm(n),1e-12)
        widths.append(float(np.max(np.abs(grid[i,:,0]))))
    ctx['half_widths']=np.asarray(widths)
    return grid,normal,theta


def constraints(p,ctx,clear=False,shell=False):
    z=p[:,2]; center=np.interp(z,ctx['zs'],ctx['centers'])
    radial=p[:,1]-center; radius=np.hypot(p[:,0],radial)
    sy=radial/np.maximum(radius,1e-9)
    region=ease(1.96,2.09,z)*(1-ease(ctx['top']-.026,ctx['top'],z))
    # Arm geometry is outside the head/torso radial core. Broad source-surface
    # shoulder separation keeps remote arms exact while allowing shoulder back.
    shoulder=np.ones(len(p)); mouth=np.ones(len(p))
    md=np.full(len(p),np.inf); sd=np.full(len(p),np.inf)
    active=np.flatnonzero(region>0)
    for i in active:
        if 2.24<p[i,2]<2.50 and p[i,1]<0.:
            md[i]=ctx['oral'].find_nearest(Vector(p[i]))[3]
            mouth[i]=ease(.004,.047,md[i])
        if p[i,2]<2.24:
            sd[i]=ctx['shoulder'].find_nearest(Vector(p[i]))[3]
            shoulder[i]=ease(.003,.045,sd[i])
    signed=np.minimum.reduce([np.abs(np.linalg.norm(p-c,axis=1)-r) for c,r in ctx['eyes']])
    eye=np.ones(len(p)) if shell else ease(.003,.044,signed)
    # These continuous feasible bounds are imposed inside every solve step.
    release=np.minimum.reduce([region,mouth,eye,shoulder])
    half_width=np.interp(z,ctx['zs'],ctx['half_widths'])
    xlimit=np.minimum(.008,.014*half_width)*release
    # A small Z change close to the dome endpoint can disproportionately
    # change a fixed-height cross section. Bound this using the frozen actual
    # half-width derivative, instead of freezing a broad horizontal crown band.
    derivative=np.abs(np.gradient(ctx['half_widths'],ctx['zs']))
    sensitivity=np.interp(z,ctx['zs'],derivative)
    crown_z_limit=.010*half_width/np.maximum(sensitivity,.05)
    crown_mix=ease(2.52,2.59,z)
    general_z_limit=np.minimum(.015 if clear else .010,
        (1-crown_mix)*(.015 if clear else .010)+crown_mix*crown_z_limit)
    # The high shell is weakly supported by the fixed reference. Its endpoint
    # is also ill conditioned in height-parameterized sections. A broad low
    # amplitude feasible domain protects the natural dome without freezing it.
    upper_mix=ease(2.52,2.62,z)
    total_limit=((1-upper_mix)*(.045 if clear else .035)+upper_mix*.012)*release
    return {'region':region,'release':release,'mouth_release':mouth,
        'eye_release':eye,'shoulder_release':shoulder,'mouth_distance':md,
        'eye_absolute_sphere_distance':signed,'shoulder_distance':sd,'sy':sy,
        'total_limit':total_limit,
        'x_limit':xlimit,'z_limit':general_z_limit*release}


def project(d,mask):
    d=d.copy()
    d[:,0]=np.clip(d[:,0],-mask['x_limit'],mask['x_limit'])
    d[:,2]=np.clip(d[:,2],-mask['z_limit'],mask['z_limit'])
    norm=np.linalg.norm(d,axis=1)
    d*=np.minimum(1,mask['total_limit']/np.maximum(norm,1e-20))[:,None]
    d[mask['release']==0]=0
    return d


def bspline_basis(x,a,b,spacing=.060,degree=3):
    inner=np.arange(a+spacing,b-1e-8,spacing)
    knots=np.r_[np.repeat(a,degree+1),inner,np.repeat(b,degree+1)]
    x=np.minimum(np.asarray(x),b-1e-12)
    b0=((x[:,None]>=knots[:-1])&(x[:,None]<knots[1:])).astype(float)
    for k in range(1,degree+1):
        count=len(knots)-k-1
        left=knots[k:k+count]-knots[:count]
        right=knots[k+1:k+count+1]-knots[1:count+1]
        p=np.zeros((len(x),count))
        good=left>0
        p[:,good]+=(x[:,None]-knots[:count])[...,good]/left[good]*b0[:,:count][:,good]
        good=right>0
        p[:,good]+=(knots[k+1:k+count+1]-x[:,None])[...,good]/right[good]*b0[:,1:count+1][:,good]
        b0=p
    return b0,knots


def periodic_basis(theta,ncontrols=40):
    u=np.mod(theta,2*np.pi)/(2*np.pi)*ncontrols
    f=np.floor(u).astype(int);t=u-f
    weights=np.column_stack(((1-t)**3,(3*t**3-6*t*t+4),(-3*t**3+3*t*t+3*t+1),t**3))/6
    b=np.zeros((len(theta),ncontrols))
    for j in range(4):b[np.arange(len(theta)),(f+j-1)%ncontrols]=weights[:,j]
    return b


def sparse_surface_basis(p,ctx,nz,nt):
    bz,_=bspline_basis(p[:,2],ctx['zs'][0],ctx['zs'][-1],spacing=ctx.get('control_spacing',.060))
    centers=np.interp(p[:,2],ctx['zs'],ctx['centers'])
    theta=np.mod(np.arctan2(p[:,1]-centers,p[:,0]),2*np.pi)
    bt=periodic_basis(theta,nt)
    iz=np.argsort(bz,axis=1)[:,-4:];it=np.argsort(bt,axis=1)[:,-4:]
    vz=np.take_along_axis(bz,iz,axis=1);vt=np.take_along_axis(bt,it,axis=1)
    return (iz[:,:,None]*nt+it[:,None,:]).reshape(-1,16),(vz[:,:,None]*vt[:,None,:]).reshape(-1,16)


def actual_mouth_observations(ctx,nz,nt,narrow=False):
    """Source skin positions and normalized real mesh-edge tangents."""
    p=ctx['points'];md=np.full(len(p),np.inf)
    scope=np.flatnonzero((p[:,2]>2.28)&(p[:,2]<2.46)&(p[:,1]<0)&(np.abs(p[:,0])<.28))
    for i in scope:md[i]=ctx['oral'].find_nearest(Vector(p[i]))[3]
    tangent_end=.012 if narrow else .026
    position_end=.008 if narrow else .015
    ids=np.flatnonzero(md<tangent_end)
    bi,bv=sparse_surface_basis(p[ids],ctx,nz,nt)
    position_w=70*(1-ease(.004,position_end,md[ids]))
    obj=bpy.data.objects[BODY]
    edges=array(obj.data.edges,'vertices',2,np.int32)
    sel=(np.maximum(md[edges[:,0]],md[edges[:,1]])<tangent_end)
    edges=edges[sel]
    ai,av=sparse_surface_basis(p[edges[:,0]],ctx,nz,nt)
    ci,cv=sparse_surface_basis(p[edges[:,1]],ctx,nz,nt)
    scale=.015/np.maximum(np.linalg.norm(p[edges[:,0]]-p[edges[:,1]],axis=1),1e-8)
    di=np.concatenate((ai,ci),axis=1)
    dv=np.concatenate((av,-cv),axis=1)*scale[:,None]
    tangent_w=10*(1-ease(.004,tangent_end,np.maximum(md[edges[:,0]],md[edges[:,1]])))
    ncoef=nz*nt
    def observe(c,i,v):return np.sum(c.reshape(-1,3)[i]*v[:,:,None],axis=1)
    def transpose(v,i,b):
        result=np.zeros((ncoef,3))
        for k in range(3):result[:,k]=np.bincount(i.ravel(),weights=(v[:,k,None]*b).ravel(),minlength=ncoef)
        return result.reshape(nz,nt,3)
    target_delta=(p[edges[:,0]]-p[edges[:,1]])*scale[:,None]
    rhs=transpose(position_w[:,None]*p[ids],bi,bv)+transpose(tangent_w[:,None]*target_delta,di,dv)
    def operator(c):return transpose(position_w[:,None]*observe(c,bi,bv),bi,bv)+transpose(tangent_w[:,None]*observe(c,di,dv),di,dv)
    diagonal=np.bincount(bi.ravel(),weights=(position_w[:,None]*bv*bv).ravel(),minlength=ncoef)
    diagonal+=np.bincount(di.ravel(),weights=(tangent_w[:,None]*dv*dv).ravel(),minlength=ncoef)
    return operator,rhs,diagonal.reshape(nz,nt),{'actual_source_mouth_position_observations':int(len(ids)),
        'actual_source_mouth_edge_tangent_observations':int(len(edges)),
        'tangent_normalization_length':.015,'tangent_weight':10.,'tangent_source_fade_end':tangent_end,
        'derivative_source':'frozen actual skin edge vectors; residual multiplied by .015 / true source edge length',
        'position_and_derivative_observations_in_same_global_target_normal_equations':True}


def target_surface(grid,theta,ctx,strength=1.,profile='B',contact_fit=False,actual_tangents=False,hermite_front=False,front_arc=False,angular_front=False,target_tangents=False,integrated_turn=False,lateral_turn=False,balanced_turn=False,rear_curve=False,contact_aware_turn=False,rear_upper_z=2.54,distributed_contact_turn=False):
    """All anatomical goals become one coarse final XYZ surface target."""
    p=grid.reshape(-1,3);z=p[:,2]
    center=np.interp(z,ctx['zs'],ctx['centers'])
    sy=(p[:,1]-center)/np.maximum(np.hypot(p[:,0],p[:,1]-center),1e-12)
    front=1-ease(-.60,-.08,sy);rear=ease(.08,.60,sy)
    side=1-np.abs(sy)
    window=ease(1.96,2.09,z)*(1-ease(ctx['top']-.026,ctx['top'],z))
    # Compact coarse objectives overlap in one surface fit, rather than being
    # added to already-modified geometry. The largest front goal is neck inset.
    face=.0130*np.exp(-.5*((z-2.357)/.049)**2)
    neck=.0230*np.exp(-.5*((z-2.231)/.085)**2)
    forehead=-.0090*np.exp(-.5*((z-2.584)/.038)**2)
    bneck_z=2.375 if profile=='B' else 2.318
    bneck_sigma=.110 if profile=='B' else .096
    bneck_amount=.0220 if profile=='B' else .0150
    bshell_amount=.0120 if profile=='B' else .0140
    back_neck=bneck_amount*np.exp(-.5*((z-bneck_z)/bneck_sigma)**2)
    back_shell=bshell_amount*np.exp(-.5*((z-2.515)/.078)**2)
    shoulder=(-.0080*np.exp(-.5*((z-2.155)/.070)**2) if profile=='B' else
        -.0060*np.exp(-.5*((z-2.095)/.050)**2))
    # Normalized weighted regional goals express a single desired final point.
    fweight=np.exp(-.5*((z-2.357)/.049)**2)
    nweight=np.exp(-.5*((z-2.231)/.085)**2)
    ftarget=(face+neck)/np.maximum(1.,fweight+nweight)
    rweight=np.exp(-.5*((z-bneck_z)/bneck_sigma)**2)
    bweight=np.exp(-.5*((z-2.515)/.078)**2)
    rtarget=(back_neck+back_shell)/np.maximum(1.,rweight+bweight)+shoulder
    desired=p.copy()
    desired[:,1]+=strength*window*(front*(ftarget+forehead)+rear*rtarget)
    desired[:,0]+=strength*window*np.sign(p[:,0])*side*(
        -.0058*np.exp(-.5*((z-2.420)/.072)**2)+.0038*np.exp(-.5*((z-2.258)/.074)**2))
    desired[:,2]+=strength*window*(-.0035*front*np.exp(-.5*((z-2.342)/.060)**2)
        -.0020*rear*np.exp(-.5*((z-2.374)/.091)**2))
    if hermite_front:
        # Six broad observations define the entire front neck/lower-face arc,
        # rather than separately shifted lobes. The true source upper position
        # and tangent are terminal observations in this same final curve.
        za,zb=2.18,2.395;length=zb-za
        anchors=(za,2.30,2.35,zb)
        ysample=np.stack([np.asarray([np.interp(q,ctx['zs'],grid[:,j,1]) for j in range(grid.shape[1])]) for q in anchors])
        ysample[0]+=.020*strength;ysample[1]+=.020*strength;ysample[2]+=.008*strength
        deriv=np.gradient(grid[:,:,1],ctx['zs'],axis=0)
        slope0=np.asarray([np.interp(za,ctx['zs'],deriv[:,j]) for j in range(grid.shape[1])])
        slope1=np.asarray([np.interp(zb,ctx['zs'],deriv[:,j]) for j in range(grid.shape[1])])
        tanchor=(np.asarray(anchors)-za)/length
        matrix=np.vstack([np.asarray([t**k for k in range(6)]) for t in tanchor]+
            [np.asarray([0.,1.,0.,0.,0.,0.]),np.asarray([0.,1.,2.,3.,4.,5.])])
        rhs=np.vstack((ysample,slope0*length,slope1*length))
        coef=np.linalg.solve(matrix,rhs)
        t=np.clip((ctx['zs']-za)/length,0,1)
        arc=np.column_stack([t**k for k in range(6)])@coef
        blend=(front*ease(2.14,2.20,z)*(1-ease(2.390,2.415,z))).reshape(grid.shape[:2])
        planned=desired.reshape(grid.shape)
        planned[:,:,1]=planned[:,:,1]*(1-blend)+arc*blend
        desired=planned.reshape(-1,3)
    angular_report={'enabled':False}
    if angular_front and not integrated_turn:
        # Plan the whole neck-to-mouth turn by its tangent angle, then integrate
        # the final Y curve. A broad changing angle does not reward a straight
        # shelf, unlike Cartesian bending alone. This is one shared surface goal.
        za,zb,zm,radius,peak=2.22,2.35,2.400440216064,.070,2.325
        source=grid[:,int(np.argmin(grid[:, :, 1].mean(axis=0))),1]
        deriv=np.gradient(source,ctx['zs']);length=zb-za;tm=(peak-za)/length
        v=lambda t:np.asarray([t**k for k in range(6)])
        dv=lambda t:np.asarray([0.]+[k*t**(k-1) for k in range(1,6)])
        matrix=np.vstack((v(0),v(1),dv(0),dv(1),v(tm),dv(tm)))
        beta0=np.arctan(-np.interp(za,ctx['zs'],deriv))
        betab=np.arcsin((zm-zb)/radius)
        coef=np.linalg.solve(matrix,[beta0,betab,0.,-length/np.sqrt(radius**2-(zm-zb)**2),np.radians(60.),0.])
        zz=np.linspace(za,zm,1201);tt=np.minimum((zz-za)/length,1.)
        beta=np.column_stack([tt**k for k in range(6)])@coef
        upper=zz>zb;beta[upper]=np.arcsin((zm-zz[upper])/radius)
        assert np.max(np.abs(beta))<np.radians(75.), 'Invalid front arc tangent'
        slope=-np.tan(beta)
        increments=(slope[:-1]+slope[1:])/2*np.diff(zz)
        yy=np.interp(zm,ctx['zs'],source)-np.r_[np.cumsum(increments[::-1])[::-1],0.]
        delta=yy-np.interp(zz,ctx['zs'],source)
        arc_delta=np.interp(z,zz,delta,left=float(delta[0]),right=0.)*strength
        fade=ease(2.15,2.22,z)*(1-ease(2.385,2.420,z))
        desired[:,1]+=window*front*fade*(arc_delta-(ftarget+forehead)*strength)
        angular_report={'enabled':True,'definition':'integrated final tangent-angle curve shared by periodic surface fit',
            'degree':5,'degrees_of_freedom':6,'start_Z':za,'circle_join_Z':zb,'mouth_Z':zm,
            'circle_radius':radius,'maximum_tangent_deflection_degrees':60.,'peak_Z':peak,
            'planned_front_delta_Y_range':[float(delta.min()),float(delta.max())],
            'lower_neck_planned_inset':float(delta[0]),'independent_saved_mesh_review_required':True}
    if front_arc:
        # One final neck-to-mouth arc is specified by endpoint position, tangent
        # and broad endpoint curvature. Interior displacement is free to change
        # sign; separate inset lobes and interior height anchors are not imposed.
        za,zb=2.24,2.395;length=zb-za
        deriv=np.gradient(grid[:,:,1],ctx['zs'],axis=0)
        at=lambda values,q:np.asarray([np.interp(q,ctx['zs'],values[:,j]) for j in range(grid.shape[1])])
        matrix=np.array([[1,0,0,0,0,0],[1,1,1,1,1,1],
            [0,1,0,0,0,0],[0,1,2,3,4,5],[0,0,2,0,0,0],[0,0,2,6,12,20]],float)
        rhs=np.vstack((at(grid[:,:,1],za)+.020*strength,at(grid[:,:,1],zb),
            at(deriv,za)*length,at(deriv,zb)*length,
            np.zeros(grid.shape[1]),np.full(grid.shape[1],12.*length**2)))
        coef=np.linalg.solve(matrix,rhs)
        t=np.clip((ctx['zs']-za)/length,0,1)
        arc=np.column_stack([t**k for k in range(6)])@coef
        blend=(front*ease(2.17,2.24,z)*(1-ease(2.390,2.415,z))).reshape(grid.shape[:2])
        planned=desired.reshape(grid.shape)
        planned[:,:,1]=planned[:,:,1]*(1-blend)+arc*blend
        desired=planned.reshape(-1,3)
    integrated_report={'enabled':False}
    if integrated_turn:
        fw=front.reshape(grid.shape[:2]).mean(axis=0)
        if balanced_turn:
            from h2_balanced_turn import balanced_front_curves
            curves,integrated_report=balanced_front_curves(grid,ctx['zs'],fw,
                strength=strength,lateral_turn=lateral_turn,contact_aware=contact_aware_turn,
                distributed_contact=distributed_contact_turn)
        else:
            curves,integrated_report=integrated_front_curves(grid,ctx['zs'],fw,
                lower_inset=.020*strength,underface_inset=.010*strength,lateral_turn=lateral_turn)
        ctx['turn_upper_ends']=np.asarray(integrated_report['upper_end_Z'])
        planned=desired.reshape(grid.shape)
        # The whole neck/front arc is a final-position goal. Its broad lower
        # join and real upper endpoint are part of the same surface fit.
        if balanced_turn:
            # The new target already includes its lower source join. Do not
            # multiply that join again and change its planned derivative field.
            ends=np.broadcast_to(ctx['turn_upper_ends'][None,:],grid.shape[:2]).ravel()
            mix=(ease(2.04,2.06,z)*(1-ease(ends+.005,ends+.045,z))*
                fw[np.tile(np.arange(grid.shape[1]),grid.shape[0])]).reshape(grid.shape[:2])
        else:
            mix=(ease(2.08,2.22,z)*fw[np.tile(np.arange(grid.shape[1]),grid.shape[0])]).reshape(grid.shape[:2])
        planned[:,:,1]=planned[:,:,1]*(1-mix)+curves*mix
        desired=planned.reshape(-1,3)
        ctx['turn_goal']=planned.copy()
    rear_report={'enabled':False}
    if rear_curve:
        from h2_balanced_turn import balanced_rear_curves
        rw=rear.reshape(grid.shape[:2]).mean(axis=0)
        curves,rear_report=balanced_rear_curves(grid,ctx['zs'],rw,strength=strength,rear_upper_z=rear_upper_z)
        planned=desired.reshape(grid.shape)
        if rear_upper_z>2.54:
            # One final rear curve also owns the upper shell join. Restoring
            # the old displaced shell above its endpoint would make a new lobe.
            mix=(rear*ease(2.02,2.10,z)).reshape(grid.shape[:2])
        else:
            mix=(rear*ease(2.02,2.10,z)*(1-ease(2.54,2.60,z))).reshape(grid.shape[:2])
        planned[:,:,1]=planned[:,:,1]*(1-mix)+curves*mix
        desired=planned.reshape(-1,3)
        ctx['turn_goal']=planned.copy()
        ctx['rear_curve']=True
        ctx['rear_upper_z']=rear_upper_z
    cm=None
    if contact_fit:
        cm=constraints(p,ctx)
        # The initial anatomical goal already respects the source contact and
        # its tangent neighborhood. This occurs before the target coefficients
        # are solved, never as a deformation mask on the finished geometry.
        desired=p+(desired-p)*cm['release'][:,None]
    desired=desired.reshape(grid.shape)
    bz,knots=bspline_basis(ctx['zs'],ctx['zs'][0],ctx['zs'][-1],spacing=ctx.get('control_spacing',.060))
    bt=periodic_basis(theta)
    iz=np.linalg.solve(bz.T@bz+1e-8*np.eye(bz.shape[1]),bz.T)
    it=np.linalg.solve(bt.T@bt+1e-8*np.eye(bt.shape[1]),bt.T)
    coefficients=np.stack([iz@desired[:,:,c]@it.T for c in range(3)],axis=-1)
    fitting_info={'contact_weighted_target_fit':False}
    if contact_fit:
        # High source-fidelity observations in the true contact/tangent band
        # take part in the same XYZ normal equations as every broad shape goal.
        # No separately solved mouth patch is added to the head/neck surface.
        md=cm['mouth_distance']; ed=cm['eye_absolute_sphere_distance']
        mouth_fit_end=.008 if (front_arc or angular_front or integrated_turn) else .015
        w=(1.+250*(1-ease(.004,mouth_fit_end,md))+150*(1-ease(.003,.012,ed))).reshape(grid.shape[:2])
        def evaluate(c):return np.stack([bz@c[:,:,k]@bt.T for k in range(3)],axis=-1)
        def adjoint(v):return np.stack([bz.T@v[:,:,k]@bt for k in range(3)],axis=-1)
        ridge=1e-5
        extra_operator=lambda c:np.zeros_like(c)
        extra_rhs=np.zeros_like(coefficients); extra_diag=np.zeros(coefficients.shape[:2]);extra_report={}
        if actual_tangents:
            extra_operator,extra_rhs,extra_diag,extra_report=actual_mouth_observations(ctx,bz.shape[1],bt.shape[1],narrow=(front_arc or angular_front or integrated_turn))
        dbz=np.gradient(bz,ctx['zs'],axis=0)
        tangent_w=(.018**2*5*front*ease(2.18,2.22,z)*(1-ease(2.375,2.405,z))).reshape(grid.shape[:2]) if target_tangents else np.zeros(grid.shape[:2])
        def derivative(c):return np.stack([dbz@c[:,:,k]@bt.T for k in range(3)],axis=-1)
        def derivative_adjoint(v):return np.stack([dbz.T@v[:,:,k]@bt for k in range(3)],axis=-1)
        def operator(c):return adjoint(w[:,:,None]*evaluate(c))+ridge*c+extra_operator(c)+derivative_adjoint(tangent_w[:,:,None]*derivative(c))
        rhs=adjoint(w[:,:,None]*desired)+extra_rhs+derivative_adjoint(tangent_w[:,:,None]*np.gradient(desired,ctx['zs'],axis=0))
        # Jacobi preconditioned conjugate gradients in the small tensor basis.
        diagonal=(bz*bz).T@w@(bt*bt)+ridge+extra_diag+(dbz*dbz).T@tangent_w@(bt*bt)
        r=rhs-operator(coefficients); pre=r/diagonal[:,:,None]; direction=pre.copy()
        rz=float(np.sum(r*pre)); initial=float(np.linalg.norm(r))
        for itn in range(160):
            applied=operator(direction);alpha=rz/max(float(np.sum(direction*applied)),1e-30)
            coefficients+=alpha*direction;r-=alpha*applied
            relative=float(np.linalg.norm(r))/max(initial,1e-30)
            if relative<2e-7:break
            pre=r/diagonal[:,:,None];newrz=float(np.sum(r*pre))
            direction=pre+(newrz/max(rz,1e-30))*direction;rz=newrz
        fitted=evaluate(coefficients).reshape(-1,3)
        near=md<=.010
        fitting_info={'contact_weighted_target_fit':True,'weighted_fit_iterations':itn+1,
            'weighted_fit_relative_residual':relative,'mouth_source_weight':251.,
            'mouth_tangent_band_full_source_weight_to':.004,'mouth_tangent_band_weight_fades_to':mouth_fit_end,
            'eye_source_weight':151.,'eye_weight_fades_to':.012,
            'maximum_target_source_error_in_mouth_0_010_band':float(np.linalg.norm(fitted[near]-p[near],axis=1).max()) if near.any() else None,
            'planned_final_surface_tangents_in_same_fit':target_tangents,
            'all_goals_and_real_contact_source_observations_share_same_control_coefficients':True,**extra_report}
    target=np.stack([bz@coefficients[:,:,c]@bt.T for c in range(3)],axis=-1)
    return target,coefficients,{'longitudinal_control_spacing':ctx.get('control_spacing',.060),
        'longitudinal_control_count':int(bz.shape[1]),'periodic_control_count':int(bt.shape[1]),
        'periodic_arc_control_spacing_at_radius_0_3':2*np.pi*.3/bt.shape[1],
        'B_spline_degree':3,'knots_Z':knots.tolist(),'initial_neck_inset':.023*strength,
        'initial_lower_face_inset':.013*strength,'initial_back_neck_fill':bneck_amount*strength,
        'back_neck_joint_target_center':bneck_z,'back_neck_joint_target_width':bneck_sigma,
        'initial_back_shell_fill':bshell_amount*strength,'initial_forehead_expand':.009*strength,
        'initial_side_X_inset':.0058*strength,'initial_side_X_fill':.0038*strength,
        'initial_front_Z_redistribution':-.0035*strength,
        'joint_front_Hermite_target':{'enabled':hermite_front,'longitudinal_degrees_of_freedom':6,
            'source_position_observation_Z':[2.18,2.30,2.35,2.395],
            'Y_inset_at_observations':[.020*strength,.020*strength,.008*strength,0.],
            'terminal_tangents':'actual frozen source periodic section Y slope at Z2.18 and Z2.395'},
        'joint_front_endpoint_arc':{'enabled':front_arc,'degree':5,'degrees_of_freedom':6,
            'endpoint_Z':[2.24,2.395],'lower_Y_inset':.020*strength,
            'endpoint_Y_second_derivatives':[0.,12.],
            'interior_Y_inset_anchors':False,'may_fill_underface_to_spread_turn':True},
        'joint_front_angular_arc':angular_report,'joint_integrated_front_turn':integrated_report,
        'joint_rear_curve':rear_report,**fitting_info}


def solve_with_turn_tangents(grid,target,mask,ctx,iterations=1800,rho=80.,sigma_z=.018,sigma_arc=.020):
    """Joint constrained positions AND target first derivatives in the final solve.

    ADMM splits the positional constraints and the weighted derivative fidelity.
    The smooth final XYZ update is diagonal in the mirrored periodic FFT basis.
    This avoids fitting tangents only in an intermediate target, then losing them
    in a position-only smoother. Derivative observations are never a saved-mesh
    validation substitute.
    """
    nz,nt,_=grid.shape;length=2*(nz-1);dz=float(ctx['zs'][1]-ctx['zs'][0])
    reflect=lambda a:np.concatenate((a,a[-2:0:-1]),axis=0)
    base,goal,turn=map(reflect,(grid,target,ctx['turn_goal']))
    extended={k:reflect(v.reshape(nz,nt)).ravel() for k,v in mask.items()}
    zz=grid[:,:,2];sy=mask['sy'].reshape(nz,nt)
    front=1-ease(-.60,-.08,sy)
    ends=ctx.get('turn_upper_ends',np.full(nt,2.400440216064))[None,:]
    weight=reflect(.025**2*24*front*ease(2.16,2.22,zz)*(1-ease(ends-.018,ends+.016,zz)))
    if ctx.get('rear_curve',False):
        rear=ease(.08,.60,sy)
        rear_end=ctx.get('rear_upper_z',2.54)
        weight+=reflect(.025**2*18*rear*ease(2.08,2.14,zz)*(1-ease(rear_end,rear_end+.06,zz)))
    confidence=reflect((1+2*mask['region']+50*(1-ease(.004,.028,mask['mouth_distance']))).reshape(nz,nt))
    confidence+=reflect(10*front*ease(2.16,2.22,zz)*(1-ease(2.38,2.43,zz)))
    kz=2*np.pi*np.fft.fftfreq(length,dz)
    ka=2*np.pi*np.fft.rfftfreq(nt,2*np.pi/nt)/.30
    bending=(sigma_z**2*kz[:,None]**2+sigma_arc**2*ka[None,:]**2)**2
    rho_derivative=.006
    multiplier=(np.exp(1j*kz*dz)-1)/dz
    denominator=rho+bending+rho_derivative*np.abs(multiplier[:,None])**2
    rho_history=[{'iteration':0,'rho':rho}]
    derivative=lambda a:(np.roll(a,-1,axis=0)-a)/dz
    adjoint=lambda a:(np.roll(a,1,axis=0)-a)/dz
    def fair(rhs):
        return np.stack([np.fft.irfft2(np.fft.rfft2(rhs[:,:,k])/denominator,s=(length,nt)) for k in range(3)],axis=-1)
    feasible=base+project((goal-base).reshape(-1,3),extended).reshape(base.shape)
    smooth=feasible.copy();gradient=derivative(smooth);wanted=derivative(turn)
    dual=np.zeros_like(base);gradient_dual=np.zeros_like(base)
    for iteration in range(iterations):
        smooth=fair(rho*(feasible-dual)+rho_derivative*adjoint(gradient-gradient_dual))
        old=feasible
        proposal=(confidence[:,:,None]*goal+rho*(smooth+dual))/(confidence[:,:,None]+rho)
        feasible=base+project((proposal-base).reshape(-1,3),extended).reshape(base.shape)
        observed=derivative(smooth)
        gradient=(weight[:,:,None]*wanted+rho_derivative*(observed+gradient_dual))/(weight[:,:,None]+rho_derivative)
        dual+=smooth-feasible;gradient_dual+=observed-gradient
        primal=float(np.max(np.linalg.norm(smooth-feasible,axis=-1)))
        tangent_residual=float(np.max(np.linalg.norm(observed-gradient,axis=-1)))
        change=float(np.max(np.linalg.norm(feasible-old,axis=-1)))
        if (iteration+1)%200==0:
            print('H2_FINAL_POSITION_AND_TANGENT_SOLVE',iteration+1,primal,tangent_residual,change,flush=True)
        if (iteration+1)%400==0 and primal>5e-5 and change<1e-5 and rho<1280.:
            old_rho=rho;rho=min(1280.,rho*2.)
            dual*=old_rho/rho
            denominator=rho+bending+rho_derivative*np.abs(multiplier[:,None])**2
            rho_history.append({'iteration':iteration+1,'rho':rho})
        if iteration>=200 and primal<2e-6 and tangent_residual<2e-4 and change<3e-8:
            break
    largest=np.unravel_index(np.argmax(np.linalg.norm(smooth-feasible,axis=-1)),(length,nt))
    return feasible[:nz]-grid,{'method':'joint final XYZ bending, positional fidelity, derivative-field fidelity and real feature constraints',
        'iterations':iteration+1,'rho':rho,'rho_history':rho_history,'derivative_rho':rho_derivative,
        'sigma_Z':sigma_z,'sigma_arc':sigma_arc,'primal_residual':primal,
        'derivative_split_residual':tangent_residual,'last_change':change,
        'final_derivative_fidelity_in_same_constrained_solve':True,
        'maximum_position_split_residual_at_source':[float(v) for v in base[largest]],
        'derivative_weight_maximum':float(weight.max()),'grid_shape':list(grid.shape),
        'mouth_exact_distance':.004,'mouth_full_release':.047,
        'eye_exact_sphere_intersection_distance':.003,'eye_full_release':.044,
        'old_spherical_installation_disk_not_frozen':True,
        'old_Z2_12_Z2_22_Z2_31_endpoint_locks_removed':True,
        'strict_convergence':bool(primal<2e-6 and tangent_residual<2e-4 and change<3e-8),
        'independent_actual_surface_and_visual_review_required':True}


def solve(grid,target,mask,ctx,iterations=1100,rho=35.,sigma_z=.020,sigma_arc=.020,weighted_final=False,angular_front=False):
    nz,nt,_=grid.shape; length=2*(nz-1)
    dz=ctx['zs'][1]-ctx['zs'][0]
    kz=2*np.pi*np.fft.fftfreq(length,dz)
    ka=2*np.pi*np.fft.rfftfreq(nt,2*np.pi/nt)/.30
    bending=(sigma_z**2*kz[:,None]**2+sigma_arc**2*ka[None,:]**2)**2
    denominator=(rho if weighted_final else 1+rho)+bending
    confidence=(1+2*mask['region']+50*(1-ease(.004,.028,mask['mouth_distance']))).reshape(nz,nt) if weighted_final else None
    if weighted_final and angular_front:
        zz=grid[:,:,2].ravel()
        confidence+=(18*(1-ease(-.60,-.08,mask['sy']))*ease(2.17,2.22,zz)*(1-ease(2.38,2.43,zz))).reshape(nz,nt)
    def reflect(a):return np.concatenate((a,a[-2:0:-1]),axis=0)
    def fair(rhs):
        return np.stack([np.fft.irfft2(np.fft.rfft2(reflect(rhs[:,:,c]))/denominator,s=(length,nt))[:nz] for c in range(3)],axis=-1)
    v=grid+project((target-grid).reshape(-1,3),mask).reshape(grid.shape)
    dual=np.zeros_like(v)
    for i in range(iterations):
        u=fair(rho*(v-dual) if weighted_final else target+rho*(v-dual));old=v
        proposed=((confidence[:,:,None]*target+rho*(u+dual))/(confidence[:,:,None]+rho)) if weighted_final else u+dual
        v=grid+project((proposed-grid).reshape(-1,3),mask).reshape(grid.shape)
        dual+=u-v
        primal=float(np.max(np.linalg.norm(u-v,axis=-1)))
        change=float(np.max(np.linalg.norm(v-old,axis=-1)))
        if (i+1)%200==0:print('H2_JOINT_XYZ_SOLVE',i+1,primal,change,flush=True)
        if i>=150 and primal<1e-6 and change<3e-8:break
    return v-grid,{'method':'coarse periodic cubic XYZ target + single ADMM final-surface bending/fidelity solve',
        'iterations':i+1,'rho':rho,'sigma_Z':sigma_z,'sigma_arc':sigma_arc,
        'primal_residual':primal,'last_change':change,
        'grid_shape':list(grid.shape),'actual_source_Z_step':float(dz),
        'protected_bounds_imposed_during_every_joint_iteration':True,
        'end_condition':'continuous source displacement bounds at crown anchor/lower chest and real contacts',
        'mouth_exact_distance':.004,'mouth_full_release':.047,'eye_exact_sphere_intersection_distance':.003,
        'eye_full_release':.044,'old_spherical_installation_disk_not_frozen':True,
        'old_Z2_12_Z2_22_Z2_31_endpoint_locks_removed':True,
        'high_shell_continuous_total_limit':{'from_Z':2.52,'to_Z':2.62,'maximum_at_top':.012,'not_frozen':True},
        'crown_Z_limit':'1% of actual frozen local half-width divided by source half-width slope, smoothly applied 2.52-2.59',
        'X_limit':'min(.008,1.4% of frozen local half-width) times joint continuous feasible release',
        'weighted_final_target_fidelity':{'enabled':weighted_final,'general_active_region':3.,'mouth_maximum':53.,'mouth_fade_end':.028},
        'front_angular_goal_fidelity_extra':18. if angular_front else 0.,
        'strict_convergence':bool(primal<1e-6 and change<3e-8)}


def sculpt(options):
    body=bpy.data.objects[BODY]
    ctx=make_context(body)
    grid,normals,theta=freeze_grid(ctx)
    ctx['control_spacing']=options.get('control_spacing',.060)
    print('H2_ACTUAL_PERIODIC_GRID_READY',grid.shape,flush=True)
    target,coefficients,target_report=target_surface(grid,theta,ctx,options['strength'],options['profile'],options['contact_fit'],options['actual_tangents'],options['hermite_front'],options.get('front_arc',False),options.get('angular_front',False),options.get('target_tangents',False),options.get('integrated_turn',False),options.get('lateral_turn',False),options.get('balanced_turn',False),options.get('rear_curve',False),options.get('contact_aware_turn',False),options.get('rear_upper_z',2.54),options.get('distributed_contact_turn',False))
    gm=constraints(grid.reshape(-1,3),ctx,options['clear_version'])
    if options.get('integrated_turn',False) or options.get('rear_curve',False):
        field,report=solve_with_turn_tangents(grid,target,gm,ctx,iterations=options['iterations'],
            rho=options['rho'],sigma_z=options['sigma_z'],sigma_arc=options['sigma_arc'])
    else:
        field,report=solve(grid,target,gm,ctx,iterations=options['iterations'],
            rho=options['rho'],sigma_z=options['sigma_z'],sigma_arc=options['sigma_arc'],weighted_final=options['weighted_final'],angular_front=options.get('angular_front',False))
    reports={}; saved=None
    for name in (BODY,SHELL):
        obj=bpy.data.objects.get(name)
        if obj is None:continue
        old=array(obj.data.vertices,'co',3,np.float32);p=points(obj)
        assert np.allclose(np.asarray(obj.matrix_world),np.eye(4),atol=1e-10)
        cm=constraints(p,ctx,options['clear_version'],shell=name==SHELL)
        d=project(interpolate(p,ctx,field),cm)
        new=(old.astype(float)+d).astype(np.float32)
        # Exact fixed feature/lower-body coordinates, using the same feasible
        # set as the solver; no additional tapered displacement is multiplied.
        new[cm['release']==0]=old[cm['release']==0]
        crown=p[:,2]>=ctx['top']-1e-7
        new[crown]=old[crown]
        actual=new.astype(float)-old.astype(float)
        obj.data.vertices.foreach_set('co',new.ravel());obj.data.update()
        reports[name]={'changed_vertices':int(np.count_nonzero(np.linalg.norm(actual,axis=1))),
            'maximum_total_displacement':float(np.linalg.norm(actual,axis=1).max()),
            'maximum_abs_X':float(np.abs(actual[:,0]).max()),'maximum_abs_Z':float(np.abs(actual[:,2]).max()),
            'delta_Y_range':[float(actual[:,1].min()),float(actual[:,1].max())],
            'crown_anchor_vertices':int(crown.sum()),'narrow_contact_vertices':int((cm['release']==0).sum()),
            'has_custom_normals_before':bool(obj.data.has_custom_normals),
            'custom_normals_action':'preserved representation; geometry update only'}
        if name==BODY:
            saved={'source_points':p,'source_normals':ctx['normals'],'triangles':ctx['triangles'],
                'actual_delta':actual,'grid_source':grid,'grid_result':grid+field,'grid_target':target,
                'grid_delta':field,'target_B_spline_coefficients_XYZ':coefficients,
                'grid_Z':ctx['zs'],'grid_theta':theta,'grid_center_Y':ctx['centers'],**cm}
            if 'turn_goal' in ctx:saved['grid_final_turn_goal']=ctx['turn_goal']
    bpy.data.objects[SHELL].hide_render=True
    bpy.context.view_layer.update()
    return {'target_surface':target_report,'solver':report,'surfaces':reports,'D0':D0,
        'topology_UV_weights_materials_policy':'coordinate update only; all mesh data and asset links retained',
        'visual_acceptance':'pending; numeric solve is not proof of eliminating overall steps'},saved
