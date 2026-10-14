"""Cache-only local Idle1 triangle diagnostic at one fixed 45-degree screen patch.

This supplementary diagnostic never opens, saves, or renders a model. It does
not modify the global C3 measurement protocol or determine visual acceptance.
"""
from pathlib import Path
import hashlib
import json
import math
import sys

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parent
SCRIPTS = ROOT.parent.parent
sys.path.insert(0, str(SCRIPTS))
from c3_measurement_utils import arc_fit, angle80

RECT = [360., 425., 535., 640.]
RAY_PIXEL_STEP = .5
FIT_MARGIN_PIXELS = 32.
STEP, DEGREE, HALF_WINDOW = .0005, 3, .009


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def camera_frame(record):
    """Use the renderer's actual camera frustum, including AUTO portrait fit."""
    pose = record['pose']
    scene = bpy.context.scene
    scene.render.resolution_x, scene.render.resolution_y = pose['resolution']
    scene.render.resolution_percentage = 100
    scene.render.pixel_aspect_x = scene.render.pixel_aspect_y = 1.
    data = bpy.data.cameras.new('Cache-only diagnostic camera')
    data.type = pose['type']
    data.ortho_scale = pose['scale']
    data.lens = pose['lens']
    data.sensor_width = 36.
    data.sensor_fit = pose.get('sensor_fit', 'AUTO')
    data.shift_x = data.shift_y = 0.
    cam = bpy.data.objects.new('Cache-only diagnostic camera', data)
    scene.collection.objects.link(cam)
    cam.location = record['location']
    cam.rotation_euler = (Vector(pose['target']) - cam.location).to_track_quat('-Z', 'Y').to_euler()
    bpy.context.view_layer.update()
    frame = np.asarray([tuple(v) for v in data.view_frame(scene=scene)], float)
    rotation = np.asarray(cam.matrix_world, float)[:3, :3]
    return {'location': np.asarray(cam.location, float),
            'right': rotation[:, 0], 'up': rotation[:, 1], 'back': rotation[:, 2],
            'left': frame[:, 0].min(), 'right_bound': frame[:, 0].max(),
            'bottom': frame[:, 1].min(), 'top': frame[:, 1].max(),
            'resolution': pose['resolution'], 'frame_local_corners': frame.tolist()}


def screen_ray(frame, pixel):
    x, y = pixel
    width, height = frame['resolution']
    xx = frame['left'] + (x + .5) / width * (frame['right_bound'] - frame['left'])
    yy = frame['top'] - (y + .5) / height * (frame['top'] - frame['bottom'])
    return frame['location'] + xx * frame['right'] + yy * frame['up'], -frame['back']


def casts(tree, frame, pixels):
    points, normal, tri = [], [], []
    for pixel in pixels:
        origin, direction = screen_ray(frame, pixel)
        hit, n, tid, _ = tree.ray_cast(Vector(origin), Vector(direction), 20.)
        if hit is None:
            raise ValueError(f'Fixed local diagnostic ray missed body at {pixel}')
        points.append(tuple(hit)); normal.append(tuple(n)); tri.append(int(tid))
    return np.asarray(points, float), np.asarray(normal, float), np.asarray(tri, int)


def fixed_lines():
    x0, x1, y0, y1 = RECT
    lines = []
    for x in np.arange(x0, x1 + .001, 5.):
        lines.append((f'vertical_x{x:g}', [x, y0], [0., 1.], y1 - y0))
    for y in np.arange(y0, y1 + .001, 7.5):
        lines.append((f'horizontal_y{y:g}', [x0, y], [1., 0.], x1 - x0))
    # Two diagonal families cross the visually selected stripe loop region.
    for sign in (-1., 1.):
        for offset in (-15., 0., 15.):
            center = np.array(((x0 + x1) * .5 + offset, (y0 + y1) * .5))
            direction = np.array((1., sign)) / math.sqrt(2.)
            limits = []
            for axis, lo, hi in ((0, x0, x1), (1, y0, y1)):
                pair = sorted(((lo - center[axis]) / direction[axis],
                               (hi - center[axis]) / direction[axis]))
                limits.append(pair)
            low = max(q[0] for q in limits); high = min(q[1] for q in limits)
            lines.append((f'diagonal_sign{sign:g}_offset{offset:g}',
                          (center + low * direction).tolist(), direction.tolist(), high - low))
    return lines


def concentration(s, angle, window):
    cumulative = np.r_[0., np.cumsum(abs(np.diff(angle)))]
    if cumulative[-1] < 1e-12:
        return None
    endpoints = np.minimum(s + window, s[-1])
    amounts = np.interp(endpoints, s, cumulative) - cumulative
    return float(amounts.max() / cumulative[-1])


def line_measure(tree, frame, label, start, screen_direction, length):
    start, direction = np.asarray(start), np.asarray(screen_direction)
    t = np.arange(-FIT_MARGIN_PIXELS, length + FIT_MARGIN_PIXELS + RAY_PIXEL_STEP * .1,
                  RAY_PIXEL_STEP)
    pixels = start[None, :] + t[:, None] * direction
    points, normals, tids = casts(tree, frame, pixels)
    source_s = np.r_[0., np.cumsum(np.linalg.norm(np.diff(points, axis=0), axis=1))]
    # Each ray line lies in one fixed plane spanned by camera depth and screen
    # direction. Use those orthonormal plane coordinates with the SAME actual
    # triangle arc sampler, polynomial degree and half-window as global C3.
    in_plane = direction[0] * frame['right'] - direction[1] * frame['up']
    q = np.column_stack((np.zeros(len(points)), points @ frame['back'], points @ in_plane))
    fitted = arc_fit(q, STEP, DEGREE, HALF_WINDOW)
    eval_t = np.interp(fitted['s'], source_s, t)
    use = (eval_t >= 0.) & (eval_t <= length)
    s, angle, curvature = fitted['s'][use], fitted['angle'][use], fitted['curvature'][use]
    p = np.column_stack([np.interp(s, source_s, points[:, i]) for i in range(3)])
    abs_k = abs(curvature); peak = int(np.argmax(abs_k))
    total = float(abs(np.diff(angle)).sum())
    return {'label': label, 'screen_start': start.tolist(), 'screen_unit_direction': direction.tolist(),
            'screen_length_pixels': float(length), 'world_bbox': [p.min(0).tolist(), p.max(0).tolist()],
            'maximum_absolute_curvature': float(abs_k[peak]),
            'peak_world': p[peak].tolist(),
            'peak_screen': (start + eval_t[use][peak] * direction).tolist(),
            'signed_curvature_range': [float(curvature.min()), float(curvature.max())],
            'curvature_absolute_p95': float(np.percentile(abs_k, 95)),
            'curve_arc_length': float(s[-1] - s[0]),
            'total_absolute_turn_radians': total,
            'angle80': angle80(s, angle),
            'maximum_absolute_turn_fraction_in_arc_0_009': concentration(s, angle, .009),
            'maximum_absolute_turn_fraction_in_arc_0_018': concentration(s, angle, .018),
            'samples': {'arc': s.tolist(), 'world_points': p.tolist(),
                        'signed_curvature': curvature.tolist(), 'unwrapped_tangent_angle': angle.tolist()},
            'ray_count_including_fit_margin': len(t), 'fitted_sample_count_in_rect': len(s)}


def main():
    contract = ROOT / 'review/selected/B/protocol.json'
    global_protocol = ROOT / 'measurement_protocol.json'
    baseline = ROOT / 'baseline/baseline_arrays.npz'
    snap_file = ROOT / 'baseline/snapshot.json'
    candidate = ROOT / 'candidate_B/saved_candidate_measurements.npz'
    validation_file = ROOT / 'candidate_B/validation_report.json'
    snap = json.loads(snap_file.read_text(encoding='utf8'))
    validation = json.loads(validation_file.read_text(encoding='utf8'))
    cp = json.loads(contract.read_text(encoding='utf8'))
    gp = json.loads(global_protocol.read_text(encoding='utf8'))
    record = next(q for q in cp['files'] if q['file'] == 'B_reflection_three_quarter.png')
    assert validation['passed'] and cp['source_sha256'] == validation['candidate_sha256']
    assert sha(baseline) == snap['array_file_sha256']
    assert sha(global_protocol) == snap['measurement_protocol_sha256']
    assert (STEP, DEGREE, HALF_WINDOW) == (gp['sections']['resample_arc_step'],
                                         gp['sections']['polynomial_degree'],
                                         gp['sections']['fit_half_window_world'])
    frame = camera_frame(record)
    prefix = snap['objects']['Naiwa unified golden body']['array_prefix']
    with np.load(baseline) as a:
        baseline_points = a[prefix + '_Idle1_world'].copy()
        baseline_triangles = a[prefix + '_Idle1_triangles'].copy()
    with np.load(candidate) as a:
        candidate_points = a['Idle1_points'].copy()
        candidate_triangles = a['Idle1_triangles'].copy()
    assert np.array_equal(candidate_triangles, baseline_triangles)
    trees = {label: BVHTree.FromPolygons(p.tolist(), t.tolist(), all_triangles=True)
             for label, p, t in (('E2', baseline_points, baseline_triangles),
                                 ('B', candidate_points, candidate_triangles))}
    models = {}; comparisons = []
    lines = fixed_lines()
    for model, tree in trees.items():
        models[model] = [line_measure(tree, frame, *line) for line in lines]
    for a, b in zip(models['E2'], models['B']):
        compare = {'label': a['label'],
                   'E2_peak': a['maximum_absolute_curvature'], 'B_peak': b['maximum_absolute_curvature'],
                   'peak_ratio': b['maximum_absolute_curvature'] / a['maximum_absolute_curvature'],
                   'E2_p95': a['curvature_absolute_p95'], 'B_p95': b['curvature_absolute_p95'],
                   'total_absolute_turn_ratio': b['total_absolute_turn_radians'] / a['total_absolute_turn_radians']}
        if a['angle80'].get('applicable') and b['angle80'].get('applicable'):
            compare['angle80_width_ratio'] = b['angle80']['width'] / a['angle80']['width']
        for key in ('maximum_absolute_turn_fraction_in_arc_0_009',
                    'maximum_absolute_turn_fraction_in_arc_0_018'):
            compare[key + '_E2'] = a[key]; compare[key + '_B'] = b[key]
        comparisons.append(compare)
    x0, x1, y0, y1 = RECT
    grid_pixels = np.array([(x, y) for y in np.arange(y0, y1 + .01, 5.)
                            for x in np.arange(x0, x1 + .01, 5.)])
    grid = {}
    for model, tree in trees.items():
        p, n, t = casts(tree, frame, grid_pixels)
        grid[model] = {'pixels': grid_pixels.tolist(), 'world_points': p.tolist(),
                       'triangle_normals': n.tolist(), 'triangle_ids': t.tolist(),
                       'world_bbox': [p.min(0).tolist(), p.max(0).tolist()]}
    displacement = np.linalg.norm(np.asarray(grid['B']['world_points']) -
                                  np.asarray(grid['E2']['world_points']), axis=1)
    # Numbers are supplementary evidence. Do not encode an automatic pass:
    # stripe closure also depends on full smooth normals and illumination.
    summary = {'directional_line_count': len(lines),
               'maximum_peak_ratio_all_directions': max(q['peak_ratio'] for q in comparisons),
               'minimum_angle80_width_ratio_applicable': min(q['angle80_width_ratio']
                   for q in comparisons if 'angle80_width_ratio' in q),
               'maximum_peak_E2': max(q['maximum_absolute_curvature'] for q in models['E2']),
               'maximum_peak_B': max(q['maximum_absolute_curvature'] for q in models['B']),
               'maximum_fixed_screen_ray_hit_shift_world': float(displacement.max())}
    sources = {str(p.relative_to(ROOT)): sha(p) for p in
               (contract, global_protocol, baseline, snap_file, candidate, validation_file,
                Path(__file__))}
    sources['../../c3_measurement_utils.py'] = sha(SCRIPTS / 'c3_measurement_utils.py')
    report = {'schema_version': 1, 'kind': 'supplementary local actual-triangle diagnostic',
              'state': 'same saved/evaluated Idle1', 'sources_sha256': sources,
              'baseline_E2_sha256': snap['source_sha256'],
              'candidate_B_sha256': validation['candidate_sha256'],
              'model_projects_opened': False, 'models_saved_or_modified': False,
              'global_measurement_protocol_unchanged_sha256': sha(global_protocol),
              'fixed_local_protocol': {'rectangle_pixels_left_right_top_bottom': RECT,
                  'pixel_coordinates': 'top-left pixel index; center offset +0.5; renderer camera view_frame',
                  'camera_record': record, 'frame_local_corners': frame['frame_local_corners'],
                  'ray_pixel_step': RAY_PIXEL_STEP, 'fit_margin_pixels': FIT_MARGIN_PIXELS,
                  'arc_resample_step_world': STEP, 'polynomial_degree': DEGREE,
                  'fit_half_window_world': HALF_WINDOW,
                  'curve_plane': 'actual camera-depth / selected screen-line direction plane',
                  'angle80_definition': gp['sections']['angle80_definition'],
                  'same_parameters_and_screen_lines_for_both_states': True},
              'ray_grid_localization': grid, 'models': models, 'comparisons': comparisons,
              'summary': summary, 'automatic_acceptance': False,
              'limitations': [
                  'Local diagnostic is a fixed screen patch, distinct from the global anatomical section protocol; results must not be mixed.',
                  'Fixed 0.009 half-window measures broad actual geometry; it cannot exclude sub-window mesh-scale waviness alone.',
                  'Directional sections are not principal curvatures and do not model interpolation of smooth vertex normals or rendered reflection radiance.',
                  'Angle80 is relative to the local total absolute turn; smaller width alone does not establish a new crease.',
                  'A numerical claim of no new defect requires the selected gray/grazing/reflection evidence too.']}
    target = ROOT / 'selected_patch_diagnostic.json'
    target.write_text(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + '\n', encoding='utf8')
    print('SELECTED_PATCH_DIAGNOSTIC_OK', json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()
