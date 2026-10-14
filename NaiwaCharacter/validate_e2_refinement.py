"""Independently inspect saved E2 meshes against a hash-pinned H2.

No eye construction function or intended target surface is imported.  All
measurements use raw saved coordinates and evaluated Idle/animation triangles.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import traceback

import bpy
import numpy as np
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from geometry_diagnostic_utils import array, mesh_record, face_point
from eye_projection_measure import visible_metrics
from saved_delivery_metadata import preserved_links, differences
from validate_lip_eye_refinement import coordinates, weight_hash
from head_neck_validation_utils import raw_record, surface_integrity
from spherical_reference_projection import eye_pixels

BODY = 'Naiwa unified golden body'
SHELL = 'Head and torso silhouette only'
D0 = .11724481274201151
S0 = .23456
EYES = {s+' concentric iris and pupil' for s in ('L', 'R')}
GLINTS = {s+' eye glint' for s in ('L', 'R')}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def digest(data):
    return hashlib.sha256(np.asarray(data).tobytes()).hexdigest()


def mesh_snapshot(obj, bone_names):
    mesh = obj.data
    out = {'coordinates': digest(coordinates(obj)),
           'world_matrix': np.asarray(obj.matrix_world).tolist(),
           'weights': weight_hash(obj, bone_names),
           'vertex_count': len(mesh.vertices),
           'material_slots': [m.name if m else None for m in mesh.materials]}
    for collection, key, width in (('edges', 'vertices', 2),
        ('loops', 'vertex_index', 1), ('polygons', 'loop_start', 1),
        ('polygons', 'loop_total', 1), ('polygons', 'material_index', 1),
        ('polygons', 'use_smooth', 1), ('edges', 'use_edge_sharp', 1)):
        out[collection+'_'+key] = digest(array(getattr(mesh, collection), key,
                                              width, np.int32))
    out['uv_layers'] = {a.name: digest(array(a.data, 'uv', 2, np.float32))
                        for a in mesh.uv_layers}
    out['colors'] = {a.name: {'domain': a.domain, 'type': a.data_type,
                            'hash': digest(array(a.data, 'color', 4, np.float32))}
                     for a in mesh.color_attributes}
    all_weights = hashlib.sha256()
    for vertex in mesh.vertices:
        for name, weight in sorted((obj.vertex_groups[g.group].name, g.weight)
                                   for g in vertex.groups):
            all_weights.update(f'{vertex.index}:{name}:{weight}'.encode())
    out['all_vertex_group_weights'] = all_weights.hexdigest()
    out['all_vertex_group_names'] = list(obj.vertex_groups.keys())
    return out


def asset_snapshot():
    rig = bpy.data.objects['Naiwa_Rig']
    names = set(rig.data.bones.keys())
    out = {o.name: mesh_snapshot(o, names) for o in bpy.data.objects
           if o.type == 'MESH'}
    out['rig'] = {'matrix': np.asarray(rig.matrix_world).tolist(),
                  'bones': [(b.name, b.parent.name if b.parent else None,
                             list(b.head_local), list(b.tail_local),
                             np.asarray(b.matrix_local).tolist())
                            for b in rig.data.bones]}
    return out


def open_pose(path, action='Naiwa_Idle', frame=1):
    bpy.ops.wm.open_mainfile(filepath=str(path), load_ui=False)
    rig = bpy.data.objects['Naiwa_Rig']
    rig.animation_data.action = bpy.data.actions[action]
    rig.animation_data.use_nla = False
    rig.data.pose_position = 'POSE'
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()


def fit_sphere(points):
    origin = points.mean(axis=0)
    p = points-origin
    a = np.column_stack((2*p, np.ones(len(p))))
    fit = np.linalg.lstsq(a, np.einsum('ij,ij->i', p, p), rcond=None)[0]
    center = origin+fit[:3]
    radius = math.sqrt(float(fit[3]+fit[:3]@fit[:3]))
    residual = np.abs(np.linalg.norm(points-center, axis=1)-radius)
    assert radius > 0 and residual.max()/radius < 1e-4, ('Not a true sphere',
                                                        residual.max(), radius)
    return center, radius, {'center': center.tolist(), 'radius': radius,
        'max_absolute_residual': float(residual.max()),
        'max_relative_residual': float(residual.max()/radius)}


def sphere_integrity(rec, center):
    triangles = rec['triangles']
    edge_counts = {}
    faces = set()
    for tri in triangles:
        signature = tuple(sorted(map(int, tri)))
        assert signature not in faces, 'Duplicate eye triangle'
        faces.add(signature)
        for a, b in zip(tri, np.roll(tri, -1)):
            key = tuple(sorted((int(a), int(b))))
            edge_counts[key] = edge_counts.get(key, 0)+1
    assert set(edge_counts.values()) == {2}, 'Eye is not a closed manifold'
    tp = rec['triangle_points']
    area = np.linalg.norm(np.cross(tp[:, 1]-tp[:, 0], tp[:, 2]-tp[:, 0]), axis=1)
    assert area.min() > 1e-12, 'Degenerate eye triangle'
    outward = np.einsum('ij,ij->i', rec['triangle_normals'], tp.mean(axis=1)-center)
    assert outward.min() > 0, 'Inward eye normals'
    euler = len(rec['points'])-len(edge_counts)+len(triangles)
    assert euler == 2, 'Eye topology is not a closed genus-zero sphere'
    assert len(np.unique(np.round(rec['points'], 7), axis=0)) == len(rec['points'])
    return {'triangles': len(triangles), 'watertight': True,
            'Euler_characteristic': int(euler), 'outward_normals': True,
            'duplicate_or_degenerate_triangles': 0}


def glint_contact(glint, center, radius, depsgraph):
    rec = mesh_record(glint, depsgraph)
    signed = np.linalg.norm(rec['points']-center, axis=1)-radius
    crossings = signed[rec['triangles']]
    crossing_count = int(np.count_nonzero((crossings.min(axis=1) <= 0)
                                        & (crossings.max(axis=1) >= 0)))
    assert signed.min() < 0 < signed.max() and crossing_count > 0, (
        glint.name, 'Detached highlight', signed.min(), signed.max())
    # Armature deformation moves the vertices without updating object matrix_world.
    # Recover the highlight's affine frame from corresponding evaluated vertices.
    local = coordinates(glint).astype(float)
    design = np.column_stack((local, np.ones(len(local))))
    affine = np.linalg.lstsq(design, rec['points'], rcond=None)[0]
    fit_error = float(np.max(np.linalg.norm(design@affine-rec['points'], axis=1)))
    assert fit_error < 1e-6, 'Highlight is no longer a coherent rigid surface'
    origin = affine[3]
    normal = (origin-center)/np.linalg.norm(origin-center)
    axis = affine[1].copy()
    axis /= np.linalg.norm(axis)
    dot = float(axis@normal)
    assert dot > .9999, 'Highlight does not follow sphere normal'
    return {'signed_vertex_range': [float(signed.min()), float(signed.max())],
            'crossing_triangle_count': crossing_count,
            'origin_surface_offset': float(np.linalg.norm(origin-center)-radius),
            'normal_alignment_dot': dot,
            'evaluated_affine_fit_max_error': fit_error,
            'normal_frame_source': 'corresponding actual evaluated highlight vertices'}


def native_eye_metrics(side, body, depsgraph, step):
    obj = bpy.data.objects[side+' concentric iris and pupil']
    raw = coordinates(obj).astype(float)
    matrix = np.asarray(obj.matrix_world)
    raw = raw@matrix[:3, :3].T+matrix[:3, 3]
    center, radius, raw_fit = fit_sphere(raw)
    rec = mesh_record(obj, depsgraph, eye=True)
    evaluated_center, evaluated_radius, evaluated_fit = fit_sphere(rec['points'])
    assert raw.shape == rec['points'].shape
    idle_error = float(np.max(np.linalg.norm(raw-rec['points'], axis=1)))
    assert idle_error < 1e-6, 'Idle frame 1 altered the sphere'
    metrics = visible_metrics(obj.evaluated_get(depsgraph),
                              body.evaluated_get(depsgraph), step=step)
    diameter = math.sqrt(4*metrics['aperture']['area']/math.pi)
    pupil_ratio = math.sqrt(metrics['black']['area']/metrics['aperture']['area'])
    aperture_center = np.asarray(metrics['aperture']['center'])
    pupil_center = np.asarray(metrics['black']['center'])
    body_record = mesh_record(body, depsgraph)
    contacts = len(rec['bvh'].overlap(body_record['bvh']))
    assert contacts > 0, 'Eye floats without skin intersection'
    gaps = []
    for p in rec['points']:
        skin = face_point(body_record, p[0], p[2])
        assert skin is not None, 'Globe exceeds body frontal footprint'
        gaps.append(float(skin[1]-p[1]))
    assert min(gaps) < 0 < max(gaps), 'Eye is entirely buried or entirely exposed'
    eye_groups = {g.index: g.name for g in obj.vertex_groups}
    assert all([(eye_groups[g.group], g.weight) for g in v.groups]
               == [('Torso', 1.)] for v in obj.data.vertices), 'Eye weights changed'
    palette = sorted(set(tuple(c.color) for c in obj.data.color_attributes['EyeColor'].data))
    # Green and black share the same sphere vertices. Fit the real shared
    # boundary to a plane rather than trusting a saved gaze-axis property.
    colors = obj.data.color_attributes['EyeColor']
    edge_regions = {}
    black_points = []
    for polygon in obj.data.polygons:
        region = 'black' if colors.data[polygon.loop_start].color[1] < .04 else 'green'
        if region == 'black':
            black_points.extend(raw[list(polygon.vertices)])
        for edge in polygon.edge_keys:
            edge_regions.setdefault(tuple(sorted(edge)), set()).add(region)
    shared = [edge for edge, regions in edge_regions.items() if len(regions) == 2]
    assert shared, 'Green and black do not share a sphere boundary'
    ids = sorted({index for edge in shared for index in edge})
    boundary = raw[ids]
    _, _, vh = np.linalg.svd(boundary-boundary.mean(axis=0), full_matrices=False)
    pupil_axis = vh[-1]
    if pupil_axis@(np.mean(black_points, axis=0)-center) < 0:
        pupil_axis = -pupil_axis
    plane_values = (boundary-center)@pupil_axis
    plane_error = float(np.ptp(plane_values))
    assert plane_error < 2e-6, 'Black cap boundary is not a true spherical latitude'
    return {'raw_sphere': raw_fit, 'Idle_frame1_evaluated_sphere': evaluated_fit,
            'raw_to_Idle_vertex_max': idle_error,
            'sphere_integrity': sphere_integrity(rec, evaluated_center),
            'actual_frontal_metrics': metrics, 'actual_visible_D': diameter,
            'actual_pupil_ratio': pupil_ratio,
            'pupil_offset_from_visible_aperture_xz': (pupil_center-aperture_center).tolist(),
            'eye_skin_triangle_contacts': contacts,
            'vertex_skin_gap_range': [min(gaps), max(gaps)],
            'eye_palette': palette, 'weights': 'Torso 1.0',
            'shared_black_green_boundary': {'edge_count': len(shared),
                'shared_vertex_count': len(ids), 'independently_fitted_axis': pupil_axis.tolist(),
                'plane_coordinate_range': plane_error,
                'fitted_cap_half_angle_degrees': math.degrees(math.acos(
                    float(np.clip(plane_values.mean()/radius, -1., 1.))))},
            'glint_contact': glint_contact(bpy.data.objects[side+' eye glint'],
                                           evaluated_center, evaluated_radius, depsgraph)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--source', required=True, type=Path)
    ap.add_argument('--source-sha', required=True)
    ap.add_argument('--candidate', '--target', required=True, type=Path)
    ap.add_argument('--output', type=Path)
    ap.add_argument('--step', type=float, default=.0006)
    ap.add_argument('--skip-poses', action='store_true')
    args = ap.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    source = args.source.resolve()
    candidate = args.candidate.resolve()
    assert source != candidate and sha(source) == args.source_sha
    candidate_sha = sha(candidate)
    open_pose(source)
    original = asset_snapshot()
    links = preserved_links()
    old_skin = {n: coordinates(bpy.data.objects[n]).copy() for n in (BODY, SHELL)
                if n in bpy.data.objects}
    old_body = old_skin[BODY]
    source_raw = raw_record(bpy.data.objects[BODY])
    source_idle = mesh_record(bpy.data.objects[BODY], bpy.context.evaluated_depsgraph_get())
    source_raw_integrity, source_raw_crossings = surface_integrity(source_raw)
    source_idle_integrity, source_idle_crossings = surface_integrity(source_idle)
    source_poses = {}
    if not args.skip_poses:
        rig = bpy.data.objects['Naiwa_Rig']
        for action_name in ('Naiwa_Walk', 'Naiwa_Laugh', 'Naiwa_Jump'):
            action = bpy.data.actions[action_name]
            rig.animation_data.action = action
            frames = sorted({int(action.frame_range[0]),
                int(round(action.frame_range[0]+.25*(action.frame_range[1]-action.frame_range[0]))),
                int(round(sum(action.frame_range)/2)), int(action.frame_range[1])})
            for frame in frames:
                bpy.context.scene.frame_set(frame); bpy.context.view_layer.update()
                rec = mesh_record(bpy.data.objects[BODY], bpy.context.evaluated_depsgraph_get())
                intact, crossings = surface_integrity(rec)
                source_poses[(action_name, frame)] = (rec, intact, crossings)
        rig.animation_data.action = bpy.data.actions['Naiwa_Idle']
        bpy.context.scene.frame_set(1); bpy.context.view_layer.update()
    original_metrics = {s: visible_metrics(
        bpy.data.objects[s+' concentric iris and pupil'].evaluated_get(
            bpy.context.evaluated_depsgraph_get()),
        bpy.data.objects[BODY].evaluated_get(bpy.context.evaluated_depsgraph_get()),
        step=args.step) for s in ('L', 'R')}
    source_palettes = {s: sorted(set(tuple(c.color) for c in
        bpy.data.objects[s+' concentric iris and pupil'].data.color_attributes['EyeColor'].data))
        for s in ('L', 'R')}
    open_pose(candidate)
    current = asset_snapshot()
    metadata_differences = differences(links, preserved_links())
    assert not metadata_differences, ('Materials, relationships, modifiers or action curves changed',
                                    metadata_differences[:10])
    assert original.keys() == current.keys(), 'Asset set changed'
    local_changes = {}
    for name in original:
        if name in EYES:
            continue
        a, b = dict(original[name]), dict(current[name])
        if name in (BODY, SHELL):
            a.pop('coordinates'); b.pop('coordinates')
        elif name in GLINTS:
            a.pop('world_matrix'); b.pop('world_matrix')
        assert a == b, ('Frozen asset/topology/UV/color/weights changed', name)
    body = bpy.data.objects[BODY]
    new_body = coordinates(body)
    delta = new_body-old_body
    changed = np.any(delta != 0, axis=1)
    assert changed.any(), 'E2 did not modify eye contact skin'
    assert np.array_equal(old_body[:, (0, 2)], new_body[:, (0, 2)]), 'Skin XZ changed'
    source_centers = [np.asarray(original_metrics[s]['aperture']['center']) for s in ('L', 'R')]
    radial_distance = np.minimum.reduce([
        np.linalg.norm(old_body[:, (0, 2)]-center, axis=1) for center in source_centers])
    assert np.all((radial_distance[changed] < .12) & (old_body[changed, 1] < -.18)), (
        'Skin changes escaped the local eye region')
    local_changes = {'changed_vertices': int(changed.sum()),
                     'maximum_total_displacement': float(np.linalg.norm(delta, axis=1).max()),
                     'skin_XZ_exact': True, 'outside_local_front_eye_region_exact': True,
                     'body_topology_UV_colors_materials_effective_weights_exact': True,
                     'changed_source_bounds': [old_body[changed].min(axis=0).tolist(),
                                              old_body[changed].max(axis=0).tolist()]}
    shell_report = None
    if SHELL in old_skin:
        old_shell = old_skin[SHELL]
        new_shell = coordinates(bpy.data.objects[SHELL])
        changed_shell = np.any(old_shell != new_shell, axis=1)
        assert np.array_equal(old_shell[:, (0, 2)], new_shell[:, (0, 2)]), 'Shell XZ changed'
        distance_shell = np.minimum.reduce([
            np.linalg.norm(old_shell[:, (0, 2)]-c, axis=1) for c in source_centers])
        assert np.all((distance_shell[changed_shell] < .12)
                      & (old_shell[changed_shell, 1] < -.18)), 'Shell eye modification escaped scope'
        shell_report = {'changed_vertices': int(changed_shell.sum()),
                        'XZ_exact': True, 'outside_local_front_eye_region_exact': True,
                        'maximum_displacement': float(np.linalg.norm(new_shell-old_shell, axis=1).max())}
    dg = bpy.context.evaluated_depsgraph_get()
    eyes = {s: native_eye_metrics(s, body, dg, args.step) for s in ('L', 'R')}
    for side, result in eyes.items():
        met = result['actual_frontal_metrics']
        center_shift = float(np.linalg.norm(np.asarray(met['aperture']['center'])-
            np.asarray(original_metrics[side]['aperture']['center'])))
        result['source_H2_actual_frontal_metrics'] = original_metrics[side]
        result['visible_center_displacement_from_H2'] = center_shift
        result['axis_ratio_reduction_from_H2'] = float(
            original_metrics[side]['aperture']['axis_ratio']-met['aperture']['axis_ratio'])
        assert result['eye_palette'] == source_palettes[side], 'Eye palette changed'
        assert met['aperture_topology']['significant_enclosed_skin_holes']==0, ('Skin intrudes into visible eye cap',side,met['aperture_topology'])
        assert center_shift <= .02*D0, ('Visible aperture center moved', side, center_shift)
        assert abs(result['actual_visible_D']/D0-1) <= .02, ('Visible diameter changed', side)
        assert met['aperture']['axis_ratio'] <= 1.15, ('Aperture elongation remains', side,
                                                      met['aperture']['axis_ratio'])
        assert abs(result['actual_pupil_ratio']-.53) <= .02, ('Pupil ratio changed', side)
    spacing = (eyes['R']['actual_frontal_metrics']['aperture']['center'][0]
               -eyes['L']['actual_frontal_metrics']['aperture']['center'][0])
    assert abs(spacing-S0) <= .02*D0, ('Eye spacing changed', spacing)
    raw = raw_record(body)
    raw_integrity, raw_crossings = surface_integrity(raw, source_raw)
    idle = mesh_record(body, dg)
    idle_integrity, idle_crossings = surface_integrity(idle, source_idle)
    assert not raw_crossings-source_raw_crossings, 'New raw skin self intersection'
    assert not idle_crossings-source_idle_crossings, 'New Idle skin self intersection'
    local_changes['surface_integrity'] = {'raw': raw_integrity, 'Idle_frame1': idle_integrity,
        'new_raw_self_intersections': 0, 'new_Idle_self_intersections': 0,
        'source_raw': source_raw_integrity, 'source_Idle_frame1': source_idle_integrity}
    views = {}
    body_rec = mesh_record(body, dg)
    eye_recs = {s: mesh_record(bpy.data.objects[s+' concentric iris and pupil'], dg, eye=True)
                for s in ('L', 'R')}
    for view, yaw in (('front', 0.), ('three_quarter45', 45.),
                      ('reference50', 50.), ('side90', 90.)):
        pose = {'type': 'ORTHO', 'yaw': yaw, 'pitch': -2.5 if yaw == 50. else 0.,
                'target': [0., -.12, 2.425 if yaw == 50. else 2.37],
                'distance': 7., 'scale': .86 if yaw == 50. else .85, 'lens': 50.}
        measured = {s: eye_pixels(eye_recs[s], body_rec, pose, 900, step=1.5)
                    for s in ('L', 'R')}
        for side, met in measured.items():
            if met['aperture'].get('visible') and met['pupil'].get('visible'):
                offset = (np.asarray(met['pupil']['center_pixels'])
                          -np.asarray(met['aperture']['center_pixels']))
                met['pupil_offset_render_pixels'] = offset.tolist()
                if yaw == 50.:
                    met['pupil_offset_registered_reference_pixels'] = (
                        offset*.41175526202447044).tolist()
        diameters = [m['aperture'].get('D_pixels', 0.) for m in measured.values()]
        views[view] = {'pose': pose, 'eyes': measured,
                       'far_near_visible_D_ratio': min(diameters)/max(diameters)}
    poses = []
    if not args.skip_poses:
        rig = bpy.data.objects['Naiwa_Rig']
        for action_name in ('Naiwa_Walk', 'Naiwa_Laugh', 'Naiwa_Jump'):
            rig.animation_data.action = bpy.data.actions[action_name]
            for frame in sorted(f for a, f in source_poses if a == action_name):
                bpy.context.scene.frame_set(frame); bpy.context.view_layer.update()
                depsgraph = bpy.context.evaluated_depsgraph_get()
                posed_body = mesh_record(body, depsgraph)
                old_record, old_integrity, old_crossings = source_poses[(action_name, frame)]
                intact, crossings = surface_integrity(posed_body, old_record)
                assert not crossings-old_crossings, ('New posed skin self intersection', action_name, frame)
                report = {'action': action_name, 'frame': frame, 'eyes': {},
                          'body_surface_integrity': intact, 'source_surface_integrity': old_integrity,
                          'new_self_intersections': 0}
                for side in ('L', 'R'):
                    eye = mesh_record(bpy.data.objects[side+' concentric iris and pupil'],
                                      depsgraph, eye=True)
                    center, radius, fitted = fit_sphere(eye['points'])
                    contacts = len(eye['bvh'].overlap(posed_body['bvh']))
                    assert contacts > 0, (action_name, frame, side, 'Detached eye')
                    report['eyes'][side] = {'sphere': fitted,
                        'eye_body_triangle_contacts': contacts,
                        'glint_contact': glint_contact(bpy.data.objects[side+' eye glint'],
                                                       center, radius, depsgraph)}
                poses.append(report)
    assert sha(source) == args.source_sha and sha(candidate) == candidate_sha
    report = {'passed': True, 'source': str(source), 'source_sha256': args.source_sha,
              'candidate': str(candidate), 'candidate_sha256': candidate_sha,
              'independent_geometry_validation': 'passed',
              'visual_selection': 'Requires actual same-condition multi-view images; numeric pass alone does not choose a candidate',
              'source_and_candidate_files_unchanged': True,
              'assets_topology_UV_colors_effective_weights_rig_exact': True,
              'object_relationships_materials_modifiers_action_curves_exact': True,
              'source_action_curve_counts': {n: len(a['curves']) for n, a in links['actions'].items()},
              'skin': local_changes, 'shell': shell_report, 'eyes': eyes, 'visible_eye_spacing': spacing,
              'multi_view_actual_triangle_measurements': views, 'animation_pose_checks': poses,
              'sampling_step': args.step}
    output = args.output.resolve() if args.output else candidate.parent/'validation_report.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, ensure_ascii=False)+'\n', encoding='utf-8')
    print('E2_REFINEMENT_VALIDATION_OK', json.dumps({'passed': True,
        'candidate_sha256': candidate_sha, 'visible_eye_spacing': spacing,
        'eyes': {s: {'D': r['actual_visible_D'],
                    'axis_ratio': r['actual_frontal_metrics']['aperture']['axis_ratio'],
                    'center_shift': r['visible_center_displacement_from_H2'],
                    'pupil_ratio': r['actual_pupil_ratio']} for s, r in eyes.items()}}), flush=True)


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:
        args = sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
        def argument(*names):
            for name in names:
                if name in args:
                    return args[args.index(name)+1]
            return None
        candidate = Path(argument('--candidate', '--target')).resolve()
        output = Path(argument('--output')).resolve() if argument('--output') else candidate.parent/'validation_report.json'
        failure = {'passed': False, 'candidate': str(candidate),
            'candidate_sha256': sha(candidate) if candidate.exists() else None,
            'source': argument('--source'), 'source_sha256': argument('--source-sha'),
            'eyes': {}, 'failure': str(exc), 'traceback': traceback.format_exc()}
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(failure, indent=2)+'\n', encoding='utf-8')
        print('E2_REFINEMENT_VALIDATION_FAILED', str(exc), flush=True)
        raise
