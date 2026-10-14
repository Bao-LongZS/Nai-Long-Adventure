"""Read the saved head silhouettes and transition derivatives without editing."""
import bpy, sys, json, hashlib, math
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from eye_projection_measure import visible_metrics
from longitudinal_surface_sculpt import coordinates

BASE=ROOT/'versions/20261005_head_reshape/Naiwa_Character.blend'
OUT=ROOT/'versions/20261005_head_side_continuity'
OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(BASE))
body=bpy.data.objects['Naiwa unified golden body']
diameters=[]; centers=[]
for side in ('L','R'):
    eye=bpy.data.objects[side+' concentric iris and pupil']
    metric=visible_metrics(eye,body,step=.0003)
    diameters.append(math.sqrt(4*metric['aperture']['area']/math.pi))
    centers.append(list(eye['frontal_aperture_center']))
D=float(np.mean(diameters)); eye_z=float(np.mean([c[1] for c in centers]))
mouth=coordinates(bpy.data.objects['Short closed oral seam'])
central=mouth[np.abs(mouth[:,0])<1e-7]
mouth_z=float((central[:,2].max()+central[:,2].min())/2)
lo=mouth_z-.8*D; hi=eye_z-.3*D
points=coordinates(body).astype(float)
edges=np.empty(len(body.data.edges)*2,dtype=np.int32)
body.data.edges.foreach_get('vertices',edges)
edges=edges.reshape(-1,2)
a,b=points[edges[:,0]],points[edges[:,1]]
selected=(np.minimum(a[:,2],b[:,2])<=hi+.035)&(np.maximum(a[:,2],b[:,2])>=lo-.035)
a,b=a[selected],b[selected]
samples=[]
zs=np.linspace(lo-.025,hi+.025,301)
for z in zs:
    crossed=((a[:,2]<=z)&(b[:,2]>=z))|((b[:,2]<=z)&(a[:,2]>=z))
    crossed&=np.abs(b[:,2]-a[:,2])>1e-12
    aa,bb=a[crossed],b[crossed]
    t=(z-aa[:,2])/(bb[:,2]-aa[:,2])
    row=aa+(bb-aa)*t[:,None]
    projections={}
    for angle in (0,45,90):
        u=row[:,0]*math.cos(math.radians(angle))+row[:,1]*math.sin(math.radians(angle))
        projections[str(angle)]=[float(u.min()),float(u.max())]
    samples.append({'z':float(z),'projections':projections})
record={'source_blend':str(BASE),'source_sha256':hashlib.sha256(BASE.read_bytes()).hexdigest(),
    'D':D,'eye_diameters':diameters,'eye_centers':centers,'eye_z':eye_z,'mouth_z':mouth_z,
    'correction_z_low':lo,'correction_z_high':hi,'normal_displacement_limit':.12*D,
    'crown_z':float(points[:,2].max()),'samples':samples}
(OUT/'baseline_dimensions.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
width=np.asarray([r['projections']['0'][1] for r in samples])
scaled=(zs-eye_z)/D
ids=(zs>=lo)&(zs<=hi)
for target in [hi,eye_z-.4*D,eye_z-.6*D,eye_z-.8*D,mouth_z,lo]:
    derivative=np.gradient(width,zs)
    curvature=np.gradient(derivative,zs)
    print('PROFILE',float(target),'halfwidth',float(np.interp(target,zs,width)),
          'slope',float(np.interp(target,zs,derivative)), 'second_derivative',float(np.interp(target,zs,curvature)))
print('HEAD_SIDE_CONTINUITY_BASELINE_OK',D,lo,hi,.12*D)
