import bpy, sys, json, math
import numpy as np
from pathlib import Path
from mathutils import Vector
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT))
from longitudinal_surface_sculpt import coordinates
bpy.ops.wm.open_mainfile(filepath=str(OUT/'iterations/before_lip_eye_refinement/Naiwa_Character.blend'))
body=bpy.data.objects['Naiwa unified golden body']
def visible_metrics(eye,body,step=.0008):
    xyz=coordinates(eye)
    xs=np.arange(xyz[:,0].min(),xyz[:,0].max()+step,step)
    zs=np.arange(xyz[:,2].min(),xyz[:,2].max()+step,step)
    points=[]; green=[]; black=[]
    for x in xs:
        for z in zs:
            origin=Vector((float(x),-2,float(z)))
            hit,p,_,idx=eye.ray_cast(origin,Vector((0,1,0)))
            if not hit: continue
            bh,bp,_,_=body.ray_cast(origin,Vector((0,1,0)))
            if bh and p.y>=bp.y-.00002: continue
            points.append((x,z))
            color=eye.data.color_attributes['EyeColor'].data[eye.data.polygons[idx].loop_start].color
            (black if color[1]<.04 else green).append((x,z))
    def stats(rows):
        q=np.array(rows); cov=np.cov(q.T,bias=True)
        vals,vec=np.linalg.eigh(cov)
        axis=vec[:,1]; angle=math.degrees(math.atan2(axis[1],axis[0]))
        if angle>90: angle-=180
        if angle<-90: angle+=180
        return {'area':len(q)*step*step,'center':q.mean(axis=0).tolist(),
            'bounds':[q.min(axis=0).tolist(),q.max(axis=0).tolist()],
            'axis_ratio':float(math.sqrt(vals[1]/vals[0])),'major_axis_degrees':angle,
            'width_height_ratio':float(np.ptp(q[:,0])/np.ptp(q[:,1]))}
    return {'aperture':stats(points),'green':stats(green),'black':stats(black)}
report={}
for side in ('L','R'):
    e=bpy.data.objects[side+' concentric iris and pupil']
    report[side]=visible_metrics(e,body)
    report[side]['properties']={k:list(e[k]) if k in ('frontal_pupil_center','head_reduction_anchor') else str(e[k]) for k in e.keys()}
(OUT/'iterations/before_lip_eye_refinement/eye_projection_metrics.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('EYE_BASELINE_MEASURED',json.dumps(report))
