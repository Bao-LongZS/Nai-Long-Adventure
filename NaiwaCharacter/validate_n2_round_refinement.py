"""Independently validate the saved R1 against the hash-pinned delivered N2.

Usage: blender -b --python NaiwaCharacter/validate_n2_round_refinement.py --
This does not construct geometry, save a .blend, render, or export.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from saved_delivery_metadata import preserved_links, differences, settings
from geometry_diagnostic_utils import mesh_record
from head_neck_validation_utils import raw_record, fingerprint, fit_sphere, surface_integrity, evaluated_points
from eye_projection_measure import visible_metrics
from r1_validation_utils import (BODY, SHELL, vertex_normals, exact_local_coordinates,
    normal_snapshot, source_protection, protective_masks, normal_delta_measurement,
    extended_sections, lightweight_sections, compare_sections, frozen_radial_sine)

SOURCE = ROOT / 'versions/20261006_head_neck_refinement/candidate_N2/Naiwa_Character.blend'
SOURCE_SHA = '9397186f3ae13e93e05cc0d492c12ceb68b743ab4d2a9a0d650426a47a66b0cb'
DEFAULT_OUT = ROOT / 'versions/20261007_N2_round_refinement'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def open_pose(path, action='Naiwa_Idle', frame=1):
    bpy.ops.wm.open_mainfile(filepath=str(path))
    rig = bpy.data.objects['Naiwa_Rig']
    rig.animation_data.action = bpy.data.actions[action]
    rig.animation_data.use_nla = False
    rig.data.pose_position = 'POSE'
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()


def meshes_snapshot():
    return {obj.name: {'fingerprint': fingerprint(obj),
        'local_points': exact_local_coordinates(obj),
        'points': np.asarray([tuple(obj.matrix_world @ v.co) for v in obj.data.vertices], float),
        'normal_settings': normal_snapshot(obj)}
        for obj in bpy.data.objects if obj.type == 'MESH'}


def rig_snapshot():
    rig = bpy.data.objects['Naiwa_Rig']
    return {'matrix': list(map(list, rig.matrix_world)),
        'bones': {b.name: {'matrix': list(map(list, b.matrix_local)),
            'parent': b.parent.name if b.parent else None, 'head': list(b.head_local),
            'tail': list(b.tail_local), 'settings': settings(b)} for b in rig.data.bones},
        'pose_bones': {b.name: {'constraints': [(c.name, c.type, settings(c)) for c in b.constraints],
            'rotation_mode': b.rotation_mode, 'lock_location': list(b.lock_location),
            'lock_rotation': list(b.lock_rotation), 'lock_scale': list(b.lock_scale)}
            for b in rig.pose.bones}}


def assert_normal_settings(source, candidate, name, build):
    old = source['normal_settings']
    new = candidate['normal_settings']
    assert old['smooth_polygon_count'] == new['smooth_polygon_count'], ('Smooth shading changed', name)
    assert old['render_hidden'] == new['render_hidden'], ('Visibility changed', name)
    if old['custom_corner_normals_sha256'] != new['custom_corner_normals_sha256'] or old['has_custom_normals'] != new['has_custom_normals']:
        reason = build.get('normal_cleanup_reason') or build.get('normal_changes') or build.get('normal_diagnostics', {}).get('cleanup_reason')
        assert reason, ('Custom split normals changed without actual-staleness reason', name)
        return {'source': old, 'candidate': new, 'changed': True, 'recorded_reason': reason}
    return {'source': old, 'candidate': new, 'changed': False}


def displacement_checks(name, source, current, normals, masks, radial_sine):
    p, q = source['points'], current['points']
    local_p, local_q = source['local_points'], current['local_points']
    # Compare actual float32 bit patterns, including the sign of zero.
    assert np.array_equal(local_q[:, 2].view(np.uint32), local_p[:, 2].view(np.uint32)), ('Final vertex Z bits changed', name)
    assert np.array_equal(q[:, 2], p[:, 2]), ('Final world Z changed', name)
    delta = q - p
    amount = np.linalg.norm(delta, axis=1)
    dx = np.abs(delta[:, 0])
    assert float(dx.max()) <= .004 + 1e-7, ('X displacement limit', name, float(dx.max()))
    assert float(amount.max()) <= .015 + 1e-7, ('Global maximum displacement limit', name, float(amount.max()))
    outside = (p[:, 2] <= 2.03) | (p[:, 2] >= 2.50)
    assert np.array_equal(q[outside], p[outside]), ('Moved outside the scoped feather band', name)
    protection = {}
    for label, mask in masks.items():
        assert np.array_equal(q[mask], p[mask]), ('Protected N2 region moved', name, label,
            float(amount[mask].max()) if mask.any() else 0.)
        protection[label] = {'vertex_count': int(mask.sum()), 'coordinates_exact': True}
    projection, tangent = normal_delta_measurement(p, normals, delta)
    z = p[:, 2]
    # Front/side/rear membership comes from the fixed source geometry around
    # each actual section center. Normal projection still uses source normals.
    side_rear = (radial_sine >= -.20) & (z >= 2.07) & (z <= 2.28)
    posterior_main = (radial_sine > .10) & (z >= 2.28) & (z <= 2.43)
    posterior_feather = (radial_sine > .10) & (z >= 2.20) & (z <= 2.50)
    checks = []
    for label, mask, normal_limit, total_limit in (
        ('side_and_rear_neck_Z2_07_to2_28', side_rear, .004, None),
        ('posterior_blend_main_Z2_28_to2_43', posterior_main, .006, .008),
        ('posterior_blend_feather_Z2_20_to2_50', posterior_feather, .006, .008)):
        assert mask.any(), ('Empty displacement region', name, label)
        normal_max = float(np.abs(projection[mask]).max())
        total_max = float(amount[mask].max())
        assert normal_max <= normal_limit + 1e-7, ('Normal projection limit', name, label, normal_max, normal_limit)
        if total_limit is not None:
            assert total_max <= total_limit + 1e-7, ('Posterior total displacement limit', name, label, total_max, total_limit)
        checks.append({'region': label, 'vertex_count': int(mask.sum()),
            'maximum_absolute_source_normal_projection': normal_max, 'normal_limit': normal_limit,
            'maximum_total_displacement': total_max, 'total_limit': total_limit,
            'maximum_tangential_displacement': float(tangent[mask].max())})
    return {'all_final_local_Z_float32_bits_exact': True, 'all_final_world_Z_exact': True,
        'topology_UV_colors_all_weights_transforms_exact': True,
        'outside_scoped_feather_band_exact': True, 'protected_regions': protection,
        'changed_vertices': int(np.count_nonzero(amount)), 'maximum_total_displacement': float(amount.max()),
        'maximum_absolute_X_displacement': float(dx.max()),
        'delta_Y_range': [float(delta[:, 1].min()), float(delta[:, 1].max())],
        'source_normal_projection_range': [float(projection.min()), float(projection.max())],
        'maximum_tangential_displacement': float(tangent.max()),
        'normal_regions_definition': 'frozen actual N2 center(z)=(front+rear)/2; radial sine=(Y-center)/hypot(X,Y-center); side/rear >= -0.20 and rear > +0.10; source vertex normals define projections',
        'region_checks': checks}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path, default=DEFAULT_OUT / 'candidate_R1/Naiwa_Character.blend')
    parser.add_argument('--output', type=Path, default=DEFAULT_OUT)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    candidate = args.candidate.resolve()
    out = args.output.resolve()
    assert sha(SOURCE) == SOURCE_SHA, 'Pinned N2 source SHA changed'
    assert candidate.is_file(), str(candidate)
    assert candidate != SOURCE, 'Candidate must be a new R1 file'
    out.mkdir(exist_ok=True, parents=True)
    report = {'candidate': 'R1', 'passed': False, 'source': str(SOURCE), 'source_sha256': SOURCE_SHA,
        'candidate_path': str(candidate), 'candidate_sha256': sha(candidate), 'objects': {}}
    build_path = candidate.parent / 'build_report.json'
    build = json.loads(build_path.read_text(encoding='utf-8')) if build_path.exists() else {}
    try:
        open_pose(SOURCE)
        source_links = preserved_links()
        source_meshes = meshes_snapshot()
        source_rig = rig_snapshot()
        source_body = raw_record(bpy.data.objects[BODY])
        source_shell = raw_record(bpy.data.objects[SHELL])
        source_normals = {name: vertex_normals(bpy.data.objects[name]) for name in (BODY, SHELL)}
        source_radial_sine = {name: frozen_radial_sine(source_meshes[name]['points'], source_body) for name in (BODY, SHELL)}
        protection_context = source_protection(bpy.data.objects[BODY],
            {side: source_meshes[side + ' concentric iris and pupil']['points'] for side in ('L', 'R')},
            bpy.data.objects['Short closed oral seam'])
        source_masks = {name: protective_masks(source_meshes[name]['points'], protection_context)[0] for name in (BODY, SHELL)}
        source_masks[BODY]['arm_weight_ge_0_5_Z_ge_1_75'] = protection_context['shoulder_seeds']
        assert source_masks[BODY]['shoulder_root_continuous_surface_0_012'][protection_context['shoulder_seeds']].all()
        source_integrity, source_crossings = surface_integrity(source_body)
        source_shell_integrity, source_shell_crossings = surface_integrity(source_shell, assert_clean=False)
        dg = bpy.context.evaluated_depsgraph_get()
        source_idle = mesh_record(bpy.data.objects[BODY], dg)
        source_idle_integrity, source_idle_crossings = surface_integrity(source_idle)
        source_fixed_idle = {name: evaluated_points(bpy.data.objects[name], dg) for name in source_meshes if name not in (BODY, SHELL)}
        source_eye_fits = {side: fit_sphere(source_fixed_idle[side + ' concentric iris and pupil']) for side in ('L', 'R')}
        source_eye_metrics = {side: visible_metrics(bpy.data.objects[side + ' concentric iris and pupil'].evaluated_get(dg),
            bpy.data.objects[BODY].evaluated_get(dg), step=.0003) for side in ('L', 'R')}
        source_spacing = source_eye_metrics['R']['aperture']['center'][0] - source_eye_metrics['L']['aperture']['center'][0]
        source_sections = extended_sections(source_body)
        source_idle_sections = extended_sections(source_idle)
        poses = []
        source_poses = {}
        for action_name in ('Naiwa_Walk', 'Naiwa_Laugh', 'Naiwa_Jump'):
            action = bpy.data.actions[action_name]
            start, end = action.frame_range
            frame = int(round(start + (end - start) * .25))
            poses.append((action_name, frame))
            bpy.data.objects['Naiwa_Rig'].animation_data.action = action
            bpy.context.scene.frame_set(frame)
            bpy.context.view_layer.update()
            rec = mesh_record(bpy.data.objects[BODY], bpy.context.evaluated_depsgraph_get())
            integrity, crossings = surface_integrity(rec)
            source_poses[(action_name, frame)] = {'record': rec, 'integrity': integrity, 'crossings': crossings}

        open_pose(candidate)
        meshes = meshes_snapshot()
        # Measurement artifacts precede pass/fail assertions, so a rejected
        # trial still has hash-tied real geometry available for diagnosis.
        dg = bpy.context.evaluated_depsgraph_get()
        body = bpy.data.objects[BODY]
        raw = raw_record(body)
        idle = mesh_record(body, dg)
        np.savez_compressed(out / 'raw_mesh_measurements.npz', points_N2=source_body['points'],
            points_R1=raw['points'], triangles=raw['triangles'], source_normals=source_normals[BODY],
            points_N2_Idle=source_idle['points'], points_R1_Idle=idle['points'])
        new_sections = extended_sections(raw)
        new_idle_sections = extended_sections(idle)
        section_document = {'source_hashes': {'N2': SOURCE_SHA, 'R1': report['candidate_sha256']},
            'models': {'N2': source_sections, 'R1': new_sections},
            'Idle_frame1_models': {'N2': source_idle_sections, 'R1': new_idle_sections},
            'coordinate_axes': {'X': 'right', 'Y': 'rear', 'Z': 'up'},
            'D0': .117244812742, 'actual_triangle_plane_intersections': True}
        (out / 'section_raw.json').write_text(json.dumps(section_document, separators=(',', ':')), encoding='utf-8')
        links = preserved_links()
        changed_links = differences(source_links, links)
        assert not changed_links, ('Object links/materials/modifiers/complete actions changed', changed_links[:12])
        assert rig_snapshot() == source_rig, 'Rig rest matrices/bones/constraints changed'
        assert meshes.keys() == source_meshes.keys(), 'Mesh object inventory changed'
        report['parents_materials_modifiers_rig_and_complete_animation_curves_exact'] = True
        for name, current in meshes.items():
            original = source_meshes[name]
            old_fingerprint = original['fingerprint'].copy()
            current_fingerprint = current['fingerprint'].copy()
            # A justified cleanup of stale split normals is the only permitted
            # difference in the established fingerprint's shading fields.
            old_fingerprint.pop('custom_normals')
            current_fingerprint.pop('custom_normals')
            assert current_fingerprint == old_fingerprint, ('Topology/UV/colors/weights/transform/shading changed', name)
            normal_info = assert_normal_settings(original, current, name, build)
            if name not in (BODY, SHELL):
                assert np.array_equal(current['local_points'].view(np.uint32), original['local_points'].view(np.uint32)), ('Fixed mesh local coordinates changed', name)
                assert np.array_equal(current['points'], original['points']), ('Fixed mesh coordinates changed', name)
                report['objects'][name] = {'coordinates_exact': True,
                    'topology_UV_colors_weights_materials_exact': True, 'normal_settings': normal_info}
            else:
                report['objects'][name] = displacement_checks(name, original, current, source_normals[name], source_masks[name], source_radial_sine[name])
                report['objects'][name]['normal_settings'] = normal_info
        assert bpy.data.objects[SHELL].hide_render, 'Historical proxy shell must stay hidden during rendering'
        for name, expected in source_fixed_idle.items():
            actual = evaluated_points(bpy.data.objects[name], dg)
            assert np.array_equal(actual, expected), ('Fixed evaluated Idle mesh moved', name)
            report['objects'][name]['Idle_frame1_coordinates_exact'] = True
        for pose_label, rec, old, old_crossings in (
            ('raw', raw, source_body, source_crossings),
            ('Idle_frame1', idle, source_idle, source_idle_crossings)):
            integrity, crossings = surface_integrity(rec, old)
            assert not crossings - old_crossings, ('New actual surface self intersection', pose_label)
            integrity['new_nonadjacent_self_intersections'] = 0
            report[pose_label + '_head_neck_integrity'] = integrity
        shell_integrity, shell_crossings = surface_integrity(raw_record(bpy.data.objects[SHELL]), source_shell, assert_clean=False)
        assert not shell_crossings - source_shell_crossings, 'New proxy shell self intersection'
        for key in ('degenerate_triangle_count', 'duplicate_triangles', 'duplicate_vertices_at_1e_7', 'boundary_edges_in_region', 'nonmanifold_edges_in_region'):
            assert shell_integrity[key] <= source_shell_integrity[key], ('New proxy shell surface defect', key)
        shell_integrity['new_nonadjacent_self_intersections'] = 0
        report['proxy_shell_integrity'] = shell_integrity
        assert np.array_equal(idle['triangles'], source_idle['triangles']), 'Idle evaluated topology changed'
        report['raw_Idle_geometry'] = {'maximum_vertex_displacement': float(np.linalg.norm(idle['points'] - raw['points'], axis=1).max()),
            'topology_exact': bool(np.array_equal(idle['triangles'], raw['triangles']))}
        assert report['raw_Idle_geometry']['maximum_vertex_displacement'] < 1e-6
        idle_delta = idle['points'] - source_idle['points']
        assert np.abs(idle_delta[:, 2]).max() <= 5e-7, 'Idle evaluated Z differs beyond evaluation rounding'
        report['Idle_evaluated_maximum_Z_delta'] = float(np.abs(idle_delta[:, 2]).max())
        for label, mask in source_masks[BODY].items():
            assert np.array_equal(idle['points'][mask], source_idle['points'][mask]), ('Idle protected source domain moved', label)
        report['section_constraints_raw'] = compare_sections(source_body, raw, source_sections, new_sections)
        report['section_constraints_Idle_frame1'] = compare_sections(source_idle, idle, source_idle_sections, new_idle_sections)
        report['sections_raw'] = lightweight_sections(new_sections)
        report['sections_Idle_frame1'] = lightweight_sections(new_idle_sections)
        eyes = {}
        for side in ('L', 'R'):
            eye = bpy.data.objects[side + ' concentric iris and pupil']
            fit = fit_sphere(evaluated_points(eye, dg))
            assert fit['maximum_relative_radius_residual'] <= 1e-4
            assert np.allclose(fit['center'], source_eye_fits[side]['center'], rtol=0, atol=1e-10)
            assert abs(fit['radius'] - source_eye_fits[side]['radius']) <= 1e-10
            metrics = visible_metrics(eye.evaluated_get(dg), body.evaluated_get(dg), step=.0003)
            source_d = np.sqrt(4 * source_eye_metrics[side]['aperture']['area'] / np.pi)
            current_d = np.sqrt(4 * metrics['aperture']['area'] / np.pi)
            assert abs(current_d / source_d - 1) < .001, ('Visible eye diameter changed', side)
            assert np.max(np.abs(np.asarray(metrics['aperture']['center']) - np.asarray(source_eye_metrics[side]['aperture']['center']))) < .0003
            eyes[side] = {'sphere_fit': fit, 'frontal_metrics': metrics, 'visible_D': float(current_d),
                'visible_D_fraction_change_from_N2': float(current_d / source_d - 1.),
                'raw_and_Idle_coordinates_exact': True}
        spacing = eyes['R']['frontal_metrics']['aperture']['center'][0] - eyes['L']['frontal_metrics']['aperture']['center'][0]
        assert abs(spacing / source_spacing - 1.) < .001
        report['eyes'] = eyes
        report['actual_frontal_spacing'] = spacing
        report['actual_frontal_spacing_fraction_change_from_N2'] = spacing / source_spacing - 1.
        pose_reports = []
        for action_name, frame in poses:
            bpy.data.objects['Naiwa_Rig'].animation_data.action = bpy.data.actions[action_name]
            bpy.context.scene.frame_set(frame)
            bpy.context.view_layer.update()
            rec = mesh_record(body, bpy.context.evaluated_depsgraph_get())
            old = source_poses[(action_name, frame)]
            for label in ('shoulder_root_continuous_surface_0_012', 'original_fixed_shoulder_seed_distance_0_012'):
                mask = source_masks[BODY][label]
                assert np.array_equal(rec['points'][mask], old['record']['points'][mask]), ('Posed shoulder root moved', action_name, frame, label)
            integrity, crossings = surface_integrity(rec, old['record'])
            assert not crossings - old['crossings'], ('New posed head/shoulder self intersection', action_name, frame)
            dots = np.einsum('ij,ij->i', rec['triangle_normals'], old['record']['triangle_normals'])
            assert dots.min() > 0., ('Posed triangle orientation changed', action_name, frame)
            pose_reports.append({'action': action_name, 'frame': frame,
                'protected_shoulder_root_coordinates_exact': True,
                'minimum_source_target_triangle_normal_dot': float(dots.min()),
                'new_nonadjacent_self_intersections': 0, 'integrity': integrity})
        report['turning_pose_shoulder_checks'] = pose_reports
        report['source_integrity'] = source_integrity
        report['source_Idle_integrity'] = source_idle_integrity
        report['source_shell_integrity'] = source_shell_integrity
        report['source_unchanged'] = sha(SOURCE) == SOURCE_SHA
        assert report['source_unchanged'], 'Pinned N2 changed during validation'
        assert sha(candidate) == report['candidate_sha256'], 'R1 changed during independent validation'
        report['passed'] = True
    except Exception as error:
        report['error'] = str(error)
        report['source_unchanged'] = sha(SOURCE) == SOURCE_SHA
        raise
    finally:
        data = json.dumps(report, indent=2, ensure_ascii=False) + '\n'
        (candidate.parent / 'validation_report.json').write_text(data, encoding='utf-8')
        (out / 'round_validation.json').write_text(data, encoding='utf-8')
    print('N2_ROUND_REFINEMENT_VALIDATION_OK')


if __name__ == '__main__':
    main()
