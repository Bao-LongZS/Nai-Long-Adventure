"""C3 cache-only joint mouth/neck solve on the final protected triangle field.

The optimization is a preparation step, not a saved candidate or an acceptance
test. It never opens the E2 project. The fixed E2 vertex masks participate in
the vertex basis before front-ray and curvature Jacobians are constructed.
"""
from pathlib import Path
import json
import sys
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, str(Path(__file__).resolve().parent))
from c3_curve_field import spline_basis
from c3_measurement_utils import measure_sections

BODY = 'Naiwa unified golden body'
LOW, HIGH, SPACING = 1.93, 2.410, .045
MODES = 3


def quintic(value):
    t = np.clip(value, 0., 1.)
    return t ** 3 * (10. + t * (-15. + 6. * t))


def scalar_basis(points, weights, centers, center_zs):
    """Small cubic Z and three broad even transverse modes, final mask first."""
    points = np.asarray(points, float)
    z = points[:, 2]
    bz, knots = spline_basis(z, LOW, HIGH, SPACING)
    center_y = np.interp(z, center_zs, centers[:, 1])
    radius = np.maximum(np.hypot(points[:, 0], points[:, 1] - center_y), 1e-15)
    front_cosine = -(points[:, 1] - center_y) / radius
    front = quintic((front_cosine + .15) / .55)
    # Lower/upper quintic envelopes have zero first and second derivatives.
    envelope = quintic((z - LOW) / .10) * quintic((HIGH - z) / .035)
    envelope[(z <= LOW) | (z >= HIGH)] = 0.
    lateral = np.tanh(points[:, 0] / .26) ** 2
    modes = np.column_stack((np.ones(len(points)), lateral, lateral ** 2))
    basis = (bz[:, :, None] * modes[:, None, :]).reshape(len(points), -1)
    basis *= (weights * envelope * front)[:, None]
    return basis, knots


def barycentric(point, triangle):
    a, b, c = triangle
    u, v, w = b - a, c - a, point - a
    uv, uu, vv = u @ v, u @ u, v @ v
    uw, vw = u @ w, v @ w
    determinant = uu * vv - uv * uv
    if abs(determinant) < 1e-24:
        raise ValueError('Degenerate triangle at a fixed front-ray observation')
    b1 = (vv * uw - uv * vw) / determinant
    b2 = (uu * vw - uv * uw) / determinant
    return np.array((1. - b1 - b2, b1, b2))


def build_ray_observations(points, triangles, weights, centers, center_zs, protocol):
    tree = BVHTree.FromPolygons(points.tolist(), triangles.tolist(), all_triangles=True)
    result = []
    for x in protocol['sections']['longitudinal_X'] + [-.231, -.194, .194, .231]:
        ranges = protocol['sections']['longitudinal_Z_ranges']
        lo, hi = ranges.get(str(float(x)), [1.95, 2.410])
        hi = min(hi, 2.425)
        zs = np.arange(lo, hi + .00005, .0005)
        hits, tids, bary, slopes = [], [], [], []
        for z in zs:
            hit, normal, triangle_id, _ = tree.ray_cast(
                Vector((float(x), -3., float(z))), Vector((0., 1., 0.)), 6.)
            if hit is None:
                raise ValueError(f'Missing immutable front ray X{x} Z{z}')
            hit = np.asarray(hit, float)
            triangle = points[triangles[triangle_id]]
            hits.append(hit)
            tids.append(triangles[triangle_id])
            bary.append(barycentric(hit, triangle))
            # This is the actual source triangle's dY/dZ, not a guessed curve.
            slopes.append(-float(normal[2]) / float(normal[1]))
        tids = np.asarray(tids, int)
        basis, knots = scalar_basis(points[tids.ravel()], weights[tids.ravel()],
                                    centers, center_zs)
        basis = basis.reshape(len(tids), 3, -1)
        jy = np.einsum('ij,ijk->ik', np.asarray(bary), basis)
        jacobian = np.column_stack((jy, -np.asarray(slopes)[:, None] * jy))
        result.append({'x': float(x), 'points': np.asarray(hits), 'jacobian': jacobian,
                       'triangle_ids': tids, 'barycentric': np.asarray(bary),
                       'source_slope': np.asarray(slopes)})
    return result, knots


def local_derivatives(source_s, source_points, jacobian, half_window=.009):
    first, second, jfirst, jsecond = [], [], [], []
    for index, s in enumerate(source_s):
        ids = np.flatnonzero(abs(source_s - s) <= half_window + 1e-12)
        if len(ids) < 5:
            ids = np.argsort(abs(source_s - s))[:5]
        t = (source_s[ids] - s) / half_window
        polynomial = np.column_stack((np.ones(len(t)), t, t * t, t ** 3))
        inverse = np.linalg.pinv(polynomial)
        d1 = inverse[1] / half_window
        d2 = 2. * inverse[2] / half_window ** 2
        first.append(d1 @ source_points[ids])
        second.append(d2 @ source_points[ids])
        jfirst.append(d1 @ jacobian[ids])
        jsecond.append(d2 @ jacobian[ids])
    return np.asarray(first), np.asarray(second), np.asarray(jfirst), np.asarray(jsecond)


def curvature_system(observations, baseline, protocol):
    systems = []
    low, high = protocol['sections']['mouth_underturn_Z_range']
    for observation in observations:
        key = str(float(observation['x']))
        if key not in baseline['longitudinal']:
            continue
        record = baseline['longitudinal'][key]
        source_points = np.asarray(record['samples']['actual_points_world'], float)
        source_s = np.asarray(record['samples']['arc'], float)
        zs = observation['points'][:, 2]
        sample_jacobian = np.column_stack([
            np.interp(source_points[:, 2], zs, column)
            for column in observation['jacobian'].T])
        d1, d2, jd1, jd2 = local_derivatives(source_s, source_points, sample_jacobian)
        speed_squared = d1[:, 1] ** 2 + d1[:, 2] ** 2
        curvature = ((d1[:, 2] * d2[:, 1] - d1[:, 1] * d2[:, 2])
                     / np.maximum(speed_squared ** 1.5, 1e-20))
        jacobian = ((d1[:, 2] / speed_squared ** 1.5)[:, None] * jd2
                    - (d2[:, 2] / speed_squared ** 1.5)[:, None] * jd1
                    - (3. * curvature * d1[:, 1] / speed_squared)[:, None] * jd1)
        angle_jacobian = (d1[:, 2] / speed_squared)[:, None] * jd1
        use = (source_points[:, 2] >= low) & (source_points[:, 2] <= high)
        # A single broad signed-curvature target redistributes the measured
        # turn; it is fitted through the already protected vertex Jacobian.
        mouth_s, mouth_curvature = source_s[use], curvature[use]
        distances = mouth_s[:, None] - mouth_s[None, :]
        kernel = np.exp(-.5 * (distances / .014) ** 2)
        broad = kernel @ mouth_curvature / kernel.sum(axis=1)
        goal = .70 * mouth_curvature + .30 * broad
        mouth_angle = np.unwrap(np.arctan2(d1[use, 1], d1[use, 2]))
        mouth_angle_j = angle_jacobian[use]
        increments = np.diff(mouth_angle)
        cumulative_j = np.vstack((np.zeros((1, sample_jacobian.shape[1])),
                                  np.cumsum(np.sign(increments)[:, None]
                                            * np.diff(mouth_angle_j, axis=0), axis=0)))
        width_record = record['angle80']
        start_arc, end_arc = width_record['start_arc'], width_record['end_arc']
        covered_j = np.array([np.interp(end_arc, mouth_s, col)
                              - np.interp(start_arc, mouth_s, col)
                              for col in cumulative_j.T])
        turn_density = np.abs(np.diff(mouth_angle)) / np.diff(mouth_s)
        midpoint = .5 * (mouth_s[1:] + mouth_s[:-1])
        endpoint_density = max(.5 * (np.interp(start_arc, midpoint, turn_density)
                                     + np.interp(end_arc, midpoint, turn_density)), 1e-3)
        width_jacobian = (.8 * cumulative_j[-1] - covered_j) / endpoint_density
        systems.append({'x': observation['x'], 'J': jacobian[use],
                        'baseline': mouth_curvature, 'goal': goal,
                        'peak': float(record['maximum_absolute_curvature']),
                        'z': source_points[use, 2], 's': mouth_s,
                        'angle80_width_J': width_jacobian,
                        'angle80_width': width_record['width']})
    return systems


def target_displacement(points):
    z, x = points[:, 2], points[:, 0]
    # +Y retracts the anterior neck. The three reference rows span side-neck
    # and shoulder; the target is intentionally broad across those positions.
    inward = .0073 * np.exp(-.5 * ((z - 2.215) / .145) ** 2)
    inward *= (1. - .13 * np.tanh(x / .26) ** 2) * quintic((2.380 - z) / .100)
    # An opposite, much smaller upper-mouth correction avoids the previous
    # inward scoop followed by a forced return at the frozen mouth contact.
    upper_outward = .0030 * np.exp(-.5 * ((z - 2.372) / .030) ** 2)
    goal = inward - upper_outward
    goal *= quintic((z - LOW) / .11) * quintic((HIGH - z) / .035)
    return goal


def solve_quadratic(rows, targets, row_weights, count, inequalities):
    matrix = np.vstack(rows)
    rhs = np.concatenate(targets)
    weight = np.concatenate(row_weights)
    scaled = matrix * weight[:, None]
    normal = scaled.T @ scaled + 1e-9 * np.eye(count)
    right = scaled.T @ (rhs * weight)
    unconstrained = np.linalg.solve(normal, right)
    coefficients = unconstrained.copy()
    j, limit = inequalities
    # Solve the small convex QP with a dual active set. The old alternating
    # penalty rows could oscillate between opposing near-contact constraints.
    inverse_j = np.linalg.solve(normal, j.T)
    active, history = [], []
    for iteration in range(250):
        violation = j @ coefficients - limit
        worst = int(np.argmax(violation))
        maximum = float(violation[worst])
        if maximum <= 2e-9:
            break
        if worst not in active:
            active.append(worst)
        for _ in range(120):
            ja = j[active]
            gram = ja @ inverse_j[:, active]
            lagrange = np.linalg.lstsq(gram, ja @ unconstrained - limit[active], rcond=1e-10)[0]
            if (lagrange >= -1e-10).all():
                coefficients = unconstrained - inverse_j[:, active] @ lagrange
                break
            del active[int(np.argmin(lagrange))]
            if not active:
                coefficients = unconstrained.copy()
                break
        if iteration % 20 == 0:
            history.append({'iteration': iteration, 'active_count': len(active),
                            'maximum_linear_constraint_violation': maximum})
    history.append({'iteration': iteration, 'active_count': len(active),
                    'maximum_linear_constraint_violation':
                    float(max(0., (j @ coefficients - limit).max()))})
    return coefficients, history


def evaluate_direction(points, weights, centers, center_zs, coefficients):
    direction = np.zeros_like(points, dtype=float)
    count = len(coefficients) // 2
    for start in range(0, len(points), 30000):
        end = min(len(points), start + 30000)
        b, _ = scalar_basis(points[start:end], weights[start:end], centers, center_zs)
        direction[start:end, 1] = b @ coefficients[:count]
        direction[start:end, 2] = b @ coefficients[count:]
    return direction


def prepare(root):
    root = Path(root)
    protocol = json.loads((root / 'measurement_protocol.json').read_text(encoding='utf-8'))
    snapshot = json.loads((root / 'baseline/snapshot.json').read_text(encoding='utf-8'))
    baseline = json.loads((root / 'baseline/measurements_Raw.json').read_text(encoding='utf-8'))
    with np.load(root / 'baseline/body_field_input.npz') as cache:
        points, triangles = cache['world'].copy(), cache['triangles'].copy()
        weights, frozen, eye = cache['weight'].copy(), cache['frozen'].copy(), cache['eye_local'].copy()
        centers, center_zs = cache['centers'].copy(), cache['zs'].copy()
    assert snapshot['protocol_frozen']
    assert points[eye, 2].min() > HIGH, 'Upper support overlaps the frozen eye-local set'
    observations, knots = build_ray_observations(points, triangles, weights, centers,
                                                center_zs, protocol)
    count = observations[0]['jacobian'].shape[1]
    scalar_count = count // 2
    systems = curvature_system(observations, baseline, protocol)
    rows, targets, row_weights = [], [], []
    for observation in observations:
        selected = np.arange(0, len(observation['points']), 8)
        p = observation['points'][selected]
        rows.append(observation['jacobian'][selected])
        targets.append(target_displacement(p))
        w = np.ones(len(p))
        w[(p[:, 2] >= 2.08) & (p[:, 2] <= 2.31)] = 3.
        row_weights.append(w)
    curvature_bounds_j, curvature_bounds_rhs = [], []
    for system in systems:
        # Match a broad curvature distribution while making any increased
        # mouth peak expensive through a separate linear bound.
        rows.append(system['J'])
        targets.append(system['goal'] - system['baseline'])
        row_weights.append(np.full(len(system['J']), .0035 / system['peak']))
        normalized = system['J'] / system['peak']
        curvature_bounds_j.extend((normalized, -normalized))
        curvature_bounds_rhs.extend((.995 - system['baseline'] / system['peak'],
                                     .995 + system['baseline'] / system['peak']))
        # First-order variation of the actual cumulative-absolute-turn width,
        # never a net-angle proxy. The central soft goal is +25% at B; lateral
        # planes discourage concentrating their measured turn while fitting.
        rows.append(system['angle80_width_J'][None, :])
        targets.append(np.array((.25 * system['angle80_width'],)))
        row_weights.append(np.array((10. if abs(system['x']) < .001 else 3.,)))
    # Coefficient regularization suppresses unsupported freedom and discourages
    # vertical displacement where simple +Y neck retraction already works.
    rows.append(np.eye(count))
    targets.append(np.zeros(count))
    row_weights.append(np.r_[np.full(scalar_count, .20), np.full(scalar_count, 5.)])
    for offset in (0, scalar_count):
        curvature_controls = np.zeros((scalar_count - 2 * MODES, count))
        for i in range(scalar_count - 2 * MODES):
            curvature_controls[i, offset + i] = 1.
            curvature_controls[i, offset + i + MODES] = -2.
            curvature_controls[i, offset + i + 2 * MODES] = 1.
        rows.append(curvature_controls)
        targets.append(np.zeros(len(curvature_controls)))
        row_weights.append(np.full(len(curvature_controls), .70))
    constraint_j = np.vstack(curvature_bounds_j)
    constraint_rhs = np.concatenate(curvature_bounds_rhs)
    coefficients, history = solve_quadratic(rows, targets, row_weights, count,
                                           (constraint_j, constraint_rhs))
    direction = evaluate_direction(points, weights, centers, center_zs, coefficients)
    direction[frozen | eye] = 0.
    maximum = np.linalg.norm(direction, axis=1).max()
    budget = protocol['budgets']['B'] * .999
    if maximum > budget:
        coefficients *= budget / maximum
        direction *= budget / maximum
    direction[frozen | eye] = 0.
    # Coordinate ties are frozen in the baseline mask; use one shared field,
    # so exact coincidences remain exact without a post-solve averaging patch.
    norm = np.linalg.norm(direction, axis=1)
    assert np.isfinite(direction).all() and norm.max() > 1e-8
    assert norm.max() <= budget + 1e-12
    assert not np.any(direction[frozen]) and not np.any(direction[eye])
    # One cache-only prediction on the actual changed triangle surface. This
    # is diagnostic; the parent checks independently after saving a candidate.
    predictions = {}
    for label, amplitude in (('A', .5), ('B', 1.)):
        predicted = measure_sections(points + amplitude * direction, triangles, protocol)
        predictions[label] = {}
        for key, rec in predicted['longitudinal'].items():
            base = baseline['longitudinal'][key]
            predictions[label][key] = {
                'baseline_peak': base['maximum_absolute_curvature'],
                'predicted_peak': rec['maximum_absolute_curvature'],
                'predicted_peak_ratio': rec['maximum_absolute_curvature'] / base['maximum_absolute_curvature'],
                'baseline_angle80_width': base['angle80'].get('width'),
                'predicted_angle80_width': rec['angle80'].get('width'),
                'predicted_angle80_width_ratio': rec['angle80'].get('width', 0.) / base['angle80'].get('width', 1.)}
    # The fixed reference row footprint is checked on the actual mesh,
    # including the Z component and triangle crossings, rather than claiming
    # that a raw +Y vertex value equals a fixed-scan contour improvement.
    reference_rows = []
    for label, amplitude in (('E2', 0.), ('A', .5), ('B', 1.)):
        tree = BVHTree.FromPolygons((points + amplitude * direction).tolist(),
                                    triangles.tolist(), all_triangles=True)
        row_values = []
        for row, x, z in ((220, .194, 2.306), (260, .194, 2.214), (300, .231, 2.122)):
            hit, _, _, _ = tree.ray_cast(Vector((x, -3., z)), Vector((0., 1., 0.)), 6.)
            row_values.append({'reference_row': row, 'world_X': x, 'world_Z': z,
                               'actual_front_world_Y': float(hit[1])})
        reference_rows.append({'state': label, 'observations': row_values})
    for record in reference_rows[1:]:
        for row, base_row in zip(record['observations'], reference_rows[0]['observations']):
            row['effective_delta_Y_from_E2'] = row['actual_front_world_Y'] - base_row['actual_front_world_Y']
    arrays = {'vertex_direction_world': direction, 'frozen_mask': frozen,
              'eye_local_mask': eye, 'coefficients': coefficients, 'knots': knots,
              'support_Z': np.array((LOW, HIGH)), 'lateral_mode_count': np.array(MODES)}
    report = {
        'method': 'one cubic-Z/even-transverse YZ vertex field; protected-mask triangle barycentric front-ray Jacobian; joint reference retraction and mouth-curvature solve',
        'field_control_count': count, 'Z_spacing': SPACING, 'Z_support': [LOW, HIGH],
        'transverse_even_modes': MODES, 'X_displacement': 'exact zero',
        'eye_local_displacement': 'exact zero through C2 upper support',
        'mouth_freeze_distance': protocol['scope']['mouth_freeze_distance'],
        'mouth_release_end': protocol['scope']['mouth_release_end'],
        'source': 'immutable cached E2 actual triangles; no E2 project reopened',
        'weights_in_vertex_Jacobian_before_optimization': True,
        'curvature_target_sigma_arc': .014, 'curvature_goal_baseline_fraction': .70,
        'curvature_linear_bound_relative_to_fixed_baseline_peak': .995,
        'one_target_fixed_for_A_B': True, 'same_target_and_scope_for_A_B': True,
        'scales': {'A': .5, 'B': 1.},
        'optimization_history': history, 'actual_triangle_predictions': predictions,
        'fixed_reference_row_footprint_actual_triangle_predictions': reference_rows,
        'unscaled_maximum_vertex_displacement': float(norm.max()),
        'unscaled_maximum_eye_local_displacement': float(norm[eye].max()),
        'delta_world_ranges': [[float(direction[:, i].min()), float(direction[:, i].max())] for i in range(3)],
        'visual_acceptance': 'pending saved-A key views; prediction is not acceptance',
        'solver_residuals': {'maximum_linear_curvature_violation':
                            float(max(0., (constraint_j @ coefficients - constraint_rhs).max()))}}
    return arrays, report


if __name__ == '__main__':
    import sys
    root = Path(sys.argv[sys.argv.index('--') + 1])
    arrays, report = prepare(root)
    (root / 'joint_solver_diagnostic.json').write_text(
        json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    print('JOINT_SOLVER_CACHE_DIAGNOSTIC', json.dumps(report, ensure_ascii=False), flush=True)
