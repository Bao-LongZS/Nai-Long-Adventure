"""One frozen low-degree normal field fitted to the actual E2 radial curves.

Only this target is prepared. A and (if justified) B evaluate the same field
with different global amplitudes, starting from the identical E2 mesh.
The geometric fit is a proposed direction, never visual acceptance.
"""
import json
import numpy as np


def spline_basis(values, lower, upper, spacing, degree=3):
    interior = np.arange(lower + spacing, upper - 1e-9, spacing)
    knots = np.r_[np.repeat(lower, degree + 1), interior,
                  np.repeat(upper, degree + 1)]
    x = np.clip(np.asarray(values), lower, upper - 1e-12)
    basis = ((x[:, None] >= knots[:-1]) & (x[:, None] < knots[1:])).astype(float)
    for order in range(1, degree + 1):
        count = len(knots) - order - 1
        left = knots[order:order + count] - knots[:count]
        right = knots[order + 1:order + count + 1] - knots[1:count + 1]
        updated = np.zeros((len(x), count))
        valid = left > 0
        updated[:, valid] += ((x[:, None] - knots[:count])[:, valid]
                             / left[valid] * basis[:, :count][:, valid])
        valid = right > 0
        updated[:, valid] += ((knots[order + 1:order + count + 1] - x[:, None])[:, valid]
                             / right[valid] * basis[:, 1:count + 1][:, valid])
        basis = updated
    return basis, knots


def fair_curve(points, spacing=.055):
    """Low-degree arc-length fit, with endpoint positions and tangents retained."""
    s = np.r_[0., np.cumsum(np.linalg.norm(np.diff(points, axis=0), axis=1))]
    b, knots = spline_basis(s, 0., s[-1], spacing)
    second = np.gradient(np.gradient(b, s, axis=0, edge_order=2),
                         s, axis=0, edge_order=2)
    # Three exact endpoint observations at either end retain broad joining
    # tangents, while avoiding fictitious data beyond the real crown.
    ids = np.r_[np.arange(3), np.arange(len(points) - 3, len(points))]
    equality = b[ids]
    normal = (b.T @ b + .020 ** 4 * (second.T @ second)
              + 1e-10 * np.eye(b.shape[1]))
    system = np.block([[normal, equality.T],
                       [equality, np.zeros((len(ids), len(ids)))]])
    right = np.vstack((b.T @ points, points[ids]))
    coefficients = np.linalg.lstsq(system, right, rcond=1e-12)[0][:b.shape[1]]
    return b @ coefficients, {'arc_length': float(s[-1]),
                              'control_count': int(b.shape[1]),
                              'endpoint_position_tangent_observations': ids.tolist()}


def prepare(grid, normals, zs, theta, weight_grid):
    """Fit the masked normal correction as one periodic continuous surface.

    Mouth, eye and outer release weights participate in the fit. They are not
    appended after choosing an unconstrained curve. XYZ are allowed through the
    real surface normal, and the lower/front-neck concavity remains anatomical.
    """
    nz, nt, _ = grid.shape
    assert normals.shape == grid.shape and weight_grid.shape == (nz, nt)
    bz, knots = spline_basis(zs, zs[0], zs[-1], .050)
    proposed = np.empty_like(grid)
    reports = []
    for column in range(nt):
        proposed[:, column], report = fair_curve(grid[:, column])
        reports.append(report)
    normal_goal = np.einsum('ijk,ijk->ij', proposed - grid, normals)
    # A shared finite-degree lateral basis gives a single head-neck target,
    # rather than unrelated changes on a few horizontal slices.
    angular = np.column_stack([np.ones(nt)] +
        [function(order * theta) for order in range(1, 5)
         for function in (np.cos, np.sin)])
    # The fixed target is fitted with the true contact/outer release in each
    # observation. Small coefficient regularization suppresses unsupported
    # excursions inside preserved regions without changing their scope.
    count = bz.shape[1] * angular.shape[1]
    normal = np.zeros((count, count))
    rhs = np.zeros(count)
    for column in range(nt):
        b = (bz[:, :, None] * angular[column][None, None, :]).reshape(nz, count)
        b *= weight_grid[:, column, None]
        desired = normal_goal[:, column] * weight_grid[:, column]
        normal += b.T @ b
        rhs += b.T @ desired
    controls = np.linalg.solve(normal + .002 * np.eye(count), rhs)
    controls = controls.reshape(bz.shape[1], angular.shape[1])
    free_field = bz @ controls @ angular.T
    delta = normals * (free_field * weight_grid)[:, :, None]
    return {'coefficients': controls, 'knots': knots, 'zs': zs, 'theta': theta,
            'grid_delta': delta, 'target_grid': proposed}, {
        'method': 'actual radial curves fitted in arc length; one cubic longitudinal and degree-four periodic normal field',
        'curve_spacing_arc': .055, 'curve_fairing_length': .020,
        'field_spacing_Z': .050,
        'angular_order': 4, 'source': 'frozen actual E2 triangle surface',
        'independent_XYZ_allowed_via_surface_normals': True,
        'same_target_and_scope_for_A_B': True,
        'release_weights_in_fit': True,
        'full_front_neck_uniform_translation': False,
        'unscaled_maximum_grid_displacement': float(np.linalg.norm(delta, axis=2).max()),
        'curve_fits': reports, 'visual_acceptance': 'pending'}


def evaluate(points, normals, weight, centers, field):
    bz, knots = spline_basis(points[:, 2], field['zs'][0], field['zs'][-1], .050)
    assert np.allclose(knots, field['knots'])
    center = np.interp(points[:, 2], field['zs'], centers[:, 1])
    # The frozen exporter uses theta=0 toward the anatomical front (-Y).
    theta = np.arctan2(points[:, 0], -(points[:, 1] - center))
    angular = np.column_stack([np.ones(len(points))] +
        [function(order * theta) for order in range(1, 5)
         for function in (np.cos, np.sin)])
    scalar = np.einsum('ij,ij->i', bz @ field['coefficients'], angular)
    delta = normals * (scalar * weight)[:, None]
    delta[weight == 0] = 0.
    return delta
