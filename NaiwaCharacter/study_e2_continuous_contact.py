"""Read-only contact study; actual mesh triangles, no saved model."""
import argparse,hashlib,json,sys
from pathlib import Path
import bpy
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent))
from e2_eye_sculpt import BODY,make_parameters,contact_target,D0
from eye_projection_measure import visible_metrics
from geometry_diagnostic_utils import array,mesh_record
from inspect_e2_candidates import ContactCache

ap=argparse.ArgumentParser()
ap.add_argument('--source',required=True,type=Path)
ap.add_argument('--output',required=True,type=Path)
ap.add_argument('--harmonic',action='store_true')
ap.add_argument('--optimized',action='store_true')
args=ap.parse_args(sys.argv[sys.argv.index('--')+1:])
source=args.source.resolve();sha=hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(source),load_ui=False)
rig=bpy.data.objects['Naiwa_Rig'];rig.animation_data.action=bpy.data.actions['Naiwa_Idle']
rig.animation_data.use_nla=False;rig.data.pose_position='POSE'
bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
body=bpy.data.objects[BODY];dg=bpy.context.evaluated_depsgraph_get()
record=mesh_record(body,dg);points=record['points'];tri=record['triangles']
normals=array(body.data.vertices,'normal',3,np.float32)
metrics={s:visible_metrics(bpy.data.objects[s+' concentric iris and pupil'].evaluated_get(dg),
                          body.evaluated_get(dg),step=.0006) for s in ('L','R')}
centers={s:metrics[s]['aperture']['center'] for s in metrics}
caches={s:ContactCache(record,centers[s]) for s in centers}
records=[]
options=([('optimized_harmonic' if args.optimized else 'harmonic_rim',.8,.5 if args.optimized else 1.,scale,width) for scale in (.98,1.,1.02)
           for width in (.055,.065,.08)] if args.harmonic else
         [(method,slope,.5,1.,.043) for method in ('continuous_gap','surface_gap')
           for slope in (.35,.5,.65,.8)])
for method,slope,shift,scale,width in options:
        revised=points.copy();eyes={}
        for side in ('L','R'):
            par=make_parameters(record,side,centers[side],shift,scale,1.10,slope,width,method)
            radial=np.linalg.norm(points[:,(0,2)]-centers[side],axis=1)
            ids=np.flatnonzero((radial<D0/2.+width+.01)&(points[:,1]<-.18)&(normals[:,1]<-.02))
            revised[ids,1]+=contact_target(points[ids],par)[0]-points[ids,1]
            eyes[side]=caches[side].with_contact(par).shape(par)
            eyes[side]['installation_fit']=par.get('installation_fit')
        tp=revised[tri];n=np.cross(tp[:,1]-tp[:,0],tp[:,2]-tp[:,0])
        n/=np.maximum(np.linalg.norm(n,axis=1)[:,None],1e-20)
        dots=np.einsum('ij,ij->i',n,record['triangle_normals'])
        row={'method':method,'slope':slope,'axis_ratio':1.10,'shift_fraction':shift,
             'radius_scale':scale,'transition':width,
             'min_normal_dot':float(dots.min()),'flipped_triangle_count':int(np.count_nonzero(dots<=0)),
             'max_skin_delta_Y':float(np.abs(revised[:,1]-points[:,1]).max()),'eyes':eyes}
        records.append(row);print('E2_CONTACT_STUDY',json.dumps(row),flush=True)
assert hashlib.sha256(source.read_bytes()).hexdigest()==sha
args.output.parent.mkdir(parents=True,exist_ok=True)
args.output.write_text(json.dumps({'source':str(source),'source_sha256':sha,
    'read_only':True,'geometry':'actual source triangle connectivity with proposed vertex coordinates',
    'records':records},indent=2)+'\n',encoding='utf-8')
