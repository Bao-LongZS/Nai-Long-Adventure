"""Independently validate hash-pinned R1 -> H2 native geometry/assets.

Numerical/asset integrity is deliberately separate from visual acceptance of
the overall shape. All measurements come from saved meshes, never build deltas.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from saved_delivery_metadata import preserved_links,differences
from geometry_diagnostic_utils import mesh_record
from head_neck_validation_utils import raw_record,fingerprint,fit_sphere,surface_integrity,evaluated_points
from eye_projection_measure import visible_metrics
from validate_n2_round_refinement import open_pose,meshes_snapshot,rig_snapshot,assert_normal_settings
from h2_validation_utils import (BODY,SHELL,D0,vertex_normals,exact_local_coordinates,
    actual_contacts,normal_delta_measurement,extended_sections,compare_section_widths)

SOURCE=ROOT/'versions/20261007_N2_round_refinement/candidate_R1/Naiwa_Character.blend'
SOURCE_SHA='ea12e9af2c564527d0c473cd31b28974339fcceb15b666502a33f0ffa057bc24'
OUT=ROOT/'versions/20261007_H2_E2_overall_refinement'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def displacement_checks(name,old,new,normals,masks,limits):
    p,q=old['points'],new['points']
    delta=q-p
    total=np.linalg.norm(delta,axis=1)
    assert total.max()<=limits['total']+1e-7,('H2 total displacement cap',name,float(total.max()),limits['total'])
    assert np.abs(delta[:,0]).max()<=limits['X']+1e-7,('H2 X displacement cap',name)
    assert np.abs(delta[:,2]).max()<=limits['Z']+2e-7,('H2 Z displacement cap',name)
    protected={}
    for label,mask in masks.items():
        moved=float(total[mask].max()) if mask.any() else 0.
        assert np.array_equal(q[mask],p[mask]),('Fixed real H2 feature moved',name,label,moved)
        protected[label]={'vertex_count':int(mask.sum()),'coordinates_exact':True,'maximum_displacement':moved}
    projection,tangent=normal_delta_measurement(p,normals,delta)
    return {'changed_vertices':int(np.count_nonzero(total)),
        'maximum_total_displacement':float(total.max()),
        'maximum_absolute_X_displacement':float(np.abs(delta[:,0]).max()),
        'maximum_absolute_Z_displacement':float(np.abs(delta[:,2]).max()),
        'delta_X_range':[float(delta[:,0].min()),float(delta[:,0].max())],
        'delta_Y_range':[float(delta[:,1].min()),float(delta[:,1].max())],
        'delta_Z_range':[float(delta[:,2].min()),float(delta[:,2].max())],
        'source_normal_projection_range':[float(projection.min()),float(projection.max())],
        'maximum_tangential_displacement':float(tangent.max()),'protected_regions':protected,
        'topology_UV_colors_all_weights_transforms_exact':True,'limits':limits,
        'old_R1_Z_and_midline_interpolation_locks_removed':True}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate',type=Path,default=OUT/'candidate_H2/Naiwa_Character.blend')
    parser.add_argument('--output',type=Path,default=OUT)
    parser.add_argument('--clear-version',action='store_true',help='Separately saved documented stronger candidate only; total .045 Z .015.')
    parser.add_argument('--mouth-contact',type=float,default=.004)
    parser.add_argument('--eye-contact',type=float,default=.003)
    parser.add_argument('--frozen-below',type=float,default=1.95)
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    assert .003<=args.mouth_contact<=.005
    assert .001<=args.eye_contact<=.005
    assert 1.90<=args.frozen_below<=1.98
    candidate=args.candidate.resolve(strict=True)
    out=args.output.resolve()
    out.mkdir(parents=True,exist_ok=True)
    assert candidate!=SOURCE
    assert sha(SOURCE)==SOURCE_SHA
    limits={'total':.045 if args.clear_version else .035,'Z':.015 if args.clear_version else .010,'X':.008}
    report={'candidate':'H2','passed':False,'source':str(SOURCE),'source_sha256':SOURCE_SHA,
        'candidate_path':str(candidate),'candidate_sha256':sha(candidate),
        'limits':limits,'stronger_clear_version':args.clear_version,'objects':{},
        'visual_overall_shape_passed':None,'visual_acceptance_requires_full_multi_view_review':True}
    build_path=candidate.parent/'build_report.json'
    build=json.loads(build_path.read_text(encoding='utf-8')) if build_path.exists() else {}
    if args.clear_version:
        assert build.get('stronger_version_reason') or build.get('clear_version_reason') or build.get('options',{}).get('clear_version'), 'Stronger candidate must record reason/parameters'
    try:
        open_pose(SOURCE)
        source_links=preserved_links()
        source_meshes=meshes_snapshot()
        source_rig=rig_snapshot()
        source_body=raw_record(bpy.data.objects[BODY])
        source_shell=raw_record(bpy.data.objects[SHELL])
        source_normals={n:vertex_normals(bpy.data.objects[n]) for n in (BODY,SHELL)}
        eye_points={side:source_meshes[side+' concentric iris and pupil']['points'] for side in ('L','R')}
        source_masks={}
        contact_infos={}
        for name in (BODY,SHELL):
            masks,info=actual_contacts(source_meshes[name]['points'],eye_points,
                bpy.data.objects['Short closed oral seam'],args.mouth_contact,args.eye_contact)
            masks['remote_lower_body']=source_meshes[name]['points'][:,2]<args.frozen_below
            # Historical hidden shell is not an eye installation surface.
            if name==SHELL:
                masks.pop('eye_true_sphere_intersection_band')
            source_masks[name]=masks
            contact_infos[name]={k:v for k,v in info.items() if k not in ('mouth_distance','eye_signed_distances')}
        source_integrity,source_crossings=surface_integrity(source_body)
        source_shell_integrity,source_shell_crossings=surface_integrity(source_shell,assert_clean=False)
        dg=bpy.context.evaluated_depsgraph_get()
        source_idle=mesh_record(bpy.data.objects[BODY],dg)
        source_idle_integrity,source_idle_crossings=surface_integrity(source_idle)
        source_fixed_idle={name:evaluated_points(bpy.data.objects[name],dg) for name in source_meshes if name not in (BODY,SHELL)}
        source_eye_fits={s:fit_sphere(source_fixed_idle[s+' concentric iris and pupil']) for s in ('L','R')}
        source_eye_metrics={s:visible_metrics(bpy.data.objects[s+' concentric iris and pupil'].evaluated_get(dg),
            bpy.data.objects[BODY].evaluated_get(dg),step=.0003) for s in ('L','R')}
        source_sections=extended_sections(source_body)
        source_idle_sections=extended_sections(source_idle)
        source_poses={}
        for action_name in ('Naiwa_Walk','Naiwa_Laugh','Naiwa_Jump'):
            action=bpy.data.actions[action_name]
            start,end=action.frame_range
            frame=int(round(start+(end-start)*.25))
            bpy.data.objects['Naiwa_Rig'].animation_data.action=action
            bpy.context.scene.frame_set(frame)
            bpy.context.view_layer.update()
            rec=mesh_record(bpy.data.objects[BODY],bpy.context.evaluated_depsgraph_get())
            integrity,crossings=surface_integrity(rec)
            source_poses[(action_name,frame)]={'record':rec,'integrity':integrity,'crossings':crossings}

        open_pose(candidate)
        meshes=meshes_snapshot()
        dg=bpy.context.evaluated_depsgraph_get()
        body=bpy.data.objects[BODY]
        raw=raw_record(body)
        idle=mesh_record(body,dg)
        # Save independent observations before assertions so rejected trials
        # remain diagnosable; reports/hash binding identify the exact inputs.
        np.savez_compressed(out/'raw_mesh_measurements.npz',points_R1=source_body['points'],
            points_H2=raw['points'],triangles=raw['triangles'],source_normals=source_normals[BODY],
            points_R1_Idle=source_idle['points'],points_H2_Idle=idle['points'])
        new_sections=extended_sections(raw)
        new_idle_sections=extended_sections(idle)
        section_document={'source_hashes':{'R1':SOURCE_SHA,'H2':report['candidate_sha256']},
            'models':{'R1':source_sections,'H2':new_sections},
            'Idle_frame1_models':{'R1':source_idle_sections,'H2':new_idle_sections},
            'coordinate_axes':{'X':'right','Y':'rear','Z':'up'},'D0':D0,
            'actual_triangle_plane_intersections':True,'old_R1_hard_locks_not_inherited':True}
        (out/'section_raw.json').write_text(json.dumps(section_document,separators=(',',':')),encoding='utf-8')
        changed_links=differences(source_links,preserved_links())
        assert not changed_links,('Saved links/materials/modifiers/complete actions changed',changed_links[:12])
        assert rig_snapshot()==source_rig,'Skeleton/shoulder joint/rest matrices/constraints changed'
        assert meshes.keys()==source_meshes.keys(),'Saved mesh inventory changed'
        report['parents_materials_modifiers_rig_complete_animation_curves_exact']=True
        for name,current in meshes.items():
            old=source_meshes[name]
            a,b=old['fingerprint'].copy(),current['fingerprint'].copy()
            a.pop('custom_normals')
            b.pop('custom_normals')
            assert a==b,('Topology/UV/colors/all weights/transform/shading changed',name)
            normal_info=assert_normal_settings(old,current,name,build)
            if name not in (BODY,SHELL):
                assert np.array_equal(current['local_points'].view(np.uint32),old['local_points'].view(np.uint32)),('Fixed H2 mesh local coords changed',name)
                assert np.array_equal(current['points'],old['points']),('Fixed H2 mesh world coords changed',name)
                report['objects'][name]={'coordinates_exact':True,'topology_UV_colors_weights_materials_exact':True}
            else:
                report['objects'][name]=displacement_checks(name,old,current,source_normals[name],source_masks[name],limits)
            report['objects'][name]['normal_settings']=normal_info
        report['independent_real_feature_contact_definitions']=contact_infos
        assert bpy.data.objects[SHELL].hide_render,'Historical shell must stay hidden'
        for name,expected in source_fixed_idle.items():
            actual=evaluated_points(bpy.data.objects[name],dg)
            assert np.array_equal(actual,expected),('Fixed evaluated H2 mesh moved',name)
            report['objects'][name]['Idle_frame1_coordinates_exact']=True
        for label,rec,old,crossings in (('raw',raw,source_body,source_crossings),
            ('Idle_frame1',idle,source_idle,source_idle_crossings)):
            integrity,newcross=surface_integrity(rec,old)
            assert not newcross-crossings,('New actual head/shoulder surface intersection',label)
            integrity['new_nonadjacent_self_intersections']=0
            report[label+'_head_neck_integrity']=integrity
        shell_integrity,shell_crossings=surface_integrity(raw_record(bpy.data.objects[SHELL]),source_shell,assert_clean=False)
        assert not shell_crossings-source_shell_crossings,'New hidden shell self intersection'
        report['proxy_shell_integrity']=shell_integrity
        assert np.array_equal(idle['triangles'],source_idle['triangles'])
        report['raw_Idle_geometry']={'maximum_vertex_displacement':float(np.linalg.norm(idle['points']-raw['points'],axis=1).max()),
            'topology_exact':bool(np.array_equal(idle['triangles'],raw['triangles']))}
        assert report['raw_Idle_geometry']['maximum_vertex_displacement']<1e-6
        for label,mask in source_masks[BODY].items():
            assert np.array_equal(idle['points'][mask],source_idle['points'][mask]),('Idle actual fixed H2 feature moved',label)
        report['actual_section_widths_raw']=compare_section_widths(source_sections,new_sections)
        report['actual_section_widths_Idle_frame1']=compare_section_widths(source_idle_sections,new_idle_sections)
        report['eyes']={}
        for side in ('L','R'):
            eye=bpy.data.objects[side+' concentric iris and pupil']
            fit=fit_sphere(evaluated_points(eye,dg))
            assert fit['maximum_relative_radius_residual']<=1e-4
            assert np.allclose(fit['center'],source_eye_fits[side]['center'],rtol=0,atol=1e-10)
            assert abs(fit['radius']-source_eye_fits[side]['radius'])<=1e-10
            metrics=visible_metrics(eye.evaluated_get(dg),body.evaluated_get(dg),step=.0003)
            oldmetric=source_eye_metrics[side]
            d0=np.sqrt(4*oldmetric['aperture']['area']/np.pi)
            d1=np.sqrt(4*metrics['aperture']['area']/np.pi)
            center_delta=np.asarray(metrics['aperture']['center'])-np.asarray(oldmetric['aperture']['center'])
            report['eyes'][side]={'sphere_fit':fit,'frontal_metrics':metrics,'R1_frontal_metrics':oldmetric,
                'visible_D':float(d1),'visible_D_fraction_change_from_R1':float(d1/d0-1),
                'frontal_aperture_center_delta_XZ':center_delta.tolist(),
                'frontal_aperture_center_absolute_shift':float(np.linalg.norm(center_delta)),
                'soft_diameter_target_2_percent_satisfied':bool(abs(d1/d0-1)<=.02),
                'soft_center_target_0_02D0_satisfied':bool(np.linalg.norm(center_delta)<=.02*D0),
                'raw_and_Idle_eye_geometry_exact':True,'soft_targets_are_diagnostics_not_hard_geometry_locks':True}
        pose_reports=[]
        for (action_name,frame),old in source_poses.items():
            bpy.data.objects['Naiwa_Rig'].animation_data.action=bpy.data.actions[action_name]
            bpy.context.scene.frame_set(frame)
            bpy.context.view_layer.update()
            rec=mesh_record(body,bpy.context.evaluated_depsgraph_get())
            integrity,crossings=surface_integrity(rec,old['record'])
            assert not crossings-old['crossings'],('New posed surface intersection',action_name,frame)
            dots=np.einsum('ij,ij->i',rec['triangle_normals'],old['record']['triangle_normals'])
            assert dots.min()>0.,('Posed triangle orientation flip',action_name,frame)
            for label,mask in source_masks[BODY].items():
                assert np.array_equal(rec['points'][mask],old['record']['points'][mask]),('Posed fixed feature moved',action_name,frame,label)
            pose_reports.append({'action':action_name,'frame':frame,
                'minimum_source_target_triangle_normal_dot':float(dots.min()),
                'new_nonadjacent_self_intersections':0,'integrity':integrity,
                'shoulder_bones_and_animation_unchanged':True,'narrow_fixed_contact_and_remote_coordinates_exact':True})
        report['turning_pose_shoulder_checks']=pose_reports
        report['source_integrity']=source_integrity
        report['source_Idle_integrity']=source_idle_integrity
        report['source_shell_integrity']=source_shell_integrity
        assert sha(SOURCE)==SOURCE_SHA
        assert sha(candidate)==report['candidate_sha256']
        report['source_unchanged']=True
        report['passed']=True
    except Exception as error:
        report['error']=str(error)
        report['source_unchanged']=sha(SOURCE)==SOURCE_SHA
        raise
    finally:
        data=json.dumps(report,ensure_ascii=False,indent=2)+'\n'
        (candidate.parent/'validation_report.json').write_text(data,encoding='utf-8')
        (out/'overall_validation.json').write_text(data,encoding='utf-8')
    print('H2_OVERALL_REFINEMENT_VALIDATION_OK',flush=True)


if __name__=='__main__':
    main()
