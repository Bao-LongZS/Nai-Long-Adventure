"""Offline read-only actual-section feasibility study; no Blender mutations."""
import json
from pathlib import Path
import numpy as np
from n2_round_diagnostic_utils import sample_section_branches,curve_diagnostics

OUT=Path(__file__).absolute().parent/'versions/20261007_N2_round_refinement'
raw=json.loads((OUT/'section_raw.json').read_text(encoding='utf-8'))
with np.load(OUT/'candidate_R1/deformation_field.npz') as f:
    p=f['points_world']; guard=f['mouth_guard']; cap=f['total_limit']; d=f['delta']
    zgrid=f['grid_Z'];g=f['grid_source'];r=f['grid_result_xy']
    take=(np.abs(p[:,0])<.004)&(p[:,1]<-.25)&(p[:,2]>=2.2)&(p[:,2]<=2.48)
    pv=p[take];gv=guard[take];cv=cap[take]
    idx=np.argsort(pv[:,2]);pv=pv[idx];gv=gv[idx];cv=cv[idx]
    for z in np.arange(2.35,2.41001,.0025):
        near=np.argmin(np.abs(pv[:,2]-z))
        j=np.argmin(np.abs(zgrid-z))
        print(f'{z:.4f} gridY {g[j,144,1]:+.7f} delta {r[j,144,1]-g[j,144,1]:+.7f} mouth {gv[near]:.5f} cap {cv[near]:.6f}')
curve=sample_section_branches(raw['models']['N2']['midline'])
z=curve['Z'];source=curve['front_Y']
print('SOURCE',curve_diagnostics(z,source)['peak'])
for x in ('N2','R1'):
    c=sample_section_branches(raw['models'][x]['midline'])
    print(x,curve_diagnostics(c['Z'],c['front_Y'])['peak'])
