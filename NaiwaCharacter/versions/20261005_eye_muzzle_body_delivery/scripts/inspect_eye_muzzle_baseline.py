import bpy,sys,json
import numpy as np
from pathlib import Path
from mathutils import Vector
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT))
from longitudinal_surface_sculpt import coordinates
from eye_projection_measure import visible_metrics
BASE=OUT/'iterations/before_eye_muzzle_body_revision'
bpy.ops.wm.open_mainfile(filepath=str(BASE/'Naiwa_Character.blend'))
body=bpy.data.objects['Naiwa unified golden body']
xyz=coordinates(body)
eyes={}
for s in ('L','R'):
    o=bpy.data.objects[s+' concentric iris and pupil']
    m=visible_metrics(o,body,step=.0004)
    eyes[s]={'properties':{k:list(o[k]) if hasattr(o[k],'to_list') else o[k] for k in ('frontal_aperture_center','frontal_eye_axes','frontal_pupil_radius','frontal_pupil_center')},'visible':m,'vertices':len(o.data.vertices)}
D=float(np.mean([2*np.sqrt(e['visible']['aperture']['area']/np.pi) for e in eyes.values()]))
mouth=coordinates(bpy.data.objects['Short closed oral seam'])
rows=mouth.reshape((-1,2,3))
M=float(rows[len(rows)//2,:,2].mean())
report={'height':float(np.ptp(xyz[:,2])),'sole_z':float(xyz[:,2].min()),'crown_z':float(xyz[:,2].max()),
    'D':D,'D_definition':'Equivalent circular diameter enclosed by the green outer boundary; pupil size does not enter the diameter measurement.',
    'axes':{'Z':[0,0,1],'F':[0,-1,0]},'eyes':eyes,'mouth_center':[float(mouth[:,0].mean()),float(mouth[:,1].mean()),M],
    'eye_center_distance':float(abs(eyes['R']['properties']['frontal_aperture_center'][0]-eyes['L']['properties']['frontal_aperture_center'][0])),
    'requested':{'face_raise':.4*D,'eye_projection':.20*D,'pupil_diameter':.53*D,'mouth_width':1.25*D,'mouth_opening':.06*D,'muzzle_advance':.35*D,'throat_retraction':.15*D,'throat_z':M+.4*D-.9*D,'belly_lower_extension':.025*float(np.ptp(xyz[:,2]))},
    'front_profile':[]}
for z in np.arange(2.20,2.6101,.01):
    hit,p,n,_=body.ray_cast(Vector((0,-2,float(z))),Vector((0,1,0)))
    if hit:report['front_profile'].append([float(z),p.y,n.y,n.z])
(BASE/'baseline_dimensions.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print('EYE_MUZZLE_BASELINE_OK',json.dumps({k:v for k,v in report.items() if k not in ('eyes','front_profile')},ensure_ascii=False))
