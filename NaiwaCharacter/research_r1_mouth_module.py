"""Read-only offline check of the final-mouth production helper on trial03."""
import json
import math
from pathlib import Path
import numpy as np
from n2_round_mouth_curvature import refine_mouth_curvature

OUT=Path(__file__).absolute().parent/'versions/20261007_N2_round_refinement'
with np.load(OUT/'trials/03_mouth_release/deformation_field.npz') as f:
    p=f['points_world'];n=f['source_normals'];d=f['delta'][:,:2]
    keys=('front','rear','side','region','mouth_guard','shoulder_guard','eye_guard','total_limit','x_limit','normal_limit')
    masks={k:f[k].copy() for k in keys}
    old=masks['mouth_guard'];lo=np.zeros(len(old));hi=np.ones(len(old))
    # Invert the trial03 Beta(3,6) guard, then use the root's Beta(3,10).
    for _ in range(60):
        t=(lo+hi)/2
        g=sum(math.comb(8,j)*t**j*(1-t)**(8-j) for j in range(3,9))
        lo=np.where(g<old,t,lo);hi=np.where(g>=old,t,hi)
    t=(lo+hi)/2
    new=sum(math.comb(12,j)*t**j*(1-t)**(12-j) for j in range(3,13))
    new=np.where(old==0,0,np.where(old==1,1,new))
    ratio=np.where(old>0,new/np.maximum(old,1e-30),0)
    for k in ('front','rear','side','region','total_limit','x_limit','normal_limit'):
        masks[k]*=ratio
    masks['mouth_guard']=new
with np.load(OUT/'raw_mesh_measurements.npz') as f:
    triangles=f['triangles']
final,report=refine_mouth_curvature(p,n,triangles,d,masks)
(OUT/'research_mouth_module.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
np.savez_compressed(OUT/'research_mouth_module.npz',final_delta_xy=final,source_global_delta_xy=d)
print(json.dumps(report,ensure_ascii=False,indent=2))
