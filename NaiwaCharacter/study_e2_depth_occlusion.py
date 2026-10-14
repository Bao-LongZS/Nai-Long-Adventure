"""Read-only E2 burial study from native actual body and sphere triangles."""
import argparse,hashlib,json,sys
from pathlib import Path
import bpy
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent))
from e2_eye_sculpt import BODY,make_parameters,modify_skin,apply_eye,gaze_axis,calibrate_pupil
from eye_projection_measure import visible_metrics
from geometry_diagnostic_utils import array,mesh_record
from inspect_e2_candidates import ContactCache
from spherical_reference_projection import eye_pixels

ap=argparse.ArgumentParser()
ap.add_argument('--source',required=True,type=Path)
ap.add_argument('--output',required=True,type=Path)
args=ap.parse_args(sys.argv[sys.argv.index('--')+1:])
source=args.source.resolve();sha=hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(source),load_ui=False)
rig=bpy.data.objects['Naiwa_Rig'];rig.animation_data.action=bpy.data.actions['Naiwa_Idle']
rig.animation_data.use_nla=False;rig.data.pose_position='POSE'
bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
body=bpy.data.objects[BODY];dg=bpy.context.evaluated_depsgraph_get()
source_record=mesh_record(body,dg)
source_points=array(body.data.vertices,'co',3,np.float32).copy()
source_metrics={s:visible_metrics(bpy.data.objects[s+' concentric iris and pupil'].evaluated_get(dg),
                          body.evaluated_get(dg),step=.0006) for s in ('L','R')}
centers={s:source_metrics[s]['aperture']['center'] for s in source_metrics}
# Fit all configurations before modifying source eyes, so repeated iterations
# cannot compound the installation or radius relative to a previous trial.
parameters={depth:{s:make_parameters(source_record,s,centers[s],.5,1.02,1.10,.8,.065,
                                     'optimized_harmonic',depth) for s in ('L','R')}
            for depth in (0.,.004,.008,.012)}
records=[]
for depth,pars in parameters.items():
    body.data.vertices.foreach_set('co',source_points.ravel());body.data.update()
    skin=modify_skin(body,pars);skin.pop('original_points');skin.pop('actual_delta')
    bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get()
    body_record=mesh_record(body,dg)
    dots=np.einsum('ij,ij->i',body_record['triangle_normals'],source_record['triangle_normals'])
    eyes={};front={}
    for side,par in pars.items():
        gaze=gaze_axis(side,20.,0.)
        cache=ContactCache(body_record,centers[side])
        cap=calibrate_pupil(cache,par,gaze)
        eye=apply_eye(par,gaze,cap)
        bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get()
        front[side]=visible_metrics(eye.evaluated_get(dg),body.evaluated_get(dg),step=.0006)
        eyes[side]=mesh_record(eye,dg,eye=True)
    views={}
    for yaw in (0.,45.,50.,90.):
        pose={'type':'ORTHO','yaw':yaw,'pitch':-2.5 if yaw==50. else 0.,
              'target':[0.,-.12,2.425 if yaw==50. else 2.37],
              'distance':7.,'scale':.86 if yaw==50. else .85,'lens':50.}
        measured={s:eye_pixels(eyes[s],body_record,pose,900,step=1.5) for s in eyes}
        diameters=[m['aperture'].get('D_pixels',0.) for m in measured.values()]
        views[str(yaw)]={'pose':pose,'eyes':measured,
            'far_near_visible_D_ratio':min(diameters)/max(diameters)}
    result={'depth_offset_Y':depth,'skin':skin,'minimum_source_target_normal_dot':float(dots.min()),
            'flipped_triangle_count':int(np.count_nonzero(dots<=0)),
            'actual_front_metrics':front,'multi_view_actual_triangle_measurements':views,
            'eyes':pars}
    records.append(result)
    print('E2_DEPTH_NATIVE_STUDY',json.dumps({'depth':depth,'normal_min':float(dots.min()),
        'flip_count':int(np.count_nonzero(dots<=0)),'max_skin_deltaY':skin['maximum_abs_Y'],
        'front':{s:{'axis_ratio':m['aperture']['axis_ratio'],'center':m['aperture']['center'],
                   'area':m['aperture']['area'],'holes':m['aperture_topology']['significant_enclosed_skin_holes']}
                  for s,m in front.items()},
        'far_near_ratios':{v:m['far_near_visible_D_ratio'] for v,m in views.items()}}),flush=True)
assert hashlib.sha256(source.read_bytes()).hexdigest()==sha
args.output.parent.mkdir(parents=True,exist_ok=True)
args.output.write_text(json.dumps({'source':str(source),'source_sha256':sha,'read_only':True,
    'model_saved':False,'method':'actual native body and full sphere triangles rebuilt from a frozen H2 source',
    'fixed_cameras':True,'source_front_metrics':source_metrics,'records':records},indent=2)+'\n',encoding='utf-8')
