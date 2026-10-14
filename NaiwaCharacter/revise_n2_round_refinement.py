"""Build R1 from a fixed, hash-pinned N2; never accumulate retries."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import bpy
import numpy as np

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from n2_round_sculpt import sculpt
SOURCE=ROOT/'versions/20261006_head_neck_refinement/candidate_N2/Naiwa_Character.blend'
SOURCE_SHA='9397186f3ae13e93e05cc0d492c12ceb68b743ab4d2a9a0d650426a47a66b0cb'
OUT=ROOT/'versions/20261007_N2_round_refinement'
ap=argparse.ArgumentParser()
ap.add_argument('--output',type=Path,default=OUT/'candidate_R1')
ap.add_argument('--sigma-z',type=float,default=.013)
ap.add_argument('--sigma-arc',type=float,default=.014)
ap.add_argument('--front-inset',type=float,default=0.)
ap.add_argument('--rear-fill',type=float,default=.0045)
ap.add_argument('--iterations',type=int,default=1600)
ap.add_argument('--rho',type=float,default=60.)
args=ap.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==SOURCE_SHA
dest=args.output.resolve()
assert dest!=SOURCE.parent.resolve()
dest.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(SOURCE),load_ui=False)
rig=bpy.data.objects['Naiwa_Rig']
rig.animation_data.action=bpy.data.actions['Naiwa_Idle']
rig.animation_data.use_nla=False
rig.data.pose_position='POSE'
bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
options={'sigma_z':args.sigma_z,'sigma_arc':args.sigma_arc,'front_inset':args.front_inset,
         'rear_fill':args.rear_fill,'iterations':args.iterations,'rho':args.rho}
report,field=sculpt(options)
report.update(source=str(SOURCE),source_sha256=SOURCE_SHA,options=options,candidate='R1')
bpy.context.preferences.filepaths.save_version=0
path=dest/'Naiwa_Character.blend'
bpy.ops.wm.save_as_mainfile(filepath=str(path),compress=True)
report['saved_blend_sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
(dest/'build_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
np.savez_compressed(dest/'deformation_field.npz',**field)
(dest/'parameters.json').write_text(json.dumps(options,indent=2)+'\n',encoding='utf-8')
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==SOURCE_SHA
print('N2_ROUND_REFINEMENT_BUILD_OK',json.dumps(report['surfaces']),flush=True)
