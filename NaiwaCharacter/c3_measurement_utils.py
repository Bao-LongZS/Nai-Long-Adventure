"""Fixed C3 measurements from actual saved world-space triangle meshes.

No construction parameters, historical curves, render thresholds or mutable
candidate-specific fitting ranges are used here.
"""
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

def arc_fit(points,step,degree,half_window):
    points=np.asarray(points,float)
    source_s=np.r_[0.,np.cumsum(np.linalg.norm(np.diff(points,axis=0),axis=1))]
    keep=np.r_[True,np.diff(source_s)>1e-12];points=points[keep];source_s=source_s[keep]
    s=np.arange(0.,source_s[-1]+step*.1,step)
    p=np.column_stack([np.interp(s,source_s,points[:,j]) for j in range(3)])
    fitted=np.zeros_like(p);d1=np.zeros_like(p);d2=np.zeros_like(p)
    for i,ss in enumerate(s):
        ids=np.flatnonzero(np.abs(s-ss)<=half_window+1e-12)
        if len(ids)<degree+2:ids=np.argsort(np.abs(s-ss))[:degree+2]
        tt=s[ids]-ss
        for j in range(3):
            co=np.polynomial.polynomial.polyfit(tt,p[ids,j],degree)
            fitted[i,j]=co[0];d1[i,j]=co[1];d2[i,j]=2*co[2]
    # Curve oriented from chest upward. Angle in the world Y/Z plane.
    angle=np.unwrap(np.arctan2(d1[:,1],d1[:,2]))
    speed=np.linalg.norm(d1[:,1:3],axis=1)
    curvature=(d1[:,2]*d2[:,1]-d1[:,1]*d2[:,2])/np.maximum(speed**3,1e-20)
    return {'s':s,'points':p,'fitted':fitted,'angle':angle,'curvature':curvature}

def angle80(s,angle):
    increments=np.abs(np.diff(angle));total=float(increments.sum())
    if len(s)<8 or total<np.deg2rad(3):return {'applicable':False,'reason':'less than three degrees absolute turn or too few samples','total_absolute_turn_radians':total}
    cumulative=np.r_[0.,np.cumsum(increments)];target=.8*total;best=None
    for start in range(len(s)):
        wanted=cumulative[start]+target
        if wanted>total+1e-14:break
        end=int(np.searchsorted(cumulative,wanted,side='left'))
        if end>=len(s):continue
        if end>start and cumulative[end]>cumulative[end-1]:
            end_s=float(s[end-1]+(s[end]-s[end-1])*(wanted-cumulative[end-1])/(cumulative[end]-cumulative[end-1]))
        else:end_s=float(s[end])
        width=end_s-float(s[start])
        if best is None or width<best[0]:best=(width,float(s[start]),end_s)
    # The minimum in a piecewise-linear cumulative-turn curve has a breakpoint
    # at either end. Check right-end breakpoints too, with fractional starts.
    for end in range(len(s)):
        wanted=cumulative[end]-target
        if wanted<0:continue
        start=int(np.searchsorted(cumulative,wanted,side='right')-1)
        if start<0 or start>end:continue
        if start<len(s)-1 and cumulative[start+1]>cumulative[start] and wanted>cumulative[start]:
            start_s=float(s[start]+(s[start+1]-s[start])*(wanted-cumulative[start])/(cumulative[start+1]-cumulative[start]))
        else:start_s=float(s[start])
        width=float(s[end])-start_s
        if best is None or width<best[0]:best=(width,start_s,float(s[end]))
    return {'applicable':True,'width':best[0],'start_arc':best[1],'end_arc':best[2],'total_absolute_turn_radians':total,'covered_fraction':.8,'definition':'shortest continuous arc containing 80 percent cumulative absolute unwrapped tangent turn; final interval fraction interpolated'}

def measure_sections(points,triangles,protocol):
    config=protocol['sections'];points=np.asarray(points,float);triangles=np.asarray(triangles,int)
    tree=BVHTree.FromPolygons(points.tolist(),triangles.tolist(),all_triangles=True)
    result={'protocol':'new-fixed-C3-v1','historical_protocol_comparison':False,'longitudinal':{}}
    for x in config['longitudinal_X']:
        zrange=config.get('longitudinal_Z_ranges',{}).get(str(float(x)),config['Z_range'])
        zs=np.arange(zrange[0],zrange[1]+config['Z_sampling_step']*.1,config['Z_sampling_step'])
        p=[]
        for z in zs:
            hit,_,_,_=tree.ray_cast(Vector((float(x),-3.,float(z))),Vector((0,1,0)),6.)
            if hit is None:raise ValueError(f'Fixed longitudinal ray missing at X{x} Z{z}')
            p.append(tuple(hit))
        fitted=arc_fit(p,config['resample_arc_step'],config['polynomial_degree'],config['fit_half_window_world'])
        lo,hi=config['mouth_underturn_Z_range'];use=(fitted['points'][:,2]>=lo)&(fitted['points'][:,2]<=hi)
        ss=fitted['s'][use];aa=fitted['angle'][use];cc=fitted['curvature'][use]
        turn=angle80(ss,aa);peak=int(np.argmax(np.abs(cc)))
        result['longitudinal'][str(float(x))]={'plane_X':float(x),'mouth_underturn_Z_range':[lo,hi],'angle80':turn,'maximum_absolute_curvature':float(abs(cc[peak])),'curvature_peak_world':fitted['points'][use][peak].tolist(),'mouth_underturn_arc_length':float(ss[-1]-ss[0]),'samples':{'arc':fitted['s'].tolist(),'actual_points_world':fitted['points'].tolist(),'fitted_points_world':fitted['fitted'].tolist(),'unwrapped_tangent_angle_radians':fitted['angle'].tolist(),'signed_curvature_per_world_unit':fitted['curvature'].tolist()}}
        if 'natural_front_neck_Z_range' in config:
            nlo,nhi=config['natural_front_neck_Z_range'];nu=(fitted['points'][:,2]>=nlo)&(fitted['points'][:,2]<=nhi)
            ns=fitted['s'][nu];na=fitted['angle'][nu];nc=fitted['curvature'][nu];npnts=fitted['points'][nu]
            chord_y=np.interp(npnts[:,2],[npnts[0,2],npnts[-1,2]],[npnts[0,1],npnts[-1,1]])
            result['longitudinal'][str(float(x))]['natural_front_neck']={'Z_range':[nlo,nhi],'angle80':angle80(ns,na),'minimum_signed_curvature':float(nc.min()),'maximum_signed_curvature':float(nc.max()),'maximum_world_Y_recess_from_endpoint_chord':float((npnts[:,1]-chord_y).max()),'arc_length':float(ns[-1]-ns[0])}
    return result
