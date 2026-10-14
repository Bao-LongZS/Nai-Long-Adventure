"""Pure NumPy diagnostics of actual R1/N2 triangle sections.

The curve metric is a local cubic fit of the *actual section*, not the
displacement. Engineering coordinates and the old N2 sampling are fixed.
"""
import numpy as np

STEP = .0005
HALF_WINDOW = .010
PEAK_REGION = (2.335, 2.400)
NEIGHBOR_REGION = (2.280, 2.440)


def section_segments(section):
    """Accept section_object dictionaries, wrappers, or an (n,2,3) array."""
    if isinstance(section, dict):
        section = section.get('body', section)
        return np.asarray([s['points'] for s in section['segments']], float)
    return np.asarray(section, float)


def sample_section_branches(section, z=None):
    """Interpolate exact triangle-plane segments at fixed Z, front=min Y."""
    if z is None:
        z = np.round(np.arange(2.05, 2.5100001, STEP), 8)
    sampled=sample_axis_branches(section,z,2,1)
    return {'Z':sampled['parameter'],'front_Y':sampled['lower_boundary'],'rear_Y':sampled['upper_boundary']}


def sample_axis_branches(section, values, parameter_axis=2, boundary_axis=1):
    """Sample actual segment intersections in any axis, preserving minima/maxima."""
    segments=section_segments(section)
    z=np.asarray(values,float)
    a, b = segments[:, 0], segments[:, 1]
    dz = b[:, parameter_axis]-a[:, parameter_axis]
    valid = np.abs(dz)>1e-12
    a, b, dz = a[valid], b[valid], dz[valid]
    front = np.full(z.shape, np.nan)
    rear = np.full(z.shape, np.nan)
    for i, height in enumerate(z):
        take = (height>=np.minimum(a[:,parameter_axis],b[:,parameter_axis])-1e-10)&(height<=np.maximum(a[:,parameter_axis],b[:,parameter_axis])+1e-10)
        ys = a[take,boundary_axis]+(height-a[take,parameter_axis])/dz[take]*(b[take,boundary_axis]-a[take,boundary_axis])
        if len(ys):
            front[i], rear[i] = ys.min(), ys.max()
    return {'parameter':z,'lower_boundary':front,'upper_boundary':rear}


def triangle_plane_segments(points, triangles, axis=0, value=0.):
    """Intersect actual triangles with a coordinate plane; no ray fitting."""
    points, triangles = np.asarray(points,float), np.asarray(triangles,np.int64)
    tp=points[triangles]
    d=tp[:,:,axis]-value
    relevant=(d.min(1)<=1e-10)&(d.max(1)>=-1e-10)
    result=[]
    for tri,dist in zip(tp[relevant],d[relevant]):
        intersections=[]
        for j,k in ((0,1),(1,2),(2,0)):
            if abs(dist[j])<=1e-10:
                intersections.append(tri[j])
            if dist[j]*dist[k]<-1e-20:
                t=dist[j]/(dist[j]-dist[k])
                intersections.append(tri[j]+t*(tri[k]-tri[j]))
        if len(intersections)<2:continue
        unique=[]
        for p in intersections:
            if not any(np.linalg.norm(p-q)<1e-9 for q in unique):unique.append(p)
        if len(unique)==2:
            result.append(unique)
        elif len(unique)>2:
            pairs=[(np.linalg.norm(a-b),a,b) for i,a in enumerate(unique) for b in unique[i+1:]]
            _,a,b=max(pairs,key=lambda v:v[0]);result.append((a,b))
    return np.asarray(result,float).reshape((-1,2,3))


def curve_diagnostics(z, y, half_window=HALF_WINDOW,
                      peak_region=PEAK_REGION, neighbor_region=NEIGHBOR_REGION,
                      expanded_region=(2.20,2.50)):
    """Fit cubics on +/-0.010 and return actual Y(Z) curvature and peaks.

    A complete symmetric window is required; end samples are NaN. The fit is
    ordinary least squares on uniformly sampled actual triangle intersections.
    """
    z,y=np.asarray(z,float),np.asarray(y,float)
    fit=np.full_like(y,np.nan);d1=fit.copy();d2=fit.copy()
    if len(z)<9:raise ValueError('Too few section samples')
    step=float(np.median(np.diff(z)))
    # The read-only baseline used |z-z0| < 0.010, so samples exactly at the
    # nominal +/-0.010 boundary are excluded. This is 39 points at step .0005.
    n=int(np.ceil(half_window/step-1e-7))-1
    regular=np.max(np.abs(np.diff(z)-step))<1e-8
    if regular:
        t=np.arange(-n,n+1)*step/half_window
        operator=np.linalg.pinv(np.column_stack((np.ones_like(t),t,t*t,t*t*t)))
        for i in range(n,len(z)-n):
            window=y[i-n:i+n+1]
            if not np.isfinite(window).all():continue
            c=operator@window
            fit[i]=c[0];d1[i]=c[1]/half_window;d2[i]=2*c[2]/half_window**2
    else:
        for i,center in enumerate(z):
            take=(np.abs(z-center)<half_window-1e-10)&np.isfinite(y)
            if take.sum()<9 or center-z[0]<half_window-1e-9 or z[-1]-center<half_window-1e-9:continue
            t=(z[take]-center)/half_window
            c=np.linalg.lstsq(np.column_stack((np.ones_like(t),t,t*t,t*t*t)),y[take],rcond=None)[0]
            fit[i]=c[0];d1[i]=c[1]/half_window;d2[i]=2*c[2]/half_window**2
    signed=d2/np.power(1+d1*d1,1.5);curvature=np.abs(signed)
    def peak(bounds):
        selected=np.flatnonzero((z>=bounds[0]-1e-9)&(z<=bounds[1]+1e-9)&np.isfinite(curvature))
        if not len(selected):return None
        idx=selected[np.argmax(curvature[selected])]
        return {'Z':float(z[idx]),'Y':float(y[idx]),'curvature':float(curvature[idx]),'signed_curvature':float(signed[idx]),'sample_index':int(idx)}
    all_candidates=np.flatnonzero((curvature[1:-1]>=curvature[:-2])&(curvature[1:-1]>curvature[2:]))+1
    candidates=all_candidates[(z[all_candidates]>=neighbor_region[0])&(z[all_candidates]<=neighbor_region[1])&np.isfinite(curvature[all_candidates])]
    local_peaks=[{'Z':float(z[i]),'curvature':float(curvature[i])} for i in sorted(candidates,key=lambda i:-curvature[i])]
    expanded_candidates=all_candidates[(z[all_candidates]>=expanded_region[0])&(z[all_candidates]<=expanded_region[1])&np.isfinite(curvature[all_candidates])]
    expanded_peaks=[{'Z':float(z[i]),'curvature':float(curvature[i]),'sample_index':int(i)} for i in sorted(expanded_candidates,key=lambda i:-curvature[i])]
    return {'Z':z,'Y':y,'fitted_Y':fit,'dY_dZ':d1,'d2Y_dZ2':d2,
        'curvature':curvature,'signed_curvature':signed,
        'peak':peak(peak_region),'neighbor_peak':peak(neighbor_region),
        'neighbor_local_peaks':local_peaks,'expanded_region_peak':peak(expanded_region),
        'expanded_region_local_peaks':expanded_peaks,'method':{'step':step,'half_window':half_window,'degree':3,
        'fit':'uniform ordinary least squares; strict abs(Z-Z0)<0.010','samples_per_full_window':2*n+1,
        'curve':'actual triangle-plane intersection','peak_region':list(peak_region),
        'neighbor_region':list(neighbor_region),'expanded_protection_boundary_region':list(expanded_region),
        'displacement_C2_used_for_acceptance':False}}


def compare_curves(source, target):
    """Small JSON diagnostic suitable for sculpt iterations."""
    assert np.allclose(source['Z'],target['Z'],rtol=0,atol=1e-10)
    a,b=source['peak'],target['peak']
    na,nb=source['neighbor_peak'],target['neighbor_peak']
    z=source['Z'];delta=target['curvature']-source['curvature']
    bounds=source['method']['expanded_protection_boundary_region']
    valid=np.flatnonzero((z>=bounds[0])&(z<=bounds[1])&np.isfinite(delta))
    worst=valid[np.argmax(delta[valid])]
    expanded=[dict(p,source_curvature_same_Z=float(source['curvature'][p['sample_index']]),
        curvature_increase_same_Z=float(delta[p['sample_index']])) for p in target['expanded_region_local_peaks']]
    return {'source_peak':a,'target_peak':b,'peak_reduction_fraction':1-b['curvature']/a['curvature'],
        'peak_position_shift_world_units':b['Z']-a['Z'],
        'source_neighbor_peak':na,'target_neighbor_peak':nb,
        'neighbor_peak_reduction_fraction':1-nb['curvature']/na['curvature'],
        'source_neighbor_local_peaks':source['neighbor_local_peaks'],
        'target_neighbor_local_peaks':target['neighbor_local_peaks'],
        'expanded_protection_boundary_region':bounds,
        'source_expanded_region_peak':source['expanded_region_peak'],
        'target_expanded_region_peak':target['expanded_region_peak'],
        'source_expanded_region_local_peaks':source['expanded_region_local_peaks'],
        'target_expanded_region_local_peaks':expanded,
        'maximum_curvature_increase_same_Z':{'Z':float(z[worst]),'delta_curvature':float(delta[worst]),
            'source_curvature':float(source['curvature'][worst]),'target_curvature':float(target['curvature'][worst])},
        'maximum_curve_displacement_Y':float(np.nanmax(np.abs(target['Y']-source['Y']))),
        'diagnostic_target_20_percent':bool(b['curvature']<=a['curvature']*.8+1e-7),
        'not_visual_acceptance':True}


def band_wave_diagnostic(z,y,low=.0117244812742,high=.0468979250968):
    """Fixed-world wavelength FFT diagnostic, with cubic low trend removed.

    This diagnostic does not prove appearance. The same evenly sampled section
    and Hann window must be used for N2/R1. Endpoint-sensitive data is recorded.
    """
    z,y=np.asarray(z,float),np.asarray(y,float)
    valid=np.isfinite(y);z,y=z[valid],y[valid]
    t=(z-z.mean())/(z[-1]-z[0])
    design=np.column_stack((np.ones_like(t),t,t*t,t*t*t))
    trend=design@np.linalg.lstsq(design,y,rcond=None)[0]
    residual=y-trend;window=np.hanning(len(y));signal=residual*window
    freq=np.fft.rfftfreq(len(y),float(np.median(np.diff(z))))
    spectrum=np.fft.rfft(signal);take=(freq>=1/high)&(freq<=1/low)
    energy=float(np.sum(np.abs(spectrum[take])**2))
    return {'wavelength_range_world_units':[low,high],'Z_range':[float(z[0]),float(z[-1])],
        'sample_count':len(z),'hann_window':True,'removed_low_trend_degree':3,
        'band_spectral_energy':energy,'residual_RMS_world_units':float(np.sqrt(np.mean(residual**2))),
        'diagnostic_not_visual_acceptance':True}
