"""Independently inspect saved N1/N2/C1 against the pinned spherical A."""
import argparse,bpy,hashlib,json,sys
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from saved_delivery_metadata import preserved_links,differences,settings
from geometry_diagnostic_utils import mesh_record
from eye_projection_measure import visible_metrics
from head_neck_validation_utils import (raw_record,fingerprint,fit_sphere,sections,
    horizontal_metrics,shoulder_fixed,protective_masks,surface_integrity,evaluated_points)

SOURCE=ROOT/'versions/20261006_spherical_eyes/candidate_A/Naiwa_Character.blend'
SHA='f251f5a0fc00fedf1df76fcc6286bc40e102406a536cb6c676a93e176cb71ca0'
OUT=ROOT/'versions/20261006_head_neck_refinement'
BODY='Naiwa unified golden body';SHELL='Head and torso silhouette only'
D0=.117244812742
parser=argparse.ArgumentParser();parser.add_argument('--baseline-only',action='store_true')
parser.add_argument('--candidates',nargs='+',choices=['N1','N2','C1'],default=['N1','N2'])
parser.add_argument('--neck-parent',choices=['N1','N2'])
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==SHA
OUT.mkdir(exist_ok=True)

def open_pose(path,action='Naiwa_Idle',frame=1):
    bpy.ops.wm.open_mainfile(filepath=str(path));rig=bpy.data.objects['Naiwa_Rig']
    rig.animation_data.action=bpy.data.actions[action];rig.animation_data.use_nla=False
    rig.data.pose_position='POSE'
    bpy.context.scene.frame_set(frame);bpy.context.view_layer.update()

def saved_meshes():
    return {o.name:{'fingerprint':fingerprint(o),
        'points':np.asarray([tuple(o.matrix_world@v.co) for v in o.data.vertices],float)}
        for o in bpy.data.objects if o.type=='MESH'}

def rig_snapshot():
    rig=bpy.data.objects['Naiwa_Rig']
    return {'matrix':list(map(list,rig.matrix_world)),
        'bones':{b.name:{'matrix':list(map(list,b.matrix_local)),
            'parent':b.parent.name if b.parent else None,'head':list(b.head_local),'tail':list(b.tail_local),
            'settings':settings(b)} for b in rig.data.bones},
        'pose_bones':{b.name:{'constraints':[(c.name,c.type,settings(c)) for c in b.constraints],
            'rotation_mode':b.rotation_mode,'lock_location':list(b.lock_location),
            'lock_rotation':list(b.lock_rotation),'lock_scale':list(b.lock_scale)} for b in rig.pose.bones}}

open_pose(SOURCE);source_links=preserved_links();source_meshes=saved_meshes();source_rig=rig_snapshot()
body=bpy.data.objects[BODY];baseline=raw_record(body);source_sections=sections(baseline)
source_eye_fits={side:fit_sphere(source_meshes[side+' concentric iris and pupil']['points']) for side in ('L','R')}
source_dg=bpy.context.evaluated_depsgraph_get()
source_fixed_idle={name:evaluated_points(bpy.data.objects[name],source_dg)
    for name in source_meshes if name not in (BODY,SHELL)}
source_idle_eye_fits={side:fit_sphere(source_fixed_idle[side+' concentric iris and pupil']) for side in ('L','R')}
source_eye_metrics={side:visible_metrics(bpy.data.objects[side+' concentric iris and pupil'].evaluated_get(source_dg),
    body.evaluated_get(source_dg),step=.0003) for side in ('L','R')}
source_eye_spacing=source_eye_metrics['R']['aperture']['center'][0]-source_eye_metrics['L']['aperture']['center'][0]
mouth=source_meshes['Short closed oral seam']['points'];mouth_bounds=[mouth.min(0),mouth.max(0)]
shoulder_tree,shoulder_seeds,_=shoulder_fixed(body)
source_masks={name:protective_masks(source_meshes[name]['points'],shoulder_tree,source_eye_fits,mouth_bounds) for name in (BODY,SHELL)}
source_integrity,source_crossings=surface_integrity(baseline)
source_shell=raw_record(bpy.data.objects[SHELL])
source_shell_integrity,source_shell_crossings=surface_integrity(source_shell,assert_clean=False)
source_idle=mesh_record(body,bpy.context.evaluated_depsgraph_get())
source_idle_sections=sections(source_idle)
section_document={'source_hashes':{'A':SHA},'models':{'A':source_sections}}
summary={'source':str(SOURCE),'source_sha256':SHA,'source_integrity':source_integrity,
    'source_shell_integrity':source_shell_integrity,'source_Idle_eye_sphere_fits':source_idle_eye_fits,
    'source_eye_metrics':source_eye_metrics,'source_eye_spacing':source_eye_spacing,
    'source_raw_Idle_maximum_vertex_displacement':float(np.linalg.norm(source_idle['points']-baseline['points'],axis=1).max()),
    'source_protection_counts':{name:{mask:int(values.sum()) for mask,values in source_masks[name].items()} for name in source_masks},
    'candidates':{}}

pose_specs=[];source_poses={}
if not args.baseline_only:
    for name in ('Naiwa_Walk','Naiwa_Laugh','Naiwa_Jump'):
        action=bpy.data.actions[name];start,end=action.frame_range
        pose_specs.append((name,int(round(start+(end-start)*.25))))
    for action,frame in pose_specs:
        rig=bpy.data.objects['Naiwa_Rig'];rig.animation_data.action=bpy.data.actions[action]
        bpy.context.scene.frame_set(frame);bpy.context.view_layer.update()
        rec=mesh_record(body,bpy.context.evaluated_depsgraph_get())
        integrity,crossings=surface_integrity(rec)
        selected=(rec['triangle_points'][:,:,2].max(1)>=1.95)&(rec['triangle_points'][:,:,2].min(1)<=2.65)
        source_poses[(action,frame)]={'points':rec['points'].copy(),'integrity':integrity,'crossings':crossings,
            'triangle_normals':rec['triangle_normals'].copy()}
    for candidate in args.candidates:
        path=OUT/('candidate_'+candidate)/'Naiwa_Character.blend'
        assert path.exists(),str(path)
        parent_path=SOURCE
        if candidate=='C1':
            assert args.neck_parent,'C1 verification requires selected --neck-parent N1 or N2'
            parent_path=OUT/('candidate_'+args.neck_parent)/'Naiwa_Character.blend'
            selection_path=OUT/'selection.json'
            assert selection_path.is_file(),'C1 verification requires the hash-pinned selection.json'
            selection=json.loads(selection_path.read_text(encoding='utf-8'))
            assert Path(selection['neck_input']).resolve()==parent_path.resolve(),('C1 parent differs from selection',parent_path)
            assert hashlib.sha256(parent_path.read_bytes()).hexdigest()==selection['neck_sha256'],'Selected neck hash changed'
            build=json.loads((path.parent/'build_report.json').read_text(encoding='utf-8'))
            assert Path(build['source']).resolve()==parent_path.resolve() and build['source_sha256']==selection['neck_sha256'],'C1 build parent differs from pinned selection'
        open_pose(parent_path);parent_meshes=saved_meshes()
        open_pose(path);links=preserved_links();meshes=saved_meshes();rig=rig_snapshot()
        assert not differences(source_links,links),('Saved relationships/materials/modifiers/actions changed',candidate,differences(source_links,links)[:8])
        assert rig==source_rig,('Rest rig/bone transforms changed',candidate)
        assert meshes.keys()==source_meshes.keys()
        report={'candidate':candidate,'passed':True,'source_sha256':SHA,
            'candidate_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
            'stage_parent':str(parent_path),'stage_parent_sha256':hashlib.sha256(parent_path.read_bytes()).hexdigest(),
            'parents_materials_modifiers_rig_and_complete_animation_curves_exact':True,'objects':{}}
        for name,current in meshes.items():
            original=source_meshes[name];p=original['points'];q=current['points'];delta=q-p
            assert current['fingerprint']==original['fingerprint'],('Topology/UV/colors/weights/materials/transform/shading changed',candidate,name)
            if name not in (BODY,SHELL):
                assert np.array_equal(q,p),('Fixed object coordinates changed',candidate,name)
                report['objects'][name]={'coordinates_exact':True,'topology_UV_colors_weights_materials_exact':True}
                continue
            assert np.array_equal(q[:,[0,2]],p[:,[0,2]]),('X/Z changed',candidate,name)
            stage_delta=q-parent_meshes[name]['points'];z=parent_meshes[name]['points'][:,2]
            outside=(z<=2.03)|(z>=2.38) if candidate in ('N1','N2') else (z<=2.38)|(z>=2.635)
            assert np.array_equal(q[outside],parent_meshes[name]['points'][outside]),('Stage action boundary changed',candidate,name)
            protected={}
            for label,mask in source_masks[name].items():
                assert np.array_equal(q[mask],p[mask]),('Protected source domain moved',candidate,name,label,float(np.abs(delta[mask]).max()))
                protected[label]={'vertex_count':int(mask.sum()),'coordinates_exact':True}
            max_displacement=float(np.linalg.norm(stage_delta,axis=1).max())
            if candidate in ('N1','N2'):
                assert stage_delta[:,1].min()>=0,('Neck correction must point toward +Y',candidate,name)
            limit=.080 if candidate=='N2' else (.056 if candidate=='N1' else .020001)
            assert max_displacement<=limit,('Maximum displacement exceeded',candidate,name,max_displacement,limit)
            report['objects'][name]={'X_Z_exact':True,'outside_stage_scope_exact':True,
                'topology_UV_colors_all_weights_materials_exact':True,'protected_domains':protected,
                'changed_vertices_from_A':int(np.count_nonzero(np.linalg.norm(delta,axis=1)>0)),
                'maximum_stage_displacement':max_displacement,'stage_delta_Y_range':[float(stage_delta[:,1].min()),float(stage_delta[:,1].max())]}
        body=bpy.data.objects[BODY];raw=raw_record(body);dg=bpy.context.evaluated_depsgraph_get();idle=mesh_record(body,dg)
        for name,expected in source_fixed_idle.items():
            actual=evaluated_points(bpy.data.objects[name],dg)
            assert np.array_equal(actual,expected),('Fixed evaluated Idle mesh changed',candidate,name)
            report['objects'][name]['Idle_frame1_coordinates_exact']=True
        integrity,crossings=surface_integrity(raw,baseline)
        new_crossings=crossings-source_crossings;assert not new_crossings,('New raw self intersections',candidate,len(new_crossings))
        integrity['new_nonadjacent_self_intersections']=len(new_crossings);report['raw_head_neck_integrity']=integrity
        shell=raw_record(bpy.data.objects[SHELL])
        shell_integrity,shell_crossings=surface_integrity(shell,source_shell,assert_clean=False)
        assert not shell_crossings-source_shell_crossings,('New proxy-shell self intersections',candidate)
        for label in ('duplicate_triangles','duplicate_vertices_at_1e_7','boundary_edges_in_region','nonmanifold_edges_in_region'):
            assert shell_integrity[label]<=source_shell_integrity[label],('New proxy-shell surface defect',candidate,label)
        shell_integrity['new_nonadjacent_self_intersections']=0
        report['proxy_shell_integrity']=shell_integrity
        report['raw_Idle_geometry']={'maximum_vertex_displacement':float(np.linalg.norm(idle['points']-raw['points'],axis=1).max()),
            'topology_exact':bool(np.array_equal(idle['triangles'],raw['triangles']))}
        assert report['raw_Idle_geometry']['maximum_vertex_displacement']<1e-6 and report['raw_Idle_geometry']['topology_exact']
        source_outside=(baseline['points'][:,2]<=2.03)|(baseline['points'][:,2]>=2.635)
        assert np.array_equal(idle['points'][source_outside],source_idle['points'][source_outside])
        report['sections_raw']=sections(raw);report['sections_Idle_frame1']=sections(idle)
        section_document['models'][candidate]=report['sections_raw'];section_document['source_hashes'][candidate]=report['candidate_sha256']
        eyes={}
        for side in ('L','R'):
            eye=bpy.data.objects[side+' concentric iris and pupil'];eye_record=mesh_record(eye,dg)
            fit=fit_sphere(eye_record['points']);assert fit['maximum_relative_radius_residual']<=1e-4
            assert np.allclose(fit['center'],source_idle_eye_fits[side]['center'],rtol=0,atol=1e-10) and abs(fit['radius']-source_idle_eye_fits[side]['radius'])<=1e-10,('Idle sphere changed',candidate,side)
            metrics=visible_metrics(eye.evaluated_get(dg),body.evaluated_get(dg),step=.0003)
            source_d=np.sqrt(4*source_eye_metrics[side]['aperture']['area']/np.pi)
            d=np.sqrt(4*metrics['aperture']['area']/np.pi)
            assert abs(d/source_d-1)<.001,('Frontal visible eye diameter changed',candidate,side,d,source_d)
            assert abs(np.asarray(metrics['aperture']['center'])-np.asarray(source_eye_metrics[side]['aperture']['center'])).max()<.0003
            eyes[side]={'sphere_fit':fit,'raw_coordinates_exact':True,'Idle_coordinates_exact':True,
                'frontal_metrics':metrics,'visible_D':float(d),'visible_D_fraction_change_from_A':float(d/source_d-1)}
        spacing=eyes['R']['frontal_metrics']['aperture']['center'][0]-eyes['L']['frontal_metrics']['aperture']['center'][0]
        assert abs(spacing/source_eye_spacing-1)<.001
        report['eyes']=eyes;report['actual_frontal_spacing']=spacing
        report['actual_frontal_spacing_fraction_change_from_A']=spacing/source_eye_spacing-1
        poses=[]
        for action,frame in pose_specs:
            rig=bpy.data.objects['Naiwa_Rig'];rig.animation_data.action=bpy.data.actions[action]
            bpy.context.scene.frame_set(frame);bpy.context.view_layer.update()
            rec=mesh_record(body,bpy.context.evaluated_depsgraph_get());old=source_poses[(action,frame)]
            shoulder=source_masks[BODY]['shoulder_root']
            assert np.array_equal(rec['points'][shoulder],old['points'][shoulder]),('Posed shoulder root moved',candidate,action,frame)
            pose_integrity,pose_crossings=surface_integrity(rec)
            assert not pose_crossings-old['crossings'],('New posed head/shoulder intersections',candidate,action,frame)
            normal_dots=np.sum(rec['triangle_normals']*old['triangle_normals'],axis=1)
            assert normal_dots.min()>0.,('Posed face orientation flips',candidate,action,frame)
            poses.append({'action':action,'frame':frame,'protected_shoulder_root_coordinates_exact':True,
                'minimum_source_target_triangle_normal_dot':float(normal_dots.min()),
                'new_nonadjacent_self_intersections':0,'integrity':pose_integrity})
        report['turning_pose_shoulder_checks']=poses
        (path.parent/'validation_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        summary['candidates'][candidate]=report
summary['source_unchanged']=hashlib.sha256(SOURCE.read_bytes()).hexdigest()==SHA
assert summary['source_unchanged'],'Pinned A changed during validation'
(OUT/'section_raw.json').write_text(json.dumps(section_document,separators=(',',':')),encoding='utf-8')
(OUT/'head_neck_validation.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
print('HEAD_NECK_BASELINE_CHECK_OK' if args.baseline_only else 'HEAD_NECK_REFINEMENT_VALIDATION_OK')
