"""Global minimum-jerk C2 quintics, bounded by monotone Bezier controls.

No SciPy is needed.  The unknowns are shared first and second derivatives at
the internal knots.  A small convex quadratic program minimizes the exact
integral of squared third derivative.  Ordered Bezier controls enforce each
span's monotonicity and convex hull, including both endpoint values.
"""
from functools import lru_cache
import math
import numpy as np

_D3 = np.array([[-1., 3., -3., 1., 0., 0.],
                [0., -1., 3., -3., 1., 0.],
                [0., 0., -1., 3., -3., 1.]])
_GRAM2 = np.array([[1/5, 1/10, 1/30],
                   [1/10, 2/15, 1/10],
                   [1/30, 1/10, 1/5]])


def _keys(anchors, values):
    x = np.asarray(anchors, dtype=float)
    y = np.asarray(values, dtype=float)
    if x.ndim != 1 or y.shape != x.shape or len(x) < 2:
        raise ValueError('Expected equally sized one-dimensional knot/value lists')
    if not np.isfinite(x).all() or not np.isfinite(y).all() or np.any(np.diff(x) <= 0):
        raise ValueError('Knots must be finite and strictly increasing; values finite')
    return tuple(x), tuple(y)


def _dual_qp(hessian, linear, constraint, bound):
    """Solve min .5*x'H*x+g'x, A*x<=b by exact dual coordinate descent."""
    size = len(linear)
    if not size:
        return np.zeros(0), {'sweeps': 0, 'dual_kkt_residual': 0., 'stationarity_residual': 0.}
    scale = 1 / np.sqrt(np.diag(hessian))
    h = hessian * scale[:, None] * scale[None, :]
    g = linear * scale
    a = constraint * scale[None, :]
    unconstrained = np.linalg.solve(h, -g)
    directions = np.linalg.solve(h, a.T)
    norms = np.sqrt(np.sum(a * directions.T, axis=1))
    a = a / norms[:, None]
    b = bound / norms
    directions = directions / norms[None, :]
    dual = np.zeros(len(b))
    current = unconstrained.copy()
    tolerance = 2e-10
    for sweep in range(1, 60001):
        for row in range(len(b)):
            violation = float(a[row] @ current - b[row])
            next_dual = max(0., dual[row] + violation)
            change = next_dual - dual[row]
            if change:
                current -= directions[:, row] * change
                dual[row] = next_dual
        if sweep % 10 == 0:
            # Recompute the primal to prevent accumulation error over sweeps.
            current = unconstrained - directions @ dual
            residual = a @ current - b
            active = dual > 1e-12
            kkt = max(float(np.maximum(residual, 0).max(initial=0)),
                      float(np.abs(residual[active]).max(initial=0)))
            if kkt <= tolerance:
                break
    else:
        raise RuntimeError('Minimum-jerk QP did not converge: KKT residual %g' % kkt)
    stationarity = h @ current + g + a.T @ dual
    return current * scale, {
        'sweeps': sweep, 'dual_kkt_residual': kkt,
        'stationarity_residual': float(np.abs(stationarity).max(initial=0)),
        'minimum_dual_multiplier': float(dual.min(initial=0)),
    }


@lru_cache(maxsize=64)
def _fit(anchor_key, value_key):
    original_x = np.asarray(anchor_key, dtype=float)
    original_y = np.asarray(value_key, dtype=float)
    x_span = float(original_x[-1] - original_x[0])
    y_scale = float(np.ptp(original_y))
    if y_scale == 0:
        control = np.repeat(original_y[:-1, None], 6, axis=1)
        control.setflags(write=False)
        return control, {'solver': 'constant', 'sweeps': 0,
                         'dual_kkt_residual': 0., 'stationarity_residual': 0.}
    x = (original_x - original_x[0]) / x_span
    y_origin = float(original_y.min())
    y = (original_y - y_origin) / y_scale
    n = len(x)
    changes = np.diff(y)
    fixed = {(0, 1), (0, 2), (n - 1, 1), (n - 1, 2)}
    for i, change in enumerate(changes):
        if change == 0:
            fixed.update((j, order) for j in (i, i+1) for order in (1, 2))
    for i in range(1, n-1):
        if changes[i-1] * changes[i] < 0:
            fixed.add((i, 1))
    variables = [(i, order) for i in range(1, n-1) for order in (1, 2)
                 if (i, order) not in fixed]
    lookup = {key: index for index, key in enumerate(variables)}
    count = len(variables)
    hessian = np.zeros((count, count))
    linear = np.zeros(count)
    forms = []
    inequalities, bounds = [], []
    for i, width in enumerate(np.diff(x)):
        base = np.array([y[i], y[i], y[i], y[i+1], y[i+1], y[i+1]])
        matrix = np.zeros((6, count))
        def add(knot, order, terms):
            column = lookup.get((knot, order))
            if column is not None:
                for row, coefficient in terms:
                    matrix[row, column] += coefficient
        add(i, 1, [(1, width/5), (2, 2*width/5)])
        add(i, 2, [(2, width*width/20)])
        add(i+1, 1, [(3, -2*width/5), (4, -width/5)])
        add(i+1, 2, [(3, width*width/20)])
        forms.append((base, matrix))
        jerk_base = _D3 @ base
        jerk_matrix = _D3 @ matrix
        factor = 3600 / width**5
        hessian += 2 * factor * jerk_matrix.T @ _GRAM2 @ jerk_matrix
        linear += 2 * factor * jerk_matrix.T @ _GRAM2 @ jerk_base
        direction = np.sign(changes[i])
        if direction:
            rows = -direction * np.diff(matrix, axis=0)
            limits = direction * np.diff(base)
            for row, limit in zip(rows, limits):
                if np.any(row):
                    inequalities.append(row)
                    bounds.append(limit)
                elif limit < 0:
                    raise RuntimeError('Inconsistent fixed monotonicity constraints')
    if count:
        result, solver = _dual_qp(hessian, linear, np.asarray(inequalities), np.asarray(bounds))
    else:
        result = np.zeros(0)
        solver = {'sweeps': 0, 'dual_kkt_residual': 0., 'stationarity_residual': 0.}
    normalized = np.array([base + matrix @ result for base, matrix in forms])
    control = normalized * y_scale + y_origin
    control[:, 0] = original_y[:-1]
    control[:, -1] = original_y[1:]
    # This is a strict rejection check, not a projection that would change C2.
    differences = np.diff(control, axis=1) * np.sign(np.diff(original_y))[:, None]
    if differences.min(initial=0) < -2e-11 * max(1., y_scale):
        raise RuntimeError('QP produced nonmonotone controls')
    control.setflags(write=False)
    solver.update(solver='global exact-integral minimum jerk; convex QP dual coordinate descent',
                  free_derivative_variables=count)
    return control, solver


def controls(anchors, values):
    """Return an (n-1, 6) copy of the fitted engineering-coordinate controls."""
    return _fit(*_keys(anchors, values))[0].copy()


def profile(z, anchors, values):
    """Evaluate the cached profile; hold each endpoint value outside its domain."""
    anchor_key, value_key = _keys(anchors, values)
    x = np.asarray(anchor_key)
    control, _ = _fit(anchor_key, value_key)
    z = np.asarray(z, dtype=float)
    if not np.isfinite(z).all():
        raise ValueError('Sample coordinates must be finite')
    segment = np.clip(np.searchsorted(x, z, side='right') - 1, 0, len(x)-2)
    t = np.clip((z - x[segment]) / (x[segment+1] - x[segment]), 0., 1.)
    work = control[segment].copy()
    # de Casteljau evaluates every endpoint exactly and is numerically stable.
    for degree in range(5, 0, -1):
        work = (1-t)[..., None] * work[..., :degree] + t[..., None] * work[..., 1:degree+1]
    return work[..., 0]


def diagnostic(anchors, values):
    """Return controls, true endpoint jets, continuity/monotonicity and QP residuals."""
    anchor_key, value_key = _keys(anchors, values)
    x = np.asarray(anchor_key)
    y = np.asarray(value_key)
    control, solver = _fit(anchor_key, value_key)
    widths = np.diff(x)
    left_d1 = 5 * (control[:, 1] - control[:, 0]) / widths
    right_d1 = 5 * (control[:, 5] - control[:, 4]) / widths
    left_d2 = 20 * (control[:, 2] - 2*control[:, 1] + control[:, 0]) / widths**2
    right_d2 = 20 * (control[:, 5] - 2*control[:, 4] + control[:, 3]) / widths**2
    signed = np.diff(control, axis=1) * np.sign(np.diff(y))[:, None]
    lower = np.minimum(y[:-1], y[1:])[:, None]
    upper = np.maximum(y[:-1], y[1:])[:, None]
    bound_violation = float(max(np.maximum(lower-control, 0).max(initial=0),
                                np.maximum(control-upper, 0).max(initial=0)))
    first_gap = float(np.abs(right_d1[:-1] - left_d1[1:]).max(initial=0))
    second_gap = float(np.abs(right_d2[:-1] - left_d2[1:]).max(initial=0))
    anchor_error = float(np.abs(profile(x, x, y) - y).max(initial=0))
    jerk = _D3 @ control.T
    objective = float(sum(3600 / h**5 * q @ _GRAM2 @ q for h, q in zip(widths, jerk.T)))
    boundary = [float(left_d1[0]), float(left_d2[0]), float(right_d1[-1]), float(right_d2[-1])]
    report = dict(solver, anchors=x.tolist(), values=y.tolist(), controls=control.tolist(),
                  minimum_signed_control_difference=float(signed.min(initial=0)),
                  maximum_bound_violation=bound_violation, maximum_anchor_error=anchor_error,
                  maximum_C1_join_gap=first_gap, maximum_C2_join_gap=second_gap,
                  endpoint_first_second_derivatives=boundary,
                  knot_first_derivatives=np.r_[left_d1, right_d1[-1]].tolist(),
                  knot_second_derivatives=np.r_[left_d2, right_d2[-1]].tolist(),
                  integrated_squared_third_derivative=objective)
    assert anchor_error == 0., 'Anchor interpolation is not exact'
    assert max(abs(v) for v in boundary) < 1e-7, 'Outer endpoint jets are not zero'
    assert first_gap < 1e-7 and second_gap < 1e-6, 'Internal C2 continuity failed'
    assert signed.min(initial=0) > -2e-11 and bound_violation < 2e-11, 'Control bound/ordering failed'
    return report
