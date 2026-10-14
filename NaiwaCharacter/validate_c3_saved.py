"""Validate one saved C3 candidate against the cached, immutable E2 baseline.

This script opens the candidate once. It does not render, reconstruct historical
stages, export, alter parameters, or save a .blend. Baseline array/schema wiring
is shared with prepare_c3_baseline.py and the C3 field preparer.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import traceback

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

SCRIPT_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_ROOT))
from geometry_diagnostic_utils import array
from head_neck_validation_utils import fingerprint
from saved_delivery_metadata import preserved_links, differences
from validate_n2_round_refinement import rig_snapshot
from eye_projection_measure import visible_metrics

BODY = 'Naiwa unified golden body'
SHELL = 'Head and torso silhouette only'
MOUTH = 'Short closed oral seam'
EYES = {s: s + ' concentric iris and pupil' for s in ('L', 'R')}
E2_SHA256 = '317f62278d63217636bf393f831e2fb41ce21acfefbe21beed6ad6d4dc75f322'


def sha(path):
    hasher = hashlib.sha256()
    with Path(path).open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            hasher.update(chunk)
    return hasher.hexdigest()


def dump(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n',
                    encoding='utf-8')


def exact_bits(a, b):
    return a.shape == b.shape and a.dtype == b.dtype and a.tobytes() == b.tobytes()


def raw_arrays(obj):
    local = array(obj.data.vertices, 'co', 3, np.float32)
    matrix = np.asarray(obj.matrix_world, dtype=float)
    world = local.astype(float) @ matrix[:3, :3].T + matrix[:3, 3]
    obj.data.calc_loop_triangles()
    triangles = array(obj.data.loop_triangles, 'vertices', 3, np.int32)
    return local, world, triangles


def bvh(points, triangles):
    return BVHTree.FromPolygons(points.tolist(), triangles.tolist(), all_triangles=True)


def changed_region_integrity(old_points, new_points, triangles, changed, label):
    """Compare changed-face integrity including collisions with unchanged faces.

    Selecting only the head and self-overlapping that crop would miss a moved
    head/shoulder face colliding with another part of the unchanged body.
    """
    assert old_points.shape == new_points.shape and np.isfinite(new_points).all(), label
    face_ids = np.flatnonzero(changed[triangles].any(axis=1))
    if not len(face_ids):
        return {'checked_triangle_count': 0, 'unchanged_surface': True}
    faces = triangles[face_ids]
    used = np.unique(faces)
    old_tp, new_tp = old_points[faces], new_points[faces]
    old_cross = np.cross(old_tp[:, 1] - old_tp[:, 0], old_tp[:, 2] - old_tp[:, 0])
    new_cross = np.cross(new_tp[:, 1] - new_tp[:, 0], new_tp[:, 2] - new_tp[:, 0])
    old_twice_area = np.linalg.norm(old_cross, axis=1)
    new_twice_area = np.linalg.norm(new_cross, axis=1)
    valid = old_twice_area > 2e-12
    newly_degenerate = valid & (new_twice_area <= 2e-12)
    assert not newly_degenerate.any(), ('New degenerate face', label)
    dot = np.einsum('ij,ij->i', old_cross[valid], new_cross[valid])
    dot /= old_twice_area[valid] * np.maximum(new_twice_area[valid], 1e-30)
    assert len(dot) and dot.min() > 0., ('Source/target triangle orientation flip', label)
    old_duplicate = len(used) - len(np.unique(np.round(old_points[used], 7), axis=0))
    new_duplicate = len(used) - len(np.unique(np.round(new_points[used], 7), axis=0))
    assert new_duplicate <= old_duplicate, ('New coincident vertices', label)
    # Fixed connectivity alone would miss separating two unwelded but truly
    # coincident source seam vertices. Preserve exact source coincidence groups.
    _, inverse, counts = np.unique(old_points[used], axis=0, return_inverse=True, return_counts=True)
    coincidence_groups = np.flatnonzero(counts > 1)
    for group in coincidence_groups:
        tied = new_points[used[inverse == group]]
        assert np.array_equal(tied, np.repeat(tied[:1], len(tied), axis=0)), ('Existing seam coincidence separated', label, int(group))

    def crossing_pairs(points):
        full = bvh(points, triangles)
        partial = bvh(points, faces)
        pairs = set()
        for local_a, global_b in partial.overlap(full):
            global_a = int(face_ids[local_a])
            if global_a == global_b or np.intersect1d(triangles[global_a], triangles[global_b]).size:
                continue
            pairs.add(tuple(sorted((global_a, int(global_b)))))
        return pairs

    old_pairs, new_pairs = crossing_pairs(old_points), crossing_pairs(new_points)
    introduced = new_pairs - old_pairs
    assert not introduced, ('New nonadjacent self intersections', label, sorted(introduced)[:12])
    return {'checked_triangle_count': int(len(faces)),
            'collision_scope': 'changed faces against all body triangles',
            'finite_coordinates': True, 'minimum_triangle_area': float(new_twice_area.min() / 2),
            'minimum_source_target_normal_dot': float(dot.min()),
            'source_degenerate_triangle_count': int(np.count_nonzero(~valid)),
            'new_degenerate_triangle_count': 0,
            'same_indexed_connectivity_and_source_coincident_seams_preserved': True,
            'source_exact_coincidence_groups_checked': int(len(coincidence_groups)),
            'source_coincident_vertices_at_1e_7': int(old_duplicate),
            'target_coincident_vertices_at_1e_7': int(new_duplicate),
            'source_nonadjacent_intersection_count': len(old_pairs),
            'target_nonadjacent_intersection_count': len(new_pairs),
            'new_nonadjacent_intersection_count': 0}


def set_pose(action='Naiwa_Idle', frame=1):
    rig = bpy.data.objects['Naiwa_Rig']
    rig.animation_data_create().action = bpy.data.actions[action]
    rig.animation_data.use_nla = False
    rig.data.pose_position = 'POSE'
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()
    return bpy.context.evaluated_depsgraph_get()


def contact_distances(points, target_bvh):
    result = []
    for point in points:
        nearest = target_bvh.find_nearest(Vector(point))
        assert nearest is not None, 'No target surface for contact sample'
        result.append(nearest[3])
    return np.asarray(result, float)


def evaluated_record(obj, depsgraph):
    """Release the temporary evaluated mesh after copying its real triangles."""
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh(preserve_all_data_layers=True, depsgraph=depsgraph)
    try:
        local = array(mesh.vertices, 'co', 3, np.float32).astype(float)
        matrix = np.asarray(evaluated.matrix_world, dtype=float)
        points = local @ matrix[:3, :3].T + matrix[:3, 3]
        mesh.calc_loop_triangles()
        triangles = array(mesh.loop_triangles, 'vertices', 3, np.int32)
        return {'points': points, 'triangles': triangles}
    finally:
        evaluated.to_mesh_clear()


def cached_record(snapshot, cache, name, pose):
    prefix = snapshot['objects'][name]['array_prefix']
    if pose == 'Raw':
        return {'points': cache[prefix + '_raw_world'], 'triangles': cache[prefix + '_triangles']}
    return {'points': cache[prefix + '_' + pose + '_world'],
            'triangles': cache[prefix + '_' + pose + '_triangles']}


def record_bvh(record):
    return bvh(record['points'], record['triangles'])


def other_mesh_intersections(snapshot, cache, records, pose, changed):
    """Changed body faces cannot introduce collisions with fixed accessories."""
    body = records[BODY]
    old_body = cached_record(snapshot, cache, BODY, pose)
    face_ids = np.flatnonzero(changed[body['triangles']].any(axis=1))
    if not len(face_ids):
        return {'checked_objects': [], 'new_intersection_count': 0, 'unchanged_body': True}
    faces = body['triangles'][face_ids]
    used = np.unique(faces)
    lo = np.minimum(body['points'][used].min(axis=0), old_body['points'][used].min(axis=0))
    hi = np.maximum(body['points'][used].max(axis=0), old_body['points'][used].max(axis=0))
    old_partial, new_partial = bvh(old_body['points'], faces), bvh(body['points'], faces)
    result = []
    for name in sorted(set(records) - {BODY, SHELL, MOUTH, *EYES.values()}):
        fixed = records[name]
        if np.any(fixed['points'].max(axis=0) < lo) or np.any(fixed['points'].min(axis=0) > hi):
            continue
        fixed_bvh = record_bvh(fixed)
        old_pairs = set(old_partial.overlap(fixed_bvh))
        new_pairs = set(new_partial.overlap(fixed_bvh))
        added = new_pairs - old_pairs
        assert not added, ('New actual body/fixed-mesh intersection', pose, name, sorted(added)[:12])
        result.append({'object': name, 'source_changed_face_intersections': len(old_pairs),
            'candidate_changed_face_intersections': len(new_pairs), 'new_intersections': 0})
    return {'checked_objects': result, 'new_intersection_count': 0,
            'scope': 'changed body triangles vs fixed visible mesh inventory, bounding-box broad phase'}


def actual_pair_lines(a, a_tree, b, b_tree):
    """Use the same actual triangle intersection as the E2 contact definition."""
    from prepare_c3_baseline import triangle_line
    lines = {}
    for i, j in a_tree.overlap(b_tree):
        segment = triangle_line(a['points'][a['triangles'][i]], b['points'][b['triangles'][j]])
        if segment is not None:
            # Canonical endpoint order lets the source/candidate curve compare
            # actual positions without depending on BVH traversal order.
            segment = segment[np.lexsort((segment[:, 2], segment[:, 1], segment[:, 0]))]
            lines[(i, j)] = segment
    return lines


def verify_contacts(snapshot, cache, records, pose):
    """Check actual saved triangle contact, retaining existing embedded seams.

    Globe/skin intersection is expected and is measured separately from new
    collisions. All mouth and highlight geometry is fixed; comparing same-state
    source/candidate distances avoids inventing a new zero-gap requirement.
    """
    old_body = cached_record(snapshot, cache, BODY, pose)
    body = records[BODY]
    old_bvh, body_bvh = record_bvh(old_body), record_bvh(body)
    old_mouth = cached_record(snapshot, cache, MOUTH, pose)
    mouth = records[MOUTH]
    old_gap = contact_distances(old_mouth['points'], old_bvh)
    new_gap = contact_distances(mouth['points'], body_bvh)
    mouth_gap_delta = np.abs(new_gap - old_gap)
    assert mouth_gap_delta.max(initial=0) <= 5e-7, ('Mouth/body actual surface gap changed', pose, float(mouth_gap_delta.max()))
    mouth_bvh = record_bvh(mouth)
    old_lines = actual_pair_lines(old_body, old_bvh, old_mouth, record_bvh(old_mouth))
    new_lines = actual_pair_lines(body, body_bvh, mouth, mouth_bvh)
    old_pairs, new_pairs = set(old_lines), set(new_lines)
    # Raw/Idle1 define the protected seam. Some original animated poses (E2
    # Laugh7) have no actual triangle intersection; preserve that same-state
    # relationship instead of inventing a contact curve for the source pose.
    if pose in ('Raw', 'Idle1'):
        assert old_pairs and new_pairs, ('Missing actual mouth/body contact curve', pose)
    assert new_pairs == old_pairs, ('Mouth/body actual contact triangle pairs changed', pose)
    maximum_line_delta = None
    if old_pairs:
        maximum_line_delta = max(float(np.linalg.norm(new_lines[pair] - old_lines[pair], axis=1).max()) for pair in old_pairs)
        assert maximum_line_delta <= 5e-7, ('Actual mouth/body contact curve moved', pose, maximum_line_delta)
    else:
        assert 'body_contact_vertex_ids' in cache and len(cache['body_contact_vertex_ids']), ('Missing protected skin samples for source pose without mouth intersection', pose)
    result = {'mouth': {'source_body_mouth_triangle_contacts': len(old_pairs),
        'candidate_body_mouth_triangle_contacts': len(new_pairs),
        'actual_contact_triangle_pairs_exact': True,
        'baseline_curve_absent': not bool(old_pairs),
        'contact_relationship_verification': ('same actual contact triangle pairs and curve positions'
            if old_pairs else 'same absent intersection curve; all mouth-to-body and protected-body-to-mouth actual surface gaps retained'),
        'actual_contact_curve_maximum_point_displacement': maximum_line_delta,
        'actual_mouth_vertices_to_body_surface_maximum_gap_delta': float(mouth_gap_delta.max(initial=0)),
        'existing_source_maximum_distance': float(old_gap.max(initial=0)),
        'candidate_maximum_distance': float(new_gap.max(initial=0)),
        'gap_units': 'world Euclidean units', 'gap_comparison_tolerance': 5e-7}}
    if 'body_contact_vertex_ids' in cache:
        contact_ids = cache['body_contact_vertex_ids'].astype(int)
        old_contact_gap = contact_distances(old_body['points'][contact_ids], mouth_bvh)
        new_contact_gap = contact_distances(body['points'][contact_ids], mouth_bvh)
        delta = np.abs(new_contact_gap - old_contact_gap)
        assert delta.max(initial=0) <= 5e-7, ('Protected skin/mouth contact changed', pose)
        result['mouth']['fixed_body_contact_vertex_count'] = int(len(contact_ids))
        result['mouth']['body_contact_to_mouth_surface_maximum_gap_delta'] = float(delta.max(initial=0))
    result['eyes'] = {}
    eye_mask = cache['body_eye_local_mask'].astype(bool)
    changed = np.linalg.norm(body['points'] - old_body['points'], axis=1) > 0
    result['other_fixed_asset_intersections'] = other_mesh_intersections(snapshot, cache, records, pose, changed)
    for side in ('L', 'R'):
        eye = records[EYES[side]]
        old_eye = cached_record(snapshot, cache, EYES[side], pose)
        eye_bvh = record_bvh(eye)
        old_eye_bvh = record_bvh(old_eye)
        old_eye_pairs = set(old_bvh.overlap(old_eye_bvh))
        new_eye_pairs = set(body_bvh.overlap(eye_bvh))
        assert old_eye_pairs and new_eye_pairs, ('Globe/body has no actual surface contact', side, pose)
        added = new_eye_pairs - old_eye_pairs
        removed = old_eye_pairs - new_eye_pairs
        # Eye-local skin may be adjusted only within its included tighter cap;
        # an unrelated neck/shoulder face must never reach the fixed globe.
        assert all(eye_mask[body['triangles'][i]].any() for i, _ in added), ('New neck/shoulder intersection with globe', side, pose)
        if not changed[eye_mask].any():
            assert not added and not removed, ('Fixed eye-local skin changed actual globe contact', side, pose)
        glint_name = side + ' eye glint'
        glint = records[glint_name]
        old_glint = cached_record(snapshot, cache, glint_name, pose)
        old_glint_gap = contact_distances(old_glint['points'], old_eye_bvh)
        glint_gap = contact_distances(glint['points'], eye_bvh)
        assert exact_bits(glint['points'], old_glint['points']) and exact_bits(eye['points'], old_eye['points']), ('Globe/glint evaluated geometry changed', side, pose)
        assert np.max(np.abs(glint_gap - old_glint_gap), initial=0) <= 1e-10, ('Highlight/globe surface contact changed', side, pose)
        result['eyes'][side] = {'source_body_globe_triangle_contacts': len(old_eye_pairs),
            'candidate_body_globe_triangle_contacts': len(new_eye_pairs),
            'added_body_globe_contact_pairs': len(added), 'removed_body_globe_contact_pairs': len(removed),
            'globe_glint_coordinates_exact': True, 'highlight_surface_contact_preserved': True,
            'source_glint_minimum_unsigned_surface_distance': float(old_glint_gap.min()),
            'candidate_glint_minimum_unsigned_surface_distance': float(glint_gap.min()),
            'highlight_vertex_surface_distance_maximum_delta': float(np.max(np.abs(glint_gap - old_glint_gap), initial=0))}
    return result


def saved_section_measurements(protocol, measurements):
    # Installed from the common, once-frozen E2 sampler by the baseline agent.
    from c3_measurement_utils import measure_sections
    return {pose: measure_sections(measurements[pose + '_points'],
                measurements[pose + '_triangles'], protocol)
            for pose in ('Raw', 'Idle1')}


def verify_eye_metrics(old, new, scale=None):
    assert old['sampling']['step'] == new['sampling']['step'] == .0003
    assert old['sampling']['skin_epsilon'] == new['sampling']['skin_epsilon']
    assert old['sampling']['grid_anchor_xz'] == new['sampling']['grid_anchor_xz']
    old_d = np.sqrt(4 * old['aperture']['area'] / np.pi)
    new_d = np.sqrt(4 * new['aperture']['area'] / np.pi)
    center_delta = np.asarray(new['aperture']['center']) - np.asarray(old['aperture']['center'])
    diameter_change = abs(new_d / old_d - 1.)
    center_shift = np.linalg.norm(center_delta)
    axis_delta = abs(new['aperture']['axis_ratio'] - old['aperture']['axis_ratio'])
    assert diameter_change <= .01 + 1e-9, ('Visible eye diameter changed', diameter_change)
    assert center_shift <= .01 * old_d + 1e-9, ('Visible aperture center shifted', center_shift)
    assert axis_delta <= .02 + 1e-9, ('Visible aperture axis ratio changed', axis_delta)
    assert new['aperture_topology']['visible_component_count'] == old['aperture_topology']['visible_component_count']
    assert new['aperture_topology']['significant_enclosed_skin_holes'] <= old['aperture_topology']['significant_enclosed_skin_holes']
    return {'sampling_step': .0003, 'E2_visible_D': float(old_d), 'candidate_visible_D': float(new_d),
            'absolute_diameter_fraction_change': float(diameter_change),
            'center_delta_world_XZ': center_delta.tolist(), 'center_shift': float(center_shift),
            'center_shift_fraction_of_E2_eye_D': float(center_shift / old_d),
            'absolute_axis_ratio_delta': float(axis_delta),
            'orthographic_metric_equivalence': 'world XZ normalized by D equals common orthographic pixel normalization',
            'candidate_metrics': new}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root', type=Path, required=True)
    ap.add_argument('--candidate', type=Path, required=True)
    ap.add_argument('--label', choices=('A', 'B'), required=True)
    args = ap.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    run_validation(args)


def run_validation(args):
    # Final baseline schema is deliberately read from the immutable cache,
    # never re-derived from a candidate or an older H2/R1 construction stage.
    root = args.root.resolve(strict=True)
    candidate = args.candidate.resolve(strict=True)
    baseline = root / 'baseline'
    snapshot_path = baseline / 'snapshot.json'
    arrays_path = baseline / 'baseline_arrays.npz'
    protocol_path = root / 'measurement_protocol.json'
    snapshot = json.loads(snapshot_path.read_text(encoding='utf-8-sig'))
    protocol = json.loads(protocol_path.read_text(encoding='utf-8-sig'))
    pinned_files = {str(p): sha(p) for p in (snapshot_path, arrays_path, protocol_path)}
    assert snapshot.get('protocol_frozen'), 'Baseline protocol has not been finalized before candidate validation'
    helper = Path(protocol['sections']['measurement_helper_file']).resolve(strict=True)
    assert helper == (SCRIPT_ROOT / 'c3_measurement_utils.py').resolve(), 'Unexpected section sampler implementation'
    pinned_files[str(helper)] = sha(helper)
    assert protocol['sections']['measurement_helper_sha256'] == pinned_files[str(helper)], 'Fixed section sampler implementation changed'
    assert snapshot['array_file_sha256'] == pinned_files[str(arrays_path)], 'Baseline arrays no longer match finalized snapshot'
    assert snapshot['measurement_protocol_sha256'] == pinned_files[str(protocol_path)], 'Measurement protocol no longer matches finalized baseline'
    source = Path(snapshot['source'])
    source_sha = snapshot['source_sha256']
    assert source_sha == E2_SHA256, 'Baseline is not the authorized hash-pinned E2'
    assert candidate.suffix.lower() == '.blend' and candidate.is_relative_to(root), 'Candidate must be a new blend within C3 output root'
    assert candidate != source.resolve(), 'Validation outputs must never be written beside the fixed E2 source'
    assert sha(source) == source_sha, 'Fixed E2 source hash changed'
    candidate_sha = sha(candidate)
    out = candidate.parent
    build_path = out / 'build_report.json'
    build = json.loads(build_path.read_text(encoding='utf-8-sig'))
    assert build['source_sha256'] == source_sha and build['candidate_sha256'] == candidate_sha, 'Saved candidate does not match its generation provenance'
    assert build['measurement_protocol_sha256'] == pinned_files[str(protocol_path)], 'Candidate was generated with a different baseline protocol'
    assert build['field_sha256'] == sha(root / 'c3_field.npz'), 'Candidate direction field changed after generation'
    field_report_path = root / 'c3_field_report.json'
    field_report = json.loads(field_report_path.read_text(encoding='utf-8-sig'))
    assert field_report['baseline_arrays_sha256'] == pinned_files[str(arrays_path)], 'Direction field was prepared from different baseline arrays'
    assert field_report['measurement_protocol_sha256'] == pinned_files[str(protocol_path)], 'Direction field was prepared with a different measurement protocol'
    assert field_report['field_sha256'] == build['field_sha256'], 'Direction field provenance differs from saved candidate'
    assert field_report['scales'][args.label] == build['amplitude_scale'] and build['label'] == args.label, 'Candidate label/amplitude differs from generation provenance'
    pinned_files[str(build_path)] = sha(build_path)
    pinned_files[str(field_report_path)] = sha(field_report_path)
    pinned_files[str(root / 'c3_field.npz')] = build['field_sha256']
    report = {'schema_version': 1, 'label': args.label, 'candidate': str(candidate),
        'candidate_sha256': candidate_sha, 'E2_source': str(source),
        'E2_source_sha256': source_sha, 'baseline_cache_sha256': pinned_files,
        'saved_generation_provenance_verified': True,
        'validation_scope': 'one saved selected candidate; no rendering/export/model mutation',
        'passed': False, 'objects': {}, 'poses': {},
        'visual_acceptance_required': ['same-camera gray and colored views',
            '45/90 degree stripe reflections and two grazing light directions',
            'protection/release boundary narrow ridges or ring marks',
            'natural front-neck concavity, wedge reduction and posterior shoulder continuity',
            'reference contour improvement and remaining similarity'],
        'automated_geometry_does_not_replace_visual_acceptance': True}
    cache = np.load(arrays_path, allow_pickle=False)
    measurements = {}
    try:
        bpy.ops.wm.open_mainfile(filepath=str(candidate), load_ui=False)
        dg = set_pose()
        names = set(snapshot['objects'])
        assert names == {o.name for o in bpy.data.objects if o.type == 'MESH'}, 'Mesh inventory changed'
        assert BODY in names and SHELL in names and MOUTH in names
        link_diffs = differences(snapshot['preserved_links'], preserved_links())
        assert not link_diffs, ('Object links/materials/modifiers/actions changed', link_diffs[:12])
        rig_diffs = differences(snapshot['rig_snapshot'], rig_snapshot())
        assert not rig_diffs, ('Rig rest bones/constraints changed', rig_diffs[:12])
        report['asset_links_materials_complete_animation_curves_and_rig_exact'] = True
        old_body = snapshot['objects'][BODY]
        body_prefix = old_body['array_prefix']
        scope = cache['body_scope_weight']
        release = cache['body_release_weight']
        frozen = cache['body_frozen_mask'].astype(bool)
        eye_local = cache['body_eye_local_mask'].astype(bool)
        allowed = (scope > 0) | (release > 0)
        assert len(scope) == len(frozen) == len(eye_local)
        assert not np.any(frozen & (scope > 0)), 'Baseline frozen/scope mask conflict'
        assert not np.any(frozen & (release > 0)), 'Baseline frozen/release mask conflict'
        # This revised solver deliberately leaves the eye-local skin unchanged.
        # Validate that tighter contract against the saved vertex data rather
        # than accepting a merely small displacement under the general cap.
        with np.load(root / 'c3_field.npz', allow_pickle=False) as field:
            direction = field['vertex_direction_world'].copy()
            field_frozen = field['frozen_mask'].astype(bool)
            field_eye = field['eye_local_mask'].astype(bool)
        assert direction.shape == cache[body_prefix + '_raw_world'].shape, 'Direction field vertex shape differs from baseline'
        assert np.isfinite(direction).all(), 'Direction field has nonfinite values'
        assert np.array_equal(field_frozen, frozen), 'Direction field frozen mask differs from immutable baseline'
        assert np.array_equal(field_eye, eye_local), 'Direction field eye-local mask differs from immutable baseline'
        assert not np.any(direction[frozen]), 'Direction field moves frozen baseline vertices'
        assert not np.any(direction[eye_local]), 'Revised direction field moves eye-local skin'
        assert not np.any(direction[~allowed]), 'Direction field moves vertices outside the immutable scope'
        D = float(protocol['scale']['D'])
        main_limit = (0.04 if args.label == 'A' else 0.08) * D
        eye_limit = .025 * D
        tolerance = 2e-8
        report['displacement_contract'] = {'state': 'Raw mesh',
            'method': 'same original vertex index; final world Euclidean candidate-minus-E2',
            'D_visible_aperture_mean': D, 'main_limit': main_limit,
            'eye_local_limit': eye_limit, 'eye_limit_is_included_in_main_limit': True,
            'numerical_tolerance_world': tolerance}
        raw_world = {}
        for name in sorted(names):
            obj = bpy.data.objects[name]
            original = snapshot['objects'][name]
            prefix = original['array_prefix']
            fp_diffs = differences(original['fingerprint'], fingerprint(obj))
            assert not fp_diffs, ('Topology/UV/colors/weights/transform/shading changed', name, fp_diffs[:12])
            local, world, triangles = raw_arrays(obj)
            old_local = cache[prefix + '_raw_local']
            old_world = cache[prefix + '_raw_world']
            old_triangles = cache[prefix + '_triangles']
            assert exact_bits(triangles, old_triangles), ('Raw triangle indices changed', name)
            assert local.shape == old_local.shape and world.shape == old_world.shape, ('Mesh coordinates shape changed', name)
            raw_world[name] = world
            delta = world - old_world
            amount = np.linalg.norm(delta, axis=1)
            detail = {'topology_UV_colors_weights_transform_shading_exact': True,
                'maximum_final_world_displacement': float(amount.max(initial=0)),
                'changed_vertex_count': int(np.count_nonzero(amount))}
            if name != BODY:
                assert exact_bits(local, old_local), ('Frozen mesh local coordinates changed', name)
                assert exact_bits(world, old_world), ('Frozen mesh world coordinates changed', name)
                detail['all_coordinates_bit_exact'] = True
            else:
                assert exact_bits(local[frozen], old_local[frozen]), 'Frozen body local coordinates changed'
                assert exact_bits(world[frozen], old_world[frozen]), 'Frozen body world coordinates changed'
                assert exact_bits(local[eye_local], old_local[eye_local]), 'Revised solver moved eye-local skin local coordinates'
                assert exact_bits(world[eye_local], old_world[eye_local]), 'Revised solver moved eye-local skin world coordinates'
                outside = ~allowed
                assert exact_bits(local[outside], old_local[outside]), 'Body coordinates outside fixed scope changed'
                assert exact_bits(world[outside], old_world[outside]), 'Body world coordinates outside fixed scope changed'
                # Reproduce just the coordinate conversion from the committed
                # field; this verifies that provenance describes the actual
                # saved candidate, not only an accompanying JSON assertion.
                matrix = np.asarray(obj.matrix_world, float)
                inverse = np.linalg.inv(matrix)
                wanted_world = old_world + direction * build['amplitude_scale']
                expected_local = (wanted_world @ inverse[:3, :3].T + inverse[:3, 3]).astype(np.float32)
                expected_local[frozen] = old_local[frozen]
                expected_local[eye_local] = old_local[eye_local]
                assert exact_bits(local, expected_local), 'Saved body coordinates differ from the frozen field and amplitude'
                assert amount.max(initial=0) <= main_limit + tolerance, ('Total world displacement exceeds candidate budget', float(amount.max()))
                assert amount[eye_local].max(initial=0) <= min(main_limit, eye_limit) + tolerance, 'Eye skin world displacement exceeds included local budget'
                changed = amount > 0
                detail.update(frozen_vertex_count=int(frozen.sum()), frozen_coordinates_bit_exact=True,
                    outside_scope_coordinates_bit_exact=True, scope_vertex_count=int((scope > 0).sum()),
                    saved_coordinates_match_frozen_direction_and_amplitude_bit_exact=True,
                    eye_local_coordinates_bit_exact=True,
                    release_vertex_count=int((release > 0).sum()),
                    eye_local_vertex_count=int(eye_local.sum()),
                    eye_local_maximum_world_displacement=float(amount[eye_local].max(initial=0)),
                    maximum_final_axis_displacements=np.abs(delta).max(axis=0).tolist(),
                    maximum_displacement_original_vertex_index=int(amount.argmax()),
                    Raw_surface_integrity=changed_region_integrity(old_world, world, triangles, changed, 'Raw'))
                measurements['Raw_points'] = world
                measurements['Raw_triangles'] = triangles
                measurements['Raw_delta_world'] = delta
            report['objects'][name] = detail
        assert bpy.data.objects[SHELL].hide_render, 'Historical proxy shell must stay hidden'
        print('C3_VALIDATION_RAW_ASSETS_OK', args.label, flush=True)
        report['eyes'] = {}
        body = bpy.data.objects[BODY]
        for side in ('L', 'R'):
            eye = bpy.data.objects[EYES[side]]
            metrics = visible_metrics(eye.evaluated_get(dg), body.evaluated_get(dg),
                step=.0003, skin_epsilon=snapshot['eye_metrics'][side]['sampling']['skin_epsilon'])
            report['eyes'][side] = verify_eye_metrics(snapshot['eye_metrics'][side], metrics)
        print('C3_VALIDATION_VISIBLE_EYES_OK', args.label, flush=True)
        # Every checked pose uses its same-state E2 baseline. Raw and Idle1 are
        # never subtracted from one another to approve a displacement budget.
        poses = (('Idle1', 'Naiwa_Idle', 1), ('Walk9', 'Naiwa_Walk', 9),
                 ('Laugh7', 'Naiwa_Laugh', 7), ('Jump6', 'Naiwa_Jump', 6))
        for pose, action, frame in poses:
            dg = set_pose(action, frame)
            pose_world = {}
            for name in sorted(names):
                record = evaluated_record(bpy.data.objects[name], dg)
                prefix = snapshot['objects'][name]['array_prefix']
                expected = cache[prefix + '_' + pose + '_world']
                expected_triangles = cache[prefix + '_' + pose + '_triangles']
                assert exact_bits(record['triangles'], expected_triangles), ('Evaluated triangle indices changed', name, pose)
                pose_world[name] = record
                if name != BODY:
                    assert exact_bits(record['points'], expected), ('Frozen evaluated mesh moved', name, pose)
                else:
                    assert exact_bits(record['points'][frozen], expected[frozen]), ('Frozen evaluated body moved', pose)
                    delta = record['points'] - expected
                    changed = np.linalg.norm(delta, axis=1) > 0
                    integrity = changed_region_integrity(expected, record['points'], record['triangles'], changed, pose)
                    report['poses'][pose] = {'action': action, 'frame': frame,
                        'same_state_E2_difference': True, 'frozen_body_coordinates_bit_exact': True,
                        'all_other_meshes_coordinates_bit_exact': True,
                        'maximum_world_difference_diagnostic_only': float(np.linalg.norm(delta, axis=1).max(initial=0)),
                        'integrity': integrity}
                    measurements[pose + '_points'] = record['points']
                    measurements[pose + '_triangles'] = record['triangles']
            report['poses'][pose]['contacts'] = verify_contacts(snapshot, cache, pose_world, pose)
            print('C3_VALIDATION_STATE_OK', args.label, pose, flush=True)
        report['Raw_contacts'] = verify_contacts(snapshot, cache,
            {name: {'points': raw_world[name], 'triangles': cache[snapshot['objects'][name]['array_prefix'] + '_triangles']}
                for name in names}, 'Raw')
        # The shared fixed section sampler reads saved candidate triangles and
        # records actual Raw/Idle1 shape; it cannot silently fit new windows.
        report['actual_sections'] = saved_section_measurements(protocol, measurements)
        print('C3_VALIDATION_SAVED_SECTIONS_OK', args.label, flush=True)
        assert sha(candidate) == candidate_sha, 'Saved candidate changed during read-only validation'
        assert sha(source) == source_sha, 'Pinned E2 changed during validation'
        assert all(sha(Path(p)) == h for p, h in pinned_files.items()), 'Fixed baseline/protocol changed during validation'
        report['candidate_source_and_baseline_hashes_unchanged'] = True
        report['passed'] = True
    except Exception as error:
        report['error'] = str(error)
        report['traceback'] = traceback.format_exc()
        raise
    finally:
        cache.close()
        if measurements:
            np.savez_compressed(out / 'saved_candidate_measurements.npz', **measurements)
        dump(out / 'validation_report.json', report)
    print('C3_SAVED_VALIDATION_OK', args.label, str(candidate), flush=True)


if __name__ == '__main__':
    main()
