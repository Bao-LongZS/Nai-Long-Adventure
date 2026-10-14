"""Search the prescribed camera grid using a stable crown/mouth scale fit."""
import bpy,sys,json,math,hashlib
from pathlib import Path
import numpy as np
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from geometry_diagnostic_utils import mesh_record
from spherical_reference_projection import axes,project,eye_pixels
from render_head_reshape_comparison import prepare_scene
OUT=ROOT/'versions/20261006_spherical_eyes';DEST=OUT/'reference_search';DEST.mkdir(exist_ok=True)
ref=json.loads((ROOT/'diagnostics/20261005_head_geometry/reference_alignment.json').read_text(encoding='utf-8'))
rf=ref['reference_features_pixels'];refD=ref['reference_eye']['equivalent_area_diameter_pixels']
refC=np.array(rf['crown']);refM=np.array(rf['mouth_approx']);refE=np.array(rf['near_eye']);refFar=np.array(rf['far_visible_eye_centroid_approx'])
resolution=600
def measure(body,eyes,pose,mouth):
    pp=project(body['points'][body['points'][:,2]>=2.17],pose,resolution)
    top=pp[:,1].min();crown=pp[pp[:,1]<top+.6].mean(0);M=project([mouth],pose,resolution)[0]
    em={s:eye_pixels(eyes[s],body,pose,resolution,2) for s in ('L','R')}
    v=crown-M;rv=refC-refM
    scale=float(np.dot(v,rv)/np.dot(v,v));translation=(refC+refM)/2-scale*(crown+M)/2
    body_error=(np.linalg.norm(scale*crown+translation-refC)+np.linalg.norm(scale*M+translation-refM))/(2*refD)
    near=em['R']['aperture'];far=em['L']['aperture']
    ne=np.array(near['center_pixels']);ne_error=float(np.linalg.norm(scale*ne+translation-refE)/refD)
    far_error=float(np.linalg.norm(scale*np.array(far['center_pixels'])+translation-refFar)/refD) if far['visible'] else 2.
    return {'eyes':em,'crown_pixels':crown.tolist(),'mouth_pixels':M.tolist(),
        'body_landmark_uniform_scale_to_reference':scale,'body_landmark_translation_to_reference':translation.tolist(),
        'body_C_M_error_over_reference_D':float(body_error),'near_eye_position_error_over_reference_D':ne_error,
        'far_visible_position_error_over_reference_D':far_error,
        'near_D_after_body_fit_pixels':near['D_pixels']*scale,
        'pose_score':float(body_error+ne_error+far_error),
        'fit_method':'Uniform scale + translation from unchanged yellow crown and mouth. No per-candidate eye-size normalization.'}
def setup(scene,pose):
    back,_,_=axes(pose['yaw'],pose['pitch']);target=Vector(pose['target']);cam=scene.camera
    cam.data.type=pose['type'];cam.data.ortho_scale=pose['scale'];cam.data.lens=pose['lens'];cam.data.sensor_width=36.;cam.data.sensor_fit='HORIZONTAL'
    cam.location=target+pose['distance']*Vector(back);cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
    scene.render.resolution_x=scene.render.resolution_y=resolution;bpy.context.view_layer.update()
records=[];selected={}
for candidate in ('A','B'):
    source=OUT/('candidate_'+candidate)/'Naiwa_Character.blend';digest=hashlib.sha256(source.read_bytes()).hexdigest()
    bpy.ops.wm.open_mainfile(filepath=str(source));scene,engine=prepare_scene();dg=bpy.context.evaluated_depsgraph_get()
    body=mesh_record(bpy.data.objects['Naiwa unified golden body'],dg)
    eyes={s:mesh_record(bpy.data.objects[s+' concentric iris and pupil'],dg,eye=True) for s in ('L','R')}
    seam=bpy.data.objects['Short closed oral seam'];p=np.array([tuple(seam.matrix_world@v.co) for v in seam.data.vertices]);mid=p[np.abs(p[:,0])<1e-7];mouth=mid.mean(0)
    poses=[{'type':'ORTHO','yaw':yaw,'pitch':pitch,'target':[0,-.12,2.425],'distance':7.,'scale':.86,'lens':50.} for yaw in (40,50,60,70,80) for pitch in (-5,0,5)]
    measured=[]
    for pose in poses:
        setup(scene,pose);name=f'{candidate}_ortho_yaw{pose["yaw"]}_pitch{pose["pitch"]}.png'
        scene.render.filepath=str(DEST/name);bpy.ops.render.render(write_still=True)
        row={'candidate':candidate,'filename':name,'pose':pose,**measure(body,eyes,pose,mouth)};measured.append(row);records.append(row)
    best=min(measured,key=lambda r:r['pose_score']);yaw,pitch=best['pose']['yaw'],best['pose']['pitch']
    # Fine orthographic neighbors plus two long-focus perspective tests.
    extra=[{**best['pose'],'yaw':yaw+d,'pitch':pitch+e} for d,e in [(-5,0),(5,0),(0,-2.5),(0,2.5)]]
    extra += [{**best['pose'],'type':'PERSP','lens':lens,'distance':.86*lens/36} for lens in (90.,135.)]
    for i,pose in enumerate(extra):
        setup(scene,pose);name=f'{candidate}_refine_{i}_{pose["type"]}.png';scene.render.filepath=str(DEST/name);bpy.ops.render.render(write_still=True)
        row={'candidate':candidate,'filename':name,'pose':pose,**measure(body,eyes,pose,mouth)};measured.append(row);records.append(row)
    selected[candidate]=min(measured,key=lambda r:r['pose_score'])
    assert hashlib.sha256(source.read_bytes()).hexdigest()==digest
    print('REFERENCE_SEARCH_CANDIDATE_DONE',candidate,selected[candidate]['pose'],selected[candidate]['pose_score'],flush=True)
far_ratio=math.sqrt(11*34)/refD
for row in records:
    near=row['eyes']['R']['aperture'];far=row['eyes']['L']['aperture']
    ratio=far['D_pixels']/near['D_pixels'] if far['visible'] else 0.
    row['far_D_over_near_D']=ratio;row['position_only_score']=row['pose_score']
    row['pose_score']+=1.5*abs(ratio-far_ratio)+.5*abs(math.log(near['principal_axis_ratio']/(57/54)))
for candidate in ('A','B'):selected[candidate]=min([r for r in records if r['candidate']==candidate],key=lambda r:r['pose_score'])
(DEST/'camera_search.json').write_text(json.dumps({'resolution':[resolution,resolution],'view_transform':'Standard','exposure':-.4,
    'reference_source':str(ROOT/'references/20261005/pdf_page03_image03_IM60.jpg'),
    'reference_landmarks':rf,'manual_reference_boundary_uncertainty_px':2,
    'grid_yaw':[40,50,60,70,80],'grid_pitch':[-5,0,5],
    'reference_far_visible_D_over_near_D_approx':far_ratio,
    'pose_score_note':'Grid and fine neighbors selected by position score; final ranking adds 1.5*far-visible-D-ratio error and 0.5*near-eye log-axis-ratio error. Approximate far contour 11x34 px; reference camera unknown.',
    'records':records,'selected_per_candidate':selected},indent=2),encoding='utf-8')
print('SPHERICAL_REFERENCE_SEARCH_OK')
