"""Final-geometry mouth transition correction on fixed X/Z triangle sections.

This module has no Blender dependency. Its small XY correction is optimized
jointly in real triangle-plane section derivatives, including all protection
weights inside the basis/Jacobian. No optimized correction is masked afterward.
"""
import numpy as np


def ease(a,b,x):
    t=np.clip((x-a)/(b-a),0,1)
    return t**3*(10+t*(-15+6*t))


def correction_basis(points,masks,parameters):
    p=np.asarray(points,float)
    z=p[:,2]
    width=float(parameters['sigma_Z'])
    centers=np.asarray(parameters['control_z'],float)
    transverse=1-ease(parameters['X_full'],parameters['X_zero'],np.abs(p[:,0]))
    boundary=ease(parameters['Z_zero_low'],parameters['Z_full_low'],z)*(1-ease(parameters['Z_full_high'],parameters['Z_zero_high'],z))
    # 'front' already includes the mouth, eye, shoulder and regional guards.
    # The source spatial weights are in the basis before differentiating final
    # sections, and the final total-displacement feasible set is separate.
    protection=np.asarray(masks['front'],float)
    base=np.exp(-.5*((z[:,None]-centers[None,:])/width)**2)*(transverse*boundary*protection)[:,None]
    modes=parameters.get('transverse_modes',['constant'])
    scale=parameters.get('transverse_scale',.08)
    xx=p[:,0]/scale
    factors={'constant':np.ones(len(p)),'linearX':xx,'quadraticX':xx*xx}
    return np.concatenate([base*factors[mode][:,None] for mode in modes],axis=1)


def evaluate_mouth_correction(points,masks,report):
    """Reuse the identical source-coordinate field on the hidden historical shell."""
    basis=correction_basis(points,masks,report['basis'])
    result=np.zeros((len(points),2),float)
    result[:,1]=basis@np.asarray(report['coefficients_Y'],float)
    return result


def sample_front_operator(points,triangles,x,z,basis):
    """Freeze actual final-X/Z barycentric front-section sampling coefficients."""
    tp=points[np.asarray(triangles,np.int64)]
    d=tp[:,:,0]-x
    relevant=(d.min(1)<=1e-10)&(d.max(1)>=-1e-10)&(tp[:,:,2].max(1)>=z[0])&(tp[:,:,2].min(1)<=z[-1])
    ids=np.asarray(triangles,np.int64)[relevant]
    candidates=[]
    for triangle in ids:
        p=points[triangle];dist=p[:,0]-x;hits=[]
        for a,b in ((0,1),(1,2),(2,0)):
            if abs(dist[a])<1e-10:
                w=np.zeros(3);w[a]=1.;hits.append((p[a],w))
            if dist[a]*dist[b]<-1e-20:
                t=dist[a]/(dist[a]-dist[b]);w=np.zeros(3);w[a]=1-t;w[b]=t
                hits.append((w@p,w))
        unique=[]
        for hit,w in hits:
            if not any(np.linalg.norm(hit-q)<1e-9 for q,_ in unique):unique.append((hit,w))
        if len(unique)>=2:
            pairs=[(np.linalg.norm(a-c),a,wa,c,wc) for j,(a,wa) in enumerate(unique) for c,wc in unique[j+1:]]
            _,a,wa,b,wb=max(pairs,key=lambda q:q[0])
            if abs(b[2]-a[2])>1e-12:candidates.append((triangle,a,b,wa,wb))
    out=np.empty(len(z));rows=np.empty((len(z),basis.shape[1]));sample_ids=np.empty((len(z),3),np.int64);weights=np.empty((len(z),3))
    for j,height in enumerate(z):
        best=None
        for triangle,a,b,wa,wb in candidates:
            if min(a[2],b[2])-1e-10<=height<=max(a[2],b[2])+1e-10:
                t=(height-a[2])/(b[2]-a[2]);w=wa+t*(wb-wa)
                y=float(w@points[triangle,1])
                if best is None or y<best[0]:best=(y,triangle,w)
        if best is None:raise ValueError(f'Missing real front section at X={x}, Z={height}')
        y,triangle,w=best
        out[j]=y;rows[j]=w@basis[triangle];sample_ids[j]=triangle;weights[j]=w
    return out,rows,sample_ids,weights


def derivative_operator(z):
    step=float(np.median(np.diff(z)));n=19
    t=np.arange(-n,n+1)*step/.010
    fit=np.linalg.pinv(np.column_stack((np.ones_like(t),t,t*t,t*t*t)))
    c1=np.zeros((len(z),len(z)));c2=c1.copy()
    for i in range(n,len(z)-n):
        c1[i,i-n:i+n+1]=fit[1]/.010
        c2[i,i-n:i+n+1]=2*fit[2]/.010**2
    return c1,c2


def peak_record(z,d1,d2,bounds=(2.28,2.44)):
    k=np.abs(d2)/(1+d1*d1)**1.5
    take=np.flatnonzero((z>=bounds[0])&(z<=bounds[1]))
    i=take[np.argmax(k[take])]
    return {'Z':float(z[i]),'curvature':float(k[i])}


def refine_mouth_curvature(points,source_normals,triangles,global_delta_xy,masks,*,target_peak=26.6,iterations=20000):
    p=np.asarray(points,float);n=np.asarray(source_normals,float)
    global_delta_xy=np.asarray(global_delta_xy,float)
    global_points=p.copy();global_points[:,:2]+=global_delta_xy
    parameters={'control_z':[2.3302164,2.3400221,2.3490031,2.3571877,2.3646443,2.3714500,2.3776150,2.3830159],
        'sigma_Z':.0065,'X_full':.04,'X_zero':.15,'Z_zero_low':2.305,'Z_full_low':2.325,
        'Z_full_high':2.395,'Z_zero_high':2.425,'transverse_modes':['constant','linearX','quadraticX'],'transverse_scale':.08,
        'weight':'Gaussian Z control functions × wide C2 X/Z envelope × protected front mask inside final-section Jacobian'}
    full_basis=correction_basis(p,masks,parameters)
    active=np.flatnonzero(np.max(np.abs(full_basis),axis=1)>1e-15)
    basis=full_basis[active]
    dx=global_delta_xy[active,0];dy=global_delta_xy[active,1]
    total=np.asarray(masks['total_limit'])[active]
    allowed=np.sqrt(np.maximum(total*total-dx*dx,0))
    lower=-allowed-dy;upper=allowed-dy
    dot_x=n[active,0]*dx;ny=n[active,1];normal=np.asarray(masks['normal_limit'])[active]
    nonzero=np.abs(ny)>1e-10
    nl=np.full(len(active),-np.inf);nu=-nl
    aa=(-normal[nonzero]-dot_x[nonzero])/ny[nonzero]-dy[nonzero]
    bb=(normal[nonzero]-dot_x[nonzero])/ny[nonzero]-dy[nonzero]
    nl[nonzero]=np.minimum(aa,bb);nu[nonzero]=np.maximum(aa,bb)
    lower=np.maximum(lower,nl);upper=np.minimum(upper,nu)
    # Native float32 write-back and the previous convex projection can leave
    # roundoff-size excess. This is the optimization feasible set tolerance.
    tolerance=8e-8
    lower=np.minimum(lower,0)-tolerance;upper=np.maximum(upper,0)+tolerance
    z=np.round(np.arange(2.25,2.460001,.0005),8)
    c1,c2=derivative_operator(z)
    fit_range=(z>=2.335)&(z<=2.400)
    neighbor=(z>=2.28)&(z<=2.44)
    curves=[]
    for x in (0.,-.04,.04,-.08,.08):
        y,rows,_,_=sample_front_operator(global_points,triangles,x,z,full_basis)
        a1=c1@rows;a2=c2@rows;s1=c1@y;s2=c2@y
        base_peak=peak_record(z,s1,s2)
        desired=target_peak if x==0 else base_peak['curvature']+.05
        threshold=np.full(len(z),desired)
        curves.append({'kind':'longitudinal','X':x,'Y':y,'A1':a1[neighbor],'A2':a2[neighbor],
            's1':s1[neighbor],'s2':s2[neighbor],'limit':threshold[neighbor],
            'weight':1. if x==0 else .35,'before':base_peak,'all_A1':a1,'all_A2':a2,'all_s1':s1,'all_s2':s2})
    xx=np.round(np.arange(-.17,.170001,.0005),8)
    hx1,hx2=derivative_operator(xx)
    horizontal_points=global_points[:,[2,1,0]]
    source_horizontal_points=p[:,[2,1,0]]
    hkeep=np.abs(xx)<=.15
    for height in (2.340,2.357,2.371,2.378,2.383):
        y,rows,_,_=sample_front_operator(horizontal_points,triangles,height,xx,full_basis)
        s1=hx1@y;s2=hx2@y;a1=hx1@rows;a2=hx2@rows
        before=np.abs(s2)/(1+s1*s1)**1.5
        source_y,_,_,_=sample_front_operator(source_horizontal_points,triangles,height,xx,np.zeros((len(p),1)))
        source1=hx1@source_y;source2=hx2@source_y
        source_signed=source2/(1+source1*source1)**1.5
        source_max=float(np.max(np.abs(source_signed[hkeep])))
        cap=max(source_max*1.15+.5,float(before[hkeep].max())+.75)
        signed_floor=min(0.,float(source_signed[hkeep].min()))-1.
        curves.append({'kind':'horizontal','Z':height,'Y':y,'A1':a1[hkeep],'A2':a2[hkeep],
            's1':s1[hkeep],'s2':s2[hkeep],'limit':np.full(hkeep.sum(),cap),
            'weight':.40,'before':{'curvature':float(before[hkeep].max())},
            'source_N2_max':source_max,'source_N2_signed_min':float(source_signed[hkeep].min()),
            'signed_lower':signed_floor,
            'all_A1':a1,'all_A2':a2,'all_s1':s1,'all_s2':s2})
    x=np.zeros(full_basis.shape[1]);best=None
    coefficient_scale=.01
    bsmall=basis*coefficient_scale
    # Exact linear interval feasibility in coefficient space. Both bounds
    # contain zero, so uniform contraction is a safe convex projection.
    def feasible(coeff):
        q=bsmall@coeff
        factor=1.
        pos=q>upper
        neg=q<lower
        if pos.any():factor=min(factor,float(np.min(upper[pos]/q[pos])))
        if neg.any():factor=min(factor,float(np.min(lower[neg]/q[neg])))
        return coeff*max(0.,min(1.,factor))
    def objective(coeff,hessian=False):
        loss=0.;grad=np.zeros_like(coeff);H=np.zeros((len(coeff),len(coeff)))
        for curve in curves:
            d1=curve['s1']+curve['A1']@(coeff*coefficient_scale)
            d2=curve['s2']+curve['A2']@(coeff*coefficient_scale)
            den=1+d1*d1;k=d2/den**1.5
            excess=np.maximum(np.abs(k)-curve['limit'],0)
            jac=coefficient_scale*(curve['A2']/den[:,None]**1.5-3*(d2*d1/den**2.5)[:,None]*curve['A1'])
            weight=100.*curve['weight']/len(k)
            loss+=weight*float(np.sum(excess**2))
            grad+=2*weight*np.sum((excess*np.sign(k))[:,None]*jac,axis=0)
            if hessian:
                jj=jac[excess>0]
                H+=2*weight*(jj.T@jj)
            if 'signed_lower' in curve:
                lower_excess=np.maximum(curve['signed_lower']-k,0)
                loss+=weight*float(np.sum(lower_excess**2))
                grad-=2*weight*np.sum(lower_excess[:,None]*jac,axis=0)
                if hessian:
                    jj=jac[lower_excess>0]
                    H+=2*weight*(jj.T@jj)
        q=bsmall@coeff
        fidelity=.08/(len(q)*.002**2)
        loss+=fidelity*float(q@q)
        grad+=2*fidelity*(bsmall.T@q)
        if hessian:H+=2*fidelity*(bsmall.T@bsmall)
        return loss,grad,H
    for iteration in range(1,min(iterations,800)+1):
        loss,grad,H=objective(x,True)
        if best is None or loss<best[0]:best=(loss,x.copy())
        damping=max(1e-7,float(np.max(np.diag(H)))*1e-7)
        step=np.linalg.lstsq(H+damping*np.eye(len(x)),-grad,rcond=1e-12)[0]
        norm=float(np.max(np.abs(bsmall@step)))
        if norm>.002:step*=.002/norm
        accepted=False
        for power in range(15):
            trial=feasible(x+step*(.5**power))
            value,_,_=objective(trial)
            if value<loss-1e-12:
                x=trial;accepted=True;break
        if not accepted:break
    coefficients=best[1]*coefficient_scale
    correction=full_basis@coefficients
    final=global_delta_xy.copy();final[:,1]+=correction
    final[masks['region']==0]=global_delta_xy[masks['region']==0]
    records=[]
    for curve in curves:
        if curve['kind']!='longitudinal':continue
        d1=curve['all_s1']+curve['all_A1']@coefficients
        d2=curve['all_s2']+curve['all_A2']@coefficients
        records.append({'X':curve['X'],'global_before_neighbor_peak':curve['before'],
            'final_neighbor_peak':peak_record(z,d1,d2),
            'final_fold_region_peak':peak_record(z,d1,d2,(2.335,2.400))})
    length=np.linalg.norm(final,axis=1);normal_move=np.abs(np.sum(final*n[:,:2],axis=1))
    horizontal=[]
    for curve in curves:
        if curve['kind']!='horizontal':continue
        before1=curve['all_s1'];before2=curve['all_s2']
        after1=before1+curve['all_A1']@coefficients
        after2=before2+curve['all_A2']@coefficients
        before_k=np.abs(before2)/(1+before1*before1)**1.5
        after_k=np.abs(after2)/(1+after1*after1)**1.5
        keep=np.abs(xx)<=.15
        horizontal.append({'Z':curve['Z'],'X_range':[-.15,.15],
            'source_N2_max_curvature':curve['source_N2_max'],
            'before_max_curvature':float(before_k[keep].max()),
            'final_max_curvature':float(after_k[keep].max()),
            'target_max_curvature':float(curve['limit'][0]),
            'source_N2_signed_min_curvature':curve['source_N2_signed_min'],
            'final_signed_min_curvature':float(np.min(after2[keep]/(1+after1[keep]**2)**1.5)),
            'signed_lower_limit':curve['signed_lower'],
            'same_actual_triangle_section_sampling':True})
    report={'method':'small Y control correction optimized in actual fixed-X/Z triangle sections; protection weights inside basis and Jacobian',
        'basis':parameters,'coefficients_Y':coefficients.tolist(),'iterations':iteration,
        'optimizer':'damped small-variable Gauss-Newton with actual-section curvature hinge and exact interval feasibility',
        'sampling_step':.0005,'fit_half_window_strict':.010,'samples_per_window':39,
        'target_central_peak':target_peak,'loss':best[0],'sections':records,
        'maximum_abs_correction_Y':float(np.max(np.abs(correction))),
        'RMS_correction_Y':float(np.sqrt(np.mean(correction[active]**2))),
        'maximum_total_final_displacement':float(length.max()),
        'maximum_total_cap_excess':float(np.max(length-np.asarray(masks['total_limit']))),
        'maximum_normal_cap_excess':float(np.max(normal_move-np.asarray(masks['normal_limit']))),
        'transverse_curvature':horizontal,'transverse_weight_C2':True,
        'transverse_modes_are_low_frequency':True,'absence_of_visible_stripes_requires_render_review':True,
        'X_Z_policy':'source Z and pre-correction global X unchanged; no final correction masking',
        'feasible_tolerance':tolerance,'not_visual_acceptance':True}
    return final,report
