"""World-space orthographic/perspective visible-eye measurements, no image scaling."""
import math
import numpy as np
from mathutils import Vector
def axes(yaw,pitch):
    a,b=math.radians(yaw),math.radians(pitch)
    back=np.array([math.sin(a)*math.cos(b),-math.cos(a)*math.cos(b),math.sin(b)])
    right=np.array([math.cos(a),math.sin(a),0.]);up=np.cross(back,right)
    return back,right,up
def project(points,pose,res=600):
    p=np.asarray(points);back,right,up=axes(pose['yaw'],pose['pitch'])
    target=np.asarray(pose['target']);rel=p-target
    if pose['type']=='ORTHO':scale=pose['scale'];x=rel@right/scale;y=rel@up/scale
    else:
        camera=target+pose['distance']*back;rel=p-camera;depth=rel@(-back)
        x=(rel@right)/depth*pose['lens']/36.;y=(rel@up)/depth*pose['lens']/36.
    return np.column_stack(((.5+x)*res,(.5-y)*res))
def stats(rows,step):
    if len(rows)<3:return {'visible':False,'sample_count':len(rows)}
    p=np.asarray(rows,float);eig=np.linalg.eigvalsh(np.cov(p.T,bias=True))
    return {'visible':True,'sample_count':len(rows),'center_pixels':p.mean(0).tolist(),
        'bbox_pixels':[p.min(0).tolist(),p.max(0).tolist()],
        'D_pixels':math.sqrt(4*len(rows)*step*step/math.pi),
        'principal_axis_ratio':math.sqrt(eig[1]/max(eig[0],1e-9))}
def eye_pixels(eye,body,pose,res=600,step=2,gaze=None,pupil_angle=None,C=None,R=None):
    pp=project(eye['points'],pose,res);low=np.floor(pp.min(0)/step)*step-step;high=np.ceil(pp.max(0)/step)*step+step
    back,right,up=axes(pose['yaw'],pose['pitch']);camera=np.asarray(pose['target'])+pose['distance']*back
    outer=[];black=[]
    for x in np.arange(low[0],high[0]+1,step):
        for y in np.arange(low[1],high[1]+1,step):
            if pose['type']=='ORTHO':origin=camera+right*((x/res-.5)*pose['scale'])+up*((.5-y/res)*pose['scale']);direction=-back
            else:
                origin=camera;direction=-back+right*((x/res-.5)*36/pose['lens'])+up*((.5-y/res)*36/pose['lens']);direction/=np.linalg.norm(direction)
            p,n,idx,d=eye['bvh'].ray_cast(Vector(origin),Vector(direction),20)
            if p is None:continue
            skin,sn,si,sd=body['bvh'].ray_cast(Vector(origin),Vector(direction),20)
            if skin is not None and sd<d-5e-7:continue
            outer.append((x,y))
            if gaze is None:is_black=eye['regions'][idx]=='black'
            else:is_black=np.dot((np.asarray(p)-C)/R,gaze)>=math.cos(pupil_angle)
            if is_black:black.append((x,y))
    return {'aperture':stats(outer,step),'pupil':stats(black,step),'sampling_step_pixels':step}
