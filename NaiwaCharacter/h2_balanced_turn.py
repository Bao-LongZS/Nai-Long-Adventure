"""Low-degree final Y targets for the joint H2 surface solve.

This module only uses NumPy, so the same target construction runs in Blender
and in a small numerical diagnosis. It reads frozen source coordinates. It does
not modify a mesh, apply the contact mask, or declare visual acceptance.
Angular blending and actual XYZ feasibility constraints belong to the caller.
"""
import math

import numpy as np


def _ease(a, b, value):
    t = np.clip((np.asarray(value) - a) / (b - a), 0.0, 1.0)
    return t ** 3 * (10.0 + t * (-15.0 + 6.0 * t))


def _basis(t, degree=5):
    return np.column_stack([
        math.comb(degree, k) * t ** k * (1.0 - t) ** (degree - k)
        for k in range(degree + 1)
    ])


def _inputs(grid, zs, weights, strength):
    grid = np.asarray(grid, dtype=float)
    zs = np.asarray(zs, dtype=float)
    weights = np.asarray(weights, dtype=float)
    if (grid.ndim != 3 or grid.shape[2] != 3 or zs.ndim != 1
            or grid.shape[0] != len(zs) or weights.shape != (grid.shape[1],)):
        raise ValueError("Expected grid[Z,column,XYZ], zs[Z], weights[column]")
    if (len(zs) < 8 or np.any(np.diff(zs) <= 0)
            or not np.all(np.isfinite(grid)) or not np.all(np.isfinite(zs))
            or not np.all(np.isfinite(weights)) or not np.isfinite(strength)
            or strength < 0):
        raise ValueError("Finite coordinates, increasing zs and strength >= 0 required")
    if zs[0] > 2.06 or zs[-1] < 2.54:
        raise ValueError("Frozen source grid must include the head-neck target interval")
    return grid, zs, np.clip(weights, 0.0, 1.0), float(strength)


def _soft_box_fit(residual, initial, lower, upper, iterations=100):
    """Tiny projected Gauss-Newton solve; no SciPy dependency in Blender."""
    x = np.clip(np.asarray(initial, dtype=float), lower, upper)
    damping = 1e-3
    r = residual(x)
    cost = float(r @ r)
    for iteration in range(iterations):
        jac = np.empty((len(r), len(x)))
        for k in range(len(x)):
            step = 1e-5 * max(1.0, abs(float(x[k])))
            trial = x.copy()
            trial[k] += step
            jac[:, k] = (residual(trial) - r) / step
        hessian = jac.T @ jac
        rhs = jac.T @ r
        blocked = ((x <= lower + 1e-10) & (rhs > 0.0)) | ((x >= upper - 1e-10) & (rhs < 0.0))
        free = np.flatnonzero(~blocked)
        if len(free) == 0 or np.max(np.abs(rhs[free])) < 1e-7:
            break
        update = np.zeros_like(x)
        normal = hessian[np.ix_(free, free)]
        update[free] = np.linalg.solve(
            normal + damping * np.diag(np.maximum(np.diag(normal), 1.0)), rhs[free])
        accepted = False
        for factor in (1.0, .5, .25, .125, .0625, .03125):
            proposed = np.clip(x - factor * update, lower, upper)
            next_r = residual(proposed)
            next_cost = float(next_r @ next_r)
            if next_cost < cost - 1e-12:
                change = float(np.max(np.abs(proposed - x)))
                x, r, cost = proposed, next_r, next_cost
                damping = max(1e-8, damping * .5)
                accepted = True
                break
        if not accepted:
            damping *= 10.0
            if damping > 1e7:
                break
        elif change < 1e-9:
            break
    return x, {"iterations": iteration + 1, "soft_objective": cost}


def _angle_diagnosis(zz, beta):
    dz = np.diff(zz)
    # Arc length is dz/cos(beta), so angle change is assessed per arc length.
    arc_steps = dz / np.cos((beta[:-1] + beta[1:]) * .5)
    turn = np.abs(np.diff(beta))
    total = float(np.sum(turn))
    peak = float(np.max(turn / np.maximum(arc_steps, 1e-12)))
    return {"minimum_actual_angle_degrees": float(np.degrees(beta).min()),
            "maximum_actual_angle_degrees": float(np.degrees(beta).max()),
            "total_absolute_turn_degrees": float(np.degrees(total)),
            "net_angle_change_degrees": float(np.degrees(beta[-1] - beta[0])),
            "maximum_planned_absolute_curvature": peak}


def balanced_front_curves(grid, zs, front_weights, strength=1., lateral_turn=True,
                          contact_aware=False, distributed_contact=False):
    """Bounded quintic angle fields, fitted softly to broad final positions.

    All six Bernstein angle controls are bounded; the four interior controls
    can distribute the turn rather than compensating with +/-200 degree
    controls. Frozen endpoint tangents are retained. Position residuals remain
    soft and are reported, never forced by an oscillatory exact interpolation.
    Weights select active columns; the caller performs the angular blending.
    """
    if distributed_contact and not contact_aware:
        raise ValueError("distributed_contact requires contact_aware")
    if contact_aware:
        return _contact_front_curves(grid, zs, front_weights, strength, lateral_turn,
                                     distributed_contact=distributed_contact)
    grid, zs, weights, strength = _inputs(grid, zs, front_weights, strength)
    curves = grid[:, :, 1].copy()
    lower_z = 2.18
    upper_ends = np.full(grid.shape[1], 2.425)
    reports = []
    bounds = np.radians([-25.0, 76.0])
    if strength == 0:
        return curves, {"enabled": True, "identity_at_zero_strength": True,
                        "lower_Z": lower_z, "upper_end_Z": upper_ends.tolist(),
                        "angular_blend_delegated_to_caller": True,
                        "independent_saved_mesh_and_visual_review_required": True,
                        "columns": []}
    for j, weight in enumerate(weights):
        if weight < 1e-5:
            continue
        source = grid[:, j, 1]
        lateral = 0.0
        x = abs(float(np.interp(2.40, zs, grid[:, j, 0])))
        if lateral_turn:
            lateral = float(_ease(.060, .205, x))
        # The old Z2.390 endpoint lies inside the unwanted under-mouth turn.
        # It is not a protected feature. Connect above it; real mouth position
        # observations remain soft here and exact contact lives in the caller.
        upper_z = 2.425 + .020 * lateral
        upper_ends[j] = upper_z
        underface_z = 2.340 + .018 * lateral
        zz = np.linspace(lower_z, upper_z, 401)
        t = (zz - lower_z) / (upper_z - lower_z)
        basis = _basis(t)
        source_y = np.interp(zz, zs, source)
        source_slopes = np.gradient(source, zs)
        ends = np.arctan(-np.interp([lower_z, upper_z], zs, source_slopes))
        if np.any(ends < bounds[0]) or np.any(ends > bounds[1]):
            reports.append({"column": j, "weight": float(weight), "used": False,
                            "reason": "source endpoint tangent outside angle-control bounds"})
            continue
        anchors = np.array([lower_z, 2.260, underface_z, 2.400440216064])
        goals = np.interp(anchors, zs, source) + strength * np.array([.018, .018, .018, 0.0])
        anchor_scale = np.array([.008, .010, .006, .004])
        anchor_weight = np.array([1.0, 1.0, 1.0, float(1.0 - _ease(.060, .130, x))])
        source_beta = np.arctan(-np.interp(zz, zs, source_slopes))
        initial = np.linalg.lstsq(basis[:, 1:5],
                                 source_beta - basis[:, 0] * ends[0]
                                 - basis[:, 5] * ends[1], rcond=None)[0]

        def evaluate(x):
            controls = np.r_[ends[0], x, ends[1]]
            beta = basis @ controls
            increments = .5 * (np.tan(beta[:-1]) + np.tan(beta[1:])) * np.diff(zz)
            yy = source_y[-1] + np.r_[np.cumsum(increments[::-1])[::-1], 0.0]
            return controls, beta, yy

        def residual(x):
            controls, beta, yy = evaluate(x)
            positions = anchor_weight * (np.interp(anchors, zz, yy) - goals) / anchor_scale
            # Low-degree controls plus this regularizer suppress angle reversals
            # and concentrated turns without imposing an all-convex anatomy.
            smooth = .15 * np.diff(controls, 2) / np.radians(25.0)
            excursion = .4 * np.maximum(np.abs(yy[::10] - source_y[::10]) - .033,
                                       0.0) / .004
            return np.r_[positions, smooth, excursion]

        solved, fit_report = _soft_box_fit(residual, initial, bounds[0], bounds[1])
        controls, beta, yy = evaluate(solved)
        delta = yy - source_y
        transferred = np.interp(zs, zz, delta, left=float(delta[0]), right=0.0)
        # End slopes match the frozen source. Below the planned arc, a broad
        # quintic release brings its constant displacement back to zero.
        transferred *= _ease(2.06, lower_z, zs)
        curves[:, j] = source + transferred
        reports.append({"column": j, "weight": float(weight), "used": True,
                        "upper_Z": upper_z, "underface_anchor_Z": underface_z,
                        "position_residuals_Y": (np.interp(anchors, zz, yy) - goals).tolist(),
                        "source_endpoint_angles_degrees": np.degrees(ends).tolist(),
                        "angle_controls_degrees": np.degrees(controls).tolist(),
                        "delta_Y_range": [float(transferred.min()), float(transferred.max())],
                        **_angle_diagnosis(zz, beta), **fit_report})
    return curves, {"enabled": True,
                    "definition": "bounded quintic Bernstein angle field; soft broad final positions and angle regularization",
                    "lower_Z": lower_z, "lower_release_start_Z": 2.06,
                    "upper_end_Z": upper_ends.tolist(),
                    "lower_inset": .018 * strength, "underface_inset": .018 * strength,
                    "angle_control_bounds_degrees": [-25.0, 76.0],
                    "free_coefficients_per_column": 4,
                    "lateral_feature_endpoints": bool(lateral_turn),
                    "angular_blend_delegated_to_caller": True,
                    "independent_saved_mesh_and_visual_review_required": True,
                    "columns": reports}


def balanced_rear_curves(grid, zs, rear_weights, strength=1., rear_upper_z=2.54):
    """One quintic final rear curve with frozen position/tangent joins.

    Two interior coefficients fit broad rear positions and smoothness jointly.
    This replaces a source-plus-regional-lobes target; no prior deformation is
    accumulated. The caller blends the rear and front targets in one surface.
    """
    grid, zs, weights, strength = _inputs(grid, zs, rear_weights, strength)
    curves = grid[:, :, 1].copy()
    za, zb = 2.10, float(rear_upper_z)
    if not np.isfinite(zb) or zb < 2.54 or zb > zs[-1]:
        raise ValueError("rear_upper_z must lie between 2.54 and the frozen grid top")
    zz = np.linspace(za, zb, 401)
    basis = _basis((zz - za) / (zb - za))
    reports = []
    if strength == 0:
        return curves, {"enabled": True, "identity_at_zero_strength": True,
                        "lower_Z": za, "upper_Z": zb,
                        "angular_blend_delegated_to_caller": True,
                        "independent_saved_mesh_and_visual_review_required": True,
                        "columns": []}
    for j, weight in enumerate(weights):
        if weight < 1e-5:
            continue
        source = grid[:, j, 1]
        source_y = np.interp(zz, zs, source)
        slopes = np.interp([za, zb], zs, np.gradient(source, zs))
        controls = np.linspace(source_y[0], source_y[-1], 6)
        controls[1] = controls[0] + slopes[0] * (zb - za) / 5.0
        controls[4] = controls[5] - slopes[1] * (zb - za) / 5.0
        anchors = np.array([2.26, 2.43])
        goals = np.interp(anchors, zs, source) + strength * np.array([.012, .018])

        def evaluate(x):
            cc = controls.copy()
            cc[2:4] = x
            return cc, basis @ cc

        def residual(x):
            cc, yy = evaluate(x)
            positions = (np.interp(anchors, zz, yy) - goals) / .006
            smooth = .08 * np.diff(cc, 2) / .025
            excursion = .5 * np.maximum(np.abs(yy[::10] - source_y[::10]) - .033,
                                       0.0) / .004
            return np.r_[positions, smooth, excursion]

        initial = controls[2:4]
        solved, fit_report = _soft_box_fit(
            residual, initial, float(source_y.min() - .08), float(source_y.max() + .08))
        cc, yy = evaluate(solved)
        delta = yy - source_y
        transferred = np.interp(zs, zz, delta, left=0.0, right=0.0)
        curves[:, j] = source + transferred
        derivative = (5.0 / (zb - za)) * (_basis((zz - za) / (zb - za), 4) @ np.diff(cc))
        beta = np.arctan(-derivative)
        reports.append({"column": j, "weight": float(weight), "used": True,
                        "position_residuals_Y": (np.interp(anchors, zz, yy) - goals).tolist(),
                        "final_position_controls_Y": cc.tolist(),
                        "source_endpoint_slopes_Y_per_Z": slopes.tolist(),
                        "planned_endpoint_slopes_Y_per_Z": derivative[[0, -1]].tolist(),
                        "endpoint_position_residuals_Y": (yy[[0, -1]] - source_y[[0, -1]]).tolist(),
                        "delta_Y_range": [float(transferred.min()), float(transferred.max())],
                        **_angle_diagnosis(zz, beta), **fit_report})
    return curves, {"enabled": True, "definition": "single quintic final rear curve; soft broad positions with exact source position/tangent joins",
                    "lower_Z": za, "upper_Z": zb,
                    "anchor_Z": [2.26, 2.43], "anchor_inset_Y": [strength * .012, strength * .018],
                    "free_coefficients_per_column": 2,
                    "angular_blend_delegated_to_caller": True,
                    "independent_saved_mesh_and_visual_review_required": True,
                    "columns": reports}


def _contact_front_curves(grid, zs, front_weights, strength, lateral_turn,
                          distributed_contact=False):
    """Alternative recipe that agrees with source throughout the mouth guard.

    The central plan ends at the lower edge of the actual source mouth contact
    neighborhood; it does not ask a later contact mask to undo a sizeable Y
    error. Underface positions yield to this continuous join. Outer cheek goals
    retain the previous recipe, with a broad source-derived lateral transition.
    Both targets are independently constructed from the same frozen source.
    """
    grid, zs, weights, strength = _inputs(grid, zs, front_weights, strength)
    curves, previous = balanced_front_curves(
        grid, zs, weights, strength=strength, lateral_turn=lateral_turn,
        contact_aware=False)
    if strength == 0:
        previous["contact_aware"] = True
        if distributed_contact:
            previous["distributed_contact"] = True
        return curves, previous
    lower_z = 2.18
    upper_ends = np.asarray(previous["upper_end_Z"]).copy()
    reports = []
    bounds = np.radians([-25.0, 76.0])
    for j, weight in enumerate(weights):
        if weight < 1e-5:
            continue
        source = grid[:, j, 1]
        x = abs(float(np.interp(2.40, zs, grid[:, j, 0])))
        contact_fraction = float(1.0 - _ease(.060, .130, x))
        if contact_fraction <= 1e-5:
            continue
        lateral = float(_ease(.060, .205, x)) if lateral_turn else 0.0
        upper_z = 2.390 + .055 * lateral
        zz = np.linspace(lower_z, upper_z, 601)
        basis = _basis((zz - lower_z) / (upper_z - lower_z))
        source_y = np.interp(zz, zs, source)
        slopes = np.gradient(source, zs)
        ends = np.arctan(-np.interp([lower_z, upper_z], zs, slopes))
        if np.any(ends < bounds[0]) or np.any(ends > bounds[1]):
            reports.append({"column": j, "used": False,
                            "reason": "source endpoint tangent outside angle-control bounds"})
            continue
        anchors = np.array([lower_z, 2.260, 2.340 + .018 * lateral,
                            2.360 + .018 * lateral, 2.380 + .028 * lateral])
        goals = np.interp(anchors, zs, source) + strength * np.array([.018, .018, .008, .006, .001])
        anchor_scale = np.array([.010, .010, .006, .004, .001])
        initial = np.linalg.lstsq(
            basis[:, 1:5], np.arctan(-np.interp(zz, zs, slopes))
            - basis[:, 0] * ends[0] - basis[:, 5] * ends[1], rcond=None)[0]

        fixed_upper_control = None
        source_upper_angle_rate = None
        if distributed_contact:
            # N's last two free controls saturate at 76 degrees, while the
            # mouth-neighborhood end is only ~11 degrees. Matching its local
            # angle rate removes that late-turn freedom. This remains a target
            # prior; only real contact constraints are imposed on the mesh.
            source_beta = np.arctan(-slopes)
            source_upper_angle_rate = float(np.interp(
                upper_z, zs, np.gradient(source_beta, zs)))
            fixed_upper_control = np.clip(
                ends[1] - min(0., source_upper_angle_rate)
                * (upper_z - lower_z) / 5., ends[1], bounds[1])
            initial = initial[:3]
            # Negative Y fills the central underface outward. Previous inset
            # goals are deliberately not retained where they require a shelf
            # followed by an abrupt mouthward turn.
            goals = np.interp(anchors, zs, source) + strength * np.array(
                [.018, .012, -.012, -.001, -.001])
            anchor_scale = np.array([.010, .012, .008, .005, .002])
            angle_z = np.array([2.260, 2.300, 2.320, 2.340, 2.360, 2.380])
            angle_z += lateral * np.array([0., .006, .010, .018, .018, .028])
            angle_goals = np.radians([37., 51., 49., 43., 30., 17.])
            late = (zz >= 2.330 + .010 * lateral) & (zz <= upper_z - .004)

        def evaluate(parameters):
            controls = (np.r_[ends[0], parameters, ends[1]]
                        if fixed_upper_control is None else
                        np.r_[ends[0], parameters, fixed_upper_control, ends[1]])
            beta = basis @ controls
            increments = .5 * (np.tan(beta[:-1]) + np.tan(beta[1:])) * np.diff(zz)
            yy = source_y[-1] + np.r_[np.cumsum(increments[::-1])[::-1], 0.0]
            return controls, beta, yy

        def residual(parameters):
            controls, beta, yy = evaluate(parameters)
            positions = (np.interp(anchors, zz, yy) - goals) / anchor_scale
            smooth = .20 * np.diff(controls, 2) / np.radians(25.0)
            excursion = .4 * np.maximum(np.abs(yy[::10] - source_y[::10]) - .030,
                                       0.0) / .004
            if distributed_contact:
                broad_angles = .60 * (np.interp(angle_z, zz, beta)
                                      - angle_goals) / np.radians(8.)
                angle_rate = np.gradient(beta, zz)
                curvature = angle_rate * np.cos(beta)
                late_turn = .15 * np.maximum(np.abs(curvature[late][::8]) - 16.,
                                             0.) / 6.
                # The declining side may retain a smooth maximum, but should
                # not create another mouthward angle rebound after Z2.32.
                reversal = .20 * np.maximum(angle_rate[late][::8], 0.) / 5.
                return np.r_[positions, smooth, excursion, broad_angles,
                             late_turn, reversal]
            return np.r_[positions, smooth, excursion]

        solved, fit_report = _soft_box_fit(residual, initial, bounds[0], bounds[1])
        controls, beta, yy = evaluate(solved)
        delta = yy - source_y
        transferred = np.interp(zs, zz, delta, left=float(delta[0]), right=0.0)
        transferred *= _ease(2.06, lower_z, zs)
        planned = source + transferred
        # This blends two final positions from the same frozen source. It does
        # not deform a previous candidate or multiply the caller's angular fw.
        curves[:, j] = curves[:, j] * (1.0 - contact_fraction) + planned * contact_fraction
        if contact_fraction >= 1.0 - 1e-12:
            upper_ends[j] = upper_z
        reports.append({"column": j, "weight": float(weight), "used": True,
                        "contact_recipe_fraction": contact_fraction,
                        "source_guard_start_Z": upper_z,
                        "position_residuals_Y": (np.interp(anchors, zz, yy) - goals).tolist(),
                        "source_endpoint_angles_degrees": np.degrees(ends).tolist(),
                        "planned_endpoint_angles_degrees": np.degrees(beta[[0, -1]]).tolist(),
                        "endpoint_position_residuals_Y": [0.0],
                        "angle_controls_degrees": np.degrees(controls).tolist(),
                        "delta_Y_range": [float(transferred.min()), float(transferred.max())],
                        "near_contact_delta_Y": {
                            str(round(float(a), 6)): float(np.interp(a, zz, delta))
                            for a in [2.34, 2.36, 2.38, upper_z]},
                        **({"distributed_contact": True,
                            "free_angle_controls": 3,
                            "source_upper_angle_rate_radians_per_Z": source_upper_angle_rate,
                            "planned_upper_angle_rate_radians_per_Z": float(
                                5. * (ends[1] - fixed_upper_control)
                                / (upper_z - lower_z)),
                            "soft_angle_goal_Z": angle_z.tolist(),
                            "soft_angle_goals_degrees": np.degrees(angle_goals).tolist(),
                            "position_goal_delta_Y": [.018, .012, -.012, -.001, -.001],
                            "late_curvature_penalty_start_Z": float(2.330 + .010 * lateral),
                            "curvature_objective_only_not_validation_threshold": 16.}
                           if distributed_contact else {}),
                        **_angle_diagnosis(zz, beta), **fit_report})
    return curves, {"enabled": True, "contact_aware": True,
                    "definition": "bounded quintic final front angle field with exact source guard and soft underface goals",
                    "lower_Z": lower_z, "lower_release_start_Z": 2.06,
                    "central_source_guard_start_Z": 2.390,
                    "upper_end_Z": upper_ends.tolist(),
                    "angle_control_bounds_degrees": [-25.0, 76.0],
                    "free_coefficients_per_column": 3 if distributed_contact else 4,
                    "lateral_contact_recipe_fade_X": [.060, .130],
                    "angular_blend_delegated_to_caller": True,
                    "outer_recipe": previous,
                    "independent_saved_mesh_and_visual_review_required": True,
                    "columns": reports,
                    **({"distributed_contact": True,
                        "reason": "N residual central plateau and concentrated under-mouth turn; no saved-mesh acceptance implied",
                        "definition": "three-free-control quintic angle field, source position/tangent guard, fixed local upper angle rate and soft broad outward underface goals"}
                       if distributed_contact else {})}
