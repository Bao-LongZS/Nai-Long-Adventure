"""Bounded C2 world-Y section mapping on a pinned, unmodified input."""
import math
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from bounded_c2_profile import profile as bounded_profile, diagnostic as profile_diagnostic

D0 = .117244812742
NECK_Z = [2.030, 2.120, 2.220, 2.310, 2.350, 2.380]
NECK_FRONT = {'N1': [0, .045, .055, .044, .015, 0],
              'N2': [0, .060, .075, .060, .021, 0]}
NECK_REAR = {'N1': [0, .025, .025, .018, .006, 0],
             'N2': [0, .035, .035, .025, .008, 0]}
CROWN_REAR_Z = [2.380, 2.430, 2.520, 2.580, 2.635]
CROWN_REAR_Y = [0, .020, .020, .015, 0]
CROWN_FRONT_Z = [2.565, 2.600, 2.635]
CROWN_FRONT_Y = [0, -.020, 0]
BODY_NAME = 'Naiwa unified golden body'
SHELL_NAME = 'Head and torso silhouette only'

def array(items, key, width, dtype=np.float32):
    values = np.empty(len(items)*width, dtype=dtype)
    items.foreach_get(key, values)
    return values.reshape(-1, width)

def world_points(obj):
    points = array(obj.data.vertices, 'co', 3).astype(float)
    matrix = np.asarray(obj.matrix_world, dtype=float)
    return points @ matrix[:3, :3].T + matrix[:3, 3]

def ease(lo, hi, value):
    t = np.clip((np.asarray(value)-lo)/(hi-lo), 0, 1)
    return t*t*t*(10 + t*(-15 + 6*t))

def raw_bvh(obj, points):
    obj.data.calc_loop_triangles()
    triangles = array(obj.data.loop_triangles, 'vertices', 3, np.int32)
    return BVHTree.FromPolygons(points.tolist(), triangles.tolist(), all_triangles=True)

def midline_controls(obj, points):
    bvh = raw_bvh(obj, points)
    zs = np.arange(2.0, 2.6445+.0001, .0005)
    rows = []
    for z in zs:
        front = bvh.ray_cast(Vector((0, -3, float(z))), Vector((0, 1, 0)), 6)[0]
        rear = bvh.ray_cast(Vector((0, 3, float(z))), Vector((0, -1, 0)), 6)[0]
        assert front is not None and rear is not None, ('Missing midline', z)
        assert rear.y > front.y, ('Invalid section depth', z)
        rows.append([float(z), float(front.y), float(rear.y)])
    return np.asarray(rows, dtype=float)

def fit_eye(obj):
    points = world_points(obj)
    mean = points.mean(0)
    p = points-mean
    fit = np.linalg.lstsq(np.column_stack((2*p, np.ones(len(p)))), np.sum(p*p, axis=1), rcond=None)[0]
    center = mean+fit[:3]
    radius = math.sqrt(fit[3]+np.dot(fit[:3], fit[:3]))
    assert np.max(np.abs(np.linalg.norm(points-center, axis=1)-radius))/radius < 1e-4
    return center, radius

def context_from_source(body):
    """Freeze every sample before changing body or its historical proxy shell."""
    points = world_points(body)
    controls = midline_controls(body, points)
    arm_indices = {group.index for group in body.vertex_groups if 'Arm.' in group.name or 'Forearm.' in group.name}
    arm_weights = np.asarray([sum(g.weight for g in v.groups if g.group in arm_indices) for v in body.data.vertices])
    shoulder_seeds = (arm_weights >= .5) & (points[:, 2] >= 1.75)
    seeds = points[shoulder_seeds]
    assert len(seeds) > 100 and seeds[:, 2].max() < 2.11
    body.data.calc_loop_triangles()
    triangles = array(body.data.loop_triangles, 'vertices', 3, np.int32)
    patch_triangles = triangles[shoulder_seeds[triangles].any(axis=1)]
    assert len(patch_triangles) > 100
    # Distance to the continuous protected surface avoids scallops caused by
    # nearest individual seed vertices along an unevenly tessellated shoulder.
    shoulder_surface = BVHTree.FromPolygons(points.tolist(), patch_triangles.tolist(), all_triangles=True)
    assert max(shoulder_surface.find_nearest(Vector(p))[3] for p in seeds) < 1e-7
    seam = world_points(bpy.data.objects['Short closed oral seam'])
    return {'controls': controls, 'shoulder_surface': shoulder_surface, 'shoulder_seed_count': len(seeds),
            'shoulder_patch_triangles': len(patch_triangles),
            'shoulder_seed_max_Z': float(seeds[:, 2].max()),
            'eyes': [fit_eye(bpy.data.objects[s+' concentric iris and pupil']) for s in ('L', 'R')],
            'mouth_bounds': [seam.min(0), seam.max(0)]}

def guards(points, context, low, high):
    z = points[:, 2]
    height = ((z > low) & (z < high)).astype(float)
    shoulder = np.ones(len(points))
    near = np.flatnonzero((z > low) & (z < min(high, 2.27)))
    for i in near:
        distance = context['shoulder_surface'].find_nearest(Vector(points[i]))[3]
        shoulder[i] = float(ease(.012, .140, distance))
    # A 0.026 exterior safety buffer exceeds the 0.020 C1 displacement.
    # Thus newly moved skin cannot cut into an otherwise fixed globe.
    signed = np.minimum(*[np.linalg.norm(points-center, axis=1)-radius for center, radius in context['eyes']])
    eye = ease(.026, .050, signed)
    lo, hi = context['mouth_bounds']
    distance = np.linalg.norm(np.maximum(np.maximum(lo-points, points-hi), 0), axis=1)
    mouth = ease(.010, .030, distance)
    return {'height_guard': height, 'shoulder_guard': shoulder,
            'eye_guard': eye, 'mouth_guard': mouth}

def deform_object(obj, candidate, context):
    old_local = array(obj.data.vertices, 'co', 3)
    old = world_points(obj)
    z = old[:, 2]
    rows = context['controls']
    front = np.interp(z, rows[:, 0], rows[:, 1])
    rear = np.interp(z, rows[:, 0], rows[:, 2])
    t = np.clip((old[:, 1]-front)/np.maximum(rear-front, 1e-6), 0, 1)
    if candidate in ('N1', 'N2'):
        df = bounded_profile(z, NECK_Z, NECK_FRONT[candidate])
        dr = bounded_profile(z, NECK_Z, NECK_REAR[candidate])
        delta = (1-t)*df+t*dr
        low, high = NECK_Z[0], NECK_Z[-1]
    else:
        assert candidate == 'C1'
        rear_delta = bounded_profile(z, CROWN_REAR_Z, CROWN_REAR_Y)
        front_delta = bounded_profile(z, CROWN_FRONT_Z, CROWN_FRONT_Y)
        delta = rear_delta*ease(.30, .85, t) + front_delta*(1-ease(.10, .50, t))
        low, high = CROWN_REAR_Z[0], CROWN_REAR_Z[-1]
    protection = guards(old, context, low, high)
    region = np.prod(np.asarray(list(protection.values())), axis=0)
    delta *= region
    new_world = old.copy()
    new_world[:, 1] += delta
    matrix = np.asarray(obj.matrix_world, dtype=float)
    inverse = np.linalg.inv(matrix)
    new_local = (new_world@inverse[:3, :3].T+inverse[:3, 3]).astype(np.float32)
    # In this fixed delivery both editable surfaces have identity transforms.
    # Conversion is explicit; copying untouched columns keeps float32 exact.
    assert np.allclose(matrix, np.eye(4), atol=1e-10)
    new_local[:, [0, 2]] = old_local[:, [0, 2]]
    protected = region == 0
    new_local[protected] = old_local[protected]
    actual = (new_local-old_local).astype(float)
    changed = np.any(new_local != old_local, axis=1)
    assert np.array_equal(new_local[:, [0, 2]], old_local[:, [0, 2]])
    assert np.max(np.linalg.norm(actual, axis=1)) <= (.080 if candidate in ('N1', 'N2') else .020)+1e-7
    obj.data.vertices.foreach_set('co', new_local.ravel())
    obj.data.update()
    obj['head_neck_refinement_candidate'] = candidate
    report = {'changed_vertices': int(changed.sum()),
              'maximum_displacement': float(np.max(np.linalg.norm(actual, axis=1))),
              'minimum_delta_Y': float(actual[:, 1].min()), 'maximum_delta_Y': float(actual[:, 1].max()),
              'modified_bounds': [old[changed].min(0).tolist(), old[changed].max(0).tolist()] if changed.any() else None,
              'all_X_Z_exact': True, 'zero_region_vertices_exact': True,
              'mask_summary': {k: {'zero_count': int((v == 0).sum()), 'fade_count': int(((v > 0) & (v < 1)).sum())} for k, v in protection.items()}}
    mask = dict(protection, points_world=old, region_mask=region, delta_Y=actual[:, 1])
    return report, mask

def refine(candidate):
    body = bpy.data.objects[BODY_NAME]
    context = context_from_source(body)
    records = {}
    body_mask = None
    for name in (BODY_NAME, SHELL_NAME):
        if name not in bpy.data.objects:
            continue
        report, mask = deform_object(bpy.data.objects[name], candidate, context)
        records[name] = report
        if name == BODY_NAME:
            body_mask = mask
    bpy.context.view_layer.update()
    return {'candidate': candidate, 'D0': D0, 'surfaces': records,
            'source_midline_controls': context['controls'].tolist(),
            'curve_method': 'global minimum integrated squared third derivative; ordered quintic Bezier controls, shared C2 jets; zero outer displacement/first/second derivatives; no overshoot',
            'curve_diagnostics': {
                'front': profile_diagnostic(NECK_Z, NECK_FRONT[candidate]),
                'rear': profile_diagnostic(NECK_Z, NECK_REAR[candidate])
            } if candidate in ('N1', 'N2') else {
                'front': profile_diagnostic(CROWN_FRONT_Z, CROWN_FRONT_Y),
                'rear': profile_diagnostic(CROWN_REAR_Z, CROWN_REAR_Y)
            },
            'shoulder_protection': {'arm_total_seed_threshold': .5, 'seed_minimum_Z': 1.75,
                'seed_count': context['shoulder_seed_count'], 'seed_maximum_Z': context['shoulder_seed_max_Z'],
                'protected_patch_triangle_count': context['shoulder_patch_triangles'],
                'distance_method': 'nearest continuous protected shoulder surface triangle',
                'world_distance_exact_radius': .012, 'world_distance_fade_end': .140, 'guard_above_Z': 2.27},
            'eye_protection': {'signed_sphere_distance_exact': .026, 'fade_end': .050},
            'mouth_protection': {'world_bbox_distance_exact': .010, 'fade_end': .030},
            'shell_note': 'Historical unbound proxy, different topology; same frozen source-body section field and world protection masks applied.'}, body_mask

def parameters():
    return {'D0': D0, 'axes': {'X': 'right', 'Y': 'rear', 'Z': 'up', 'F': [0, -1, 0]},
            'neck_Z': NECK_Z, 'neck_front': NECK_FRONT, 'neck_rear': NECK_REAR,
            'C1_rear_Z': CROWN_REAR_Z, 'C1_rear_Y': CROWN_REAR_Y,
            'C1_front_Z': CROWN_FRONT_Z, 'C1_front_Y': CROWN_FRONT_Y,
            'midline_sampling_step_Z': .0005,
            'registration': {'scale': .41175526202447044, 'translation': [245.38813136509236, -17.159665982101984]}}
