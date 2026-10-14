"""Read-only normal comparison of saved E2 trials, with actual triangle IDs."""
import argparse,json,sys
from pathlib import Path
import bpy
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent))
from head_neck_validation_utils import raw_record
from e2_eye_sculpt import BODY

ap=argparse.ArgumentParser()
ap.add_argument('--source',required=True,type=Path)
ap.add_argument('--candidates',nargs='+',required=True,type=Path)
ap.add_argument('--output',required=True,type=Path)
args=ap.parse_args(sys.argv[sys.argv.index('--')+1:])
def read(path):
    bpy.ops.wm.open_mainfile(filepath=str(path.resolve()),load_ui=False)
    return raw_record(bpy.data.objects[BODY])
old=read(args.source);out=[]
for path in args.candidates:
    new=read(path);dots=np.sum(old['triangle_normals']*new['triangle_normals'],axis=1)
    ids=np.flatnonzero(dots<=0)
    entries=[]
    for index in ids:
        vertices=old['triangles'][index]
        entries.append({'id':int(index),'dot':float(dots[index]),'vertex_ids':vertices.tolist(),
            'source':old['points'][vertices].tolist(),'candidate':new['points'][vertices].tolist(),
            'delta':(new['points'][vertices]-old['points'][vertices]).tolist(),
            'source_normal':old['triangle_normals'][index].tolist(),
            'candidate_normal':new['triangle_normals'][index].tolist()})
    record={'candidate':str(path),'flipped_count':len(ids),'minimum_dot':float(dots.min()),
            'flipped_triangles':entries}
    out.append(record)
    print('E2_NORMAL_DIAGNOSIS',str(path),len(ids),float(dots.min()),json.dumps(entries[:4]),flush=True)
args.output.write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8')
