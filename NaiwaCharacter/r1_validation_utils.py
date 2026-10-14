"""Independent measurements of the fixed N2 and saved R1 surfaces.

These routines read actual saved coordinates and triangles, never construction
weights or a builder's claimed displacement.  All distances are world units.
"""
import hashlib
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from head_neck_validation_utils import raw_record, shoulder_fixed, fit_sphere, sections
from geometry_diagnostic_utils import array, section_object, intersections

D0 = .117244812742
BODY = 'Naiwa unified golden body'
SHELL = 'Head and torso silhouette only'


def vertex_normals(obj):
    obj.data.update()
    normals = array(obj.data.vertices, 'normal', 3, np.float32).astype(float)
    matrix = np.asarray(obj.matrix_world, dtype=float)
    normals = normals @ np.linalg.inv(matrix[:3, :3])
    normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-20)
    return normals


def exact_local_coordinates(obj):
    return array(obj.data.vertices, 'co', 3, np.float32)


def normal_snapshot(obj):
    mesh = obj.data
    has_custom = bool(mesh.has_custom_normals)
    custom_digest = None
    if has_custom:
        values = array(mesh.corner_normals, 'vector', 3, np.float32)
        custom_digest = hashlib.sha256(values.tobytes()).hexdigest()
    return {'has_custom_normals': has_custom,
            'custom_corner_normals_sha256': custom_digest,
            'smooth_polygon_count': sum(p.use_smooth for p in mesh.polygons),
            'polygon_count': len(mesh.polygons),
            'render_hidden': bool(obj.hide_render)}


def source_protection(body, eye_meshes, mouth_obj):
    """Reconstruct exact fixed regions from frozen N2, with true oral distance."""
    shoulder_tree, shoulder_seeds, body_points = shoulder_fixed(body)
    body.data.calc_loop_triangles()
    triangles = array(body.data.loop_triangles, 'vertices', 3, np.int32)
    shoulder_triangles = triangles[shoulder_seeds[triangles].any(axis=1)]
    shoulder_surface = BVHTree.FromPolygons(body_points.tolist(), shoulder_triangles.tolist(), all_triangles=True)
    mouth_record = raw_record(mouth_obj)
    eyes = {side: fit_sphere(points) for side, points in eye_meshes.items()}
    return {'shoulder_tree': shoulder_tree, 'shoulder_seeds': shoulder_seeds,
            'shoulder_surface': shoulder_surface, 'eyes': eyes,
            'mouth_bvh': mouth_record['bvh'],
            'shoulder_triangle_count': len(shoulder_triangles)}


def protective_masks(points, context):
    eye = np.zeros(len(points), dtype=bool)
    for fit in context['eyes'].values():
        eye |= np.linalg.norm(points - np.asarray(fit['center']), axis=1) - fit['radius'] <= .026
    # Expensive continuous queries need only cover the actual edit band.
    scoped = np.flatnonzero((points[:, 2] >= 1.75) & (points[:, 2] <= 2.52))
    mouth_distance = np.full(len(points), np.inf)
    shoulder_distance = np.full(len(points), np.inf)
    original_shoulder_distance = np.full(len(points), np.inf)
    for index in scoped:
        point = Vector(points[index])
        mouth_distance[index] = context['mouth_bvh'].find_nearest(point)[3]
        shoulder_distance[index] = context['shoulder_surface'].find_nearest(point)[3]
        original_shoulder_distance[index] = context['shoulder_tree'].find(point)[2]
    return {'eye_installation_signed_sphere_0_026': eye,
            'mouth_skin_true_triangle_distance_0_012': mouth_distance <= .012,
            'shoulder_root_continuous_surface_0_012': shoulder_distance <= .012,
            'original_fixed_shoulder_seed_distance_0_012': original_shoulder_distance <= .012,
            'crown_above_2_635': points[:, 2] >= 2.635}, {
                'mouth_distance': mouth_distance,
                'shoulder_distance': shoulder_distance,
                'original_shoulder_distance': original_shoulder_distance}


def normal_delta_measurement(points, normals, delta):
    """Source normals define the specified projection; Z movement is forbidden."""
    normal_amount = np.einsum('ij,ij->i', delta, normals)
    tangent = delta - normal_amount[:, None] * normals
    return normal_amount, np.linalg.norm(tangent, axis=1)


def frozen_radial_sine(points, source_body):
    """Classify front/side/rear from actual source cross-section centers.

    The midpoint is sampled independently at each vertex's true source Z.
    This classification is unrelated to the displacement builder's weights.
    """
    result = np.zeros(len(points))
    indices = np.flatnonzero((points[:, 2] >= 2.03) & (points[:, 2] <= 2.50))
    zs, inverse = np.unique(points[indices, 2], return_inverse=True)
    centers = []
    for z in zs:
        front = source_body['bvh'].ray_cast(Vector((0., -3., float(z))), Vector((0., 1., 0.)), 6.)[0]
        rear = source_body['bvh'].ray_cast(Vector((0., 3., float(z))), Vector((0., -1., 0.)), 6.)[0]
        assert front is not None and rear is not None, ('Missing radial classification source center', z)
        centers.append((front.y + rear.y) / 2.)
    offset = points[indices, 1] - np.asarray(centers)[inverse]
    result[indices] = offset / np.maximum(np.hypot(points[indices, 0], offset), 1e-8)
    return result


def scalar_width(record, z):
    segments = intersections(record, 2, float(z))
    assert segments, ('Empty horizontal cut', z)
    points = np.asarray([p for segment in segments for p in segment['points']], float)
    return float(np.ptp(points[:, 0]))


def extended_sections(record):
    result = sections(record)
    result['longitudinal'] = [
        {'plane_X': x, 'body': section_object(record, 0, x, select_head=True)}
        for x in (-.18, -.12, 0., .12, .18)]
    samples = []
    zs = np.arange(2.030, 2.620 + .0001, .0005)
    for x in (-.18, -.12, 0., .12, .18):
        rows = []
        for z in zs:
            front = record['bvh'].ray_cast(Vector((float(x), -3., float(z))), Vector((0., 1., 0.)), 6.)[0]
            rear = record['bvh'].ray_cast(Vector((float(x), 3., float(z))), Vector((0., -1., 0.)), 6.)[0]
            if front is not None and rear is not None:
                rows.append([float(z), float(front.y), float(rear.y)])
        samples.append({'plane_X': float(x), 'columns': ['Z', 'front_Y', 'rear_Y'],
                        'step_Z': .0005, 'rows': rows})
    result['longitudinal_ray_samples'] = samples
    return result


def lightweight_sections(section_data):
    return [{key: row[key] for key in ('Z', 'front_Y', 'rear_Y', 'center_Y', 'depth_Y', 'width_X')}
            for row in section_data['horizontal']]


def compare_sections(old_record, new_record, old_sections, new_sections):
    rows = []
    limits = {2.12: .003, 2.22: .003, 2.31: .004}
    for old, new in zip(old_sections['horizontal'], new_sections['horizontal']):
        assert old['Z'] == new['Z']
        z = old['Z']
        width_change = new['width_X'] / old['width_X'] - 1.
        assert abs(width_change) <= .015 + 1e-7, ('Horizontal width changed more than 1.5%', z, width_change)
        row = {'Z': z, 'N2_front_Y': old['front_Y'], 'R1_front_Y': new['front_Y'],
               'N2_rear_Y': old['rear_Y'], 'R1_rear_Y': new['rear_Y'],
               'delta_front_Y': new['front_Y'] - old['front_Y'],
               'delta_rear_Y': new['rear_Y'] - old['rear_Y'],
               'width_fraction_change': width_change,
               'center_Y_change': new['center_Y'] - old['center_Y']}
        if z in limits:
            row['endpoint_tolerance'] = limits[z]
            assert abs(row['delta_front_Y']) <= limits[z] + 1e-7, ('Front endpoint tolerance', row)
            assert abs(row['delta_rear_Y']) <= limits[z] + 1e-7, ('Rear endpoint tolerance', row)
        rows.append(row)
    # Check intermediate cuts as well, not just the six advertised heights.
    widths = []
    for z in np.arange(2.07, 2.50 + .0001, .005):
        old = scalar_width(old_record, z)
        new = scalar_width(new_record, z)
        change = new / old - 1.
        assert abs(change) <= .015 + 1e-7, ('Intermediate horizontal width tolerance', float(z), change)
        widths.append({'Z': float(z), 'N2_width_X': old, 'R1_width_X': new,
                       'fraction_change': change})
    return {'six_actual_sections': rows, 'intermediate_width_step_Z': .005,
            'intermediate_width_checks': widths,
            'maximum_intermediate_width_fraction_change': max(abs(r['fraction_change']) for r in widths)}
