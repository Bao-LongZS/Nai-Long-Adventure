"""E2 real spherical eyes and local skin contact, from a selected H2 only.

The aperture is an actual skin/sphere intersection.  The body keeps its vertex
indices, faces, UVs, colors and weights.  A local contact surface is blended
into the frozen H2 skin in a wide annulus.  No image mask or material-only
aperture is used.  A full sphere carries both green and black face colors.
"""
import math

import bpy
import numpy as np
from mathutils import Vector

from geometry_diagnostic_utils import array, face_point, mesh_record
from spherical_eye_sculpt import D0, sphere_mesh

BODY = 'Naiwa unified golden body'
SHELL = 'Head and torso silhouette only'


def ease(value):
    t = np.clip(value, 0., 1.)
    return t*t*t*(10.+t*(-15.+6.*t))


def fit_sphere(obj):
    p = array(obj.data.vertices, 'co', 3, np.float32).astype(float)
    matrix = np.asarray(obj.matrix_world)
    p = p@matrix[:3, :3].T+matrix[:3, 3]
    a = np.column_stack((2.*p, np.ones(len(p))))
    fit = np.linalg.lstsq(a, np.einsum('ij,ij->i', p, p), rcond=None)[0]
    center = fit[:3]
    radius = math.sqrt(float(fit[3]+center@center))
    residual = np.linalg.norm(p-center, axis=1)-radius
    return center, radius, float(np.max(np.abs(residual)))


def aperture_axes(side, axis_ratio):
    angle = math.radians(-39.7 if side == 'R' else 39.7)
    u = np.array([math.cos(angle), math.sin(angle)])
    v = np.array([-u[1], u[0]])
    return u, v, D0/2.*math.sqrt(axis_ratio), D0/2./math.sqrt(axis_ratio)


def contact_coordinates(xz, par):
    xz = np.asarray(xz)-np.asarray(par['aperture_center_xz'])
    u, v, a, b = aperture_axes(par['side'], par['target_axis_ratio'])
    radial = np.sqrt((xz@u/a)**2+(xz@v/b)**2)*D0/2.
    return radial


def contact_target(points, par, skin_y=None):
    """A local skin graph crosses the true sphere exactly at the aperture.

    At contact, body_y = sphere_y.  Inside, body_y is behind the globe;
    outside, body_y is in front of it.  The gap slope and wide quintic radial
    blend are solved together as the contact surface, rather than multiplying
    an unrelated old deformation mask after the fit.
    """
    p = np.asarray(points, dtype=float)
    skin = p[:, 1] if skin_y is None else np.asarray(skin_y, dtype=float)
    c = np.asarray(par['sphere_center'])
    radial = contact_coordinates(p[:, (0, 2)], par)
    r = D0/2.
    method = par.get('contact_method', 'surface_gap')
    if method in ('harmonic_rim', 'optimized_harmonic'):
        # Continue the fitted ring correction through the complete disk as
        # solid harmonics.  Its dominant order-1 term is a plane tilt, rather
        # than a radial sphere shell or an angle-dependent ridge at the center.
        u, v, a, b = aperture_axes(par['side'], par['target_axis_ratio'])
        offsets = p[:, (0, 2)]-np.asarray(par['aperture_center_xz'])
        theta = np.arctan2(offsets@v/b, offsets@u/a)
        coefficients = np.asarray(par['rim_fourier_coefficients'])
        correction = np.full(len(p), coefficients[0])
        rho = radial/r
        d = np.maximum(rho-1., 0.)
        for order in range(1, (len(coefficients)+1)//2):
            angular = (coefficients[2*order-1]*np.cos(order*theta)
                       +coefficients[2*order]*np.sin(order*theta))
            if (order > 3 and par.get('rim_high_order_extension')
                    == 'c2_matched_damped'):
                # Match rho**order in value and its first two derivatives at
                # rho=1.  Higher angular orders must decay in the outer skin
                # instead of amplifying a fitted rim ripple as a quadratic.
                continuation = np.exp(-order*d)*(1.+2.*order*d
                    +(2.*order*order-.5*order)*d*d)
            else:
                # Preserve the old three-order continuation exactly.
                continuation = 1.+order*d+.5*order*(order-1)*d*d
            radial_term = np.where(rho<=1., rho**order, continuation)
            correction += radial_term*angular
        # The current independent validator bounds all E2 skin changes to
        # radius .12.  End this broad continuation smoothly inside that limit,
        # including the ellipse's longest direction; do not clip a moved ring.
        direction_norm = np.linalg.norm(offsets,axis=1)/np.maximum(radial,1e-12)
        outer_radial = .119/np.maximum(direction_norm,1e-12)
        width = np.minimum(par['contact_transition_width'],outer_radial-r)
        weight = 1.-ease(np.maximum(radial-r,0.)/np.maximum(width,1e-6))
        # Release before the saved front-skin scope boundary; the true ring
        # lies below Y=-.31 and remains untouched by this outer boundary term.
        weight *= 1.-ease((skin+.25)/.07)
        return skin+weight*correction, weight
    if method == 'continuous_gap':
        # Solve one physical contact patch and continue its value, first and
        # second radial derivatives into the outer annulus.  Extending the
        # sphere square root to its equator creates an unrelated steep shelf;
        # switching to a fitted rim in a 0.006 band creates opposite slopes.
        # Here the outer skin is blended over the complete declared annulus.
        center = np.asarray(par['aperture_center_xz'])
        offsets = p[:, (0, 2)]-center
        direction = offsets/np.maximum(radial[:, None], 1e-12)
        sphere_offset = center-c[(0, 2), ]
        rim_vector = sphere_offset+r*direction
        rim_q = par['R']**2-np.einsum('ij,ij->i', rim_vector, rim_vector)
        if np.any(rim_q <= 0):
            raise ValueError('Constructed eye rim exceeds the true sphere')
        rim_root = np.sqrt(rim_q)
        along = np.einsum('ij,ij->i', rim_vector, direction)
        slope = along/rim_root-par['contact_gap_slope']
        second = (np.einsum('ij,ij->i', direction, direction)/rim_root
                  +along*along/rim_root**3-par['contact_gap_slope']/r)
        d = np.maximum(radial-r, 0.)
        continuation = c[1]-rim_root+slope*d+.5*second*d*d
        weight = 1.-ease(d/par['contact_transition_width'])
        outside = skin+weight*(continuation-skin)
        dxz = p[:, (0, 2)]-c[(0, 2), ]
        q = par['R']**2-np.einsum('ij,ij->i', dxz, dxz)
        cap = (c[1]-np.sqrt(np.maximum(q, 0.))
               +par['contact_gap_slope']*(r*r-radial*radial)/(2*r))
        return np.where(radial<=r, cap, outside), np.where(radial<=r,1.,weight)
    rim_result = None
    if method in ('rim_fit', 'hybrid_gap'):
        u, v, a, b = aperture_axes(par['side'], par['target_axis_ratio'])
        offsets = p[:, (0, 2)]-np.asarray(par['aperture_center_xz'])
        theta = np.arctan2(offsets@v/b, offsets@u/a)
        coefficients = np.asarray(par['rim_fourier_coefficients'])
        correction = np.full(len(p), coefficients[0])
        for order in range(1, (len(coefficients)+1)//2):
            correction += coefficients[2*order-1]*np.cos(order*theta)
            correction += coefficients[2*order]*np.sin(order*theta)
        distance = np.abs(radial-r)
        core = par.get('contact_core_width', .004)
        outer = par.get('contact_transition_width', .043)
        weight = 1.-ease((distance-core)/(outer-core))
        weight *= ease(radial/.021)
        rim_result = skin+weight*correction
        if method == 'rim_fit':
            return rim_result, weight
    delta_xz = p[:, (0, 2)]-c[(0, 2), ]
    q = par['R']**2-np.einsum('ij,ij->i', delta_xz, delta_xz)
    # The outer skin must be continued beyond the sphere's equator without
    # introducing a square-root derivative at q=0.  Tangent continuation at a
    # small positive q is hidden outside the constructed aperture.
    root_floor = par.get('hidden_sphere_continuation_root', .014)
    root = np.sqrt(np.maximum(q, root_floor**2))
    root = np.where(q < root_floor**2,
                    root_floor+(q-root_floor**2)/(2.*root_floor), root)
    sphere_y = c[1]-root
    # Quadratic gap is zero with the chosen slope at the rim and has a smooth
    # center. The entire hidden skin under the cap stays behind the sphere.
    target_y = sphere_y+par['contact_gap_slope']*(r*r-radial*radial)/(2*r)
    if method == 'hybrid_gap':
        # Enforce the physical gap over the exposed cap and narrow true rim.
        # Outside that rim, the fitted source correction continues the skin;
        # a sphere's equator is not extrapolated into the broad facial annulus.
        distance = np.maximum(radial-r, 0.)
        strict = 1.-ease((distance-.004)/.006)
        return rim_result+strict*(target_y-rim_result), strict
    distance = np.abs(radial-r)
    core = par.get('contact_core_width', .004)
    outer = par.get('contact_transition_width', .043)
    weight = np.where(radial<=r,1.,1.-ease((distance-core)/(outer-core)))
    result = skin+weight*(target_y-skin)
    return result, weight


def make_parameters(body_record, side, aperture_center, shift_fraction=.5,
                    radius_scale=1., target_axis_ratio=1.12,
                    contact_gap_slope=.9, transition=.043,
                    contact_method='rim_fit', depth_offset=0.,
                    installation_mode='source-fit', installation_outset=None,
                    installation_rise=None, rim_orders=3):
    if isinstance(rim_orders, bool) or not isinstance(rim_orders, int) or not 3 <= rim_orders <= 6:
        raise ValueError('E2 harmonic rim orders must be an integer from 3 through 6')
    if installation_mode not in ('source-fit', 'aperture-bias'):
        raise ValueError('Unknown E2 installation mode: '+str(installation_mode))
    if installation_mode == 'aperture-bias':
        if installation_outset is None or installation_rise is None:
            raise ValueError('aperture-bias requires installation_outset and installation_rise')
        if not all(math.isfinite(value) and value >= 0.
                   for value in (installation_outset, installation_rise)):
            raise ValueError('Aperture-relative installation offsets must be finite and nonnegative')
    elif installation_outset is not None or installation_rise is not None:
        raise ValueError('Installation offsets require aperture-bias mode')
    eye = bpy.data.objects[side+' concentric iris and pupil']
    c0, r0, residual = fit_sphere(eye)
    center = np.array(aperture_center, dtype=float)
    c = c0.copy()
    c[(0, 2), ] += shift_fraction*(center-c0[(0, 2), ])
    if installation_mode == 'aperture-bias':
        # Positive offsets make the visible cap face outward/upward relative
        # to the globe center.  They are not outward/upward globe translations.
        # Retain this prescribed XZ installation while fitting Cy and contact
        # skin below; the source-rim optimizer must not undo the offsets.
        sign = -1. if side == 'L' else 1.
        c[(0, 2), ] = center-np.array([sign*installation_outset, installation_rise])
    radius = r0*radius_scale
    u, v, a, b = aperture_axes(side, target_axis_ratio)
    theta = np.arange(256)*2.*math.pi/256.
    xz = center+np.cos(theta)[:, None]*a*u+np.sin(theta)[:, None]*b*v
    ring = np.array([face_point(body_record, float(x), float(z)) for x, z in xz])
    if ring.shape != (256, 3):
        raise ValueError('No continuous H2 skin at the proposed eye contact')
    installation_fit = None
    if installation_mode == 'aperture-bias':
        installation_fit = {
            'method': 'fixed aperture-relative spherical-cap XZ offsets; analytic true-ring Cy fit',
            'mode': installation_mode,
            'aperture_center_xz': center.tolist(),
            'installation_outset': float(installation_outset),
            'installation_rise': float(installation_rise),
            'side_sign': sign,
            'definition': 'Cx=aperture_x-side_sign*outset; Cz=aperture_z-rise',
            'fixed_center_xz': c[(0, 2), ].tolist(),
            'source_to_fixed_center_xz': (c[(0, 2), ]-c0[(0, 2), ]).tolist(),
            'XZ_optimized': False,
            'source_fit_shift_fraction_applied': False,
            'fixed_radius_scale': radius_scale,
        }
    elif contact_method == 'optimized_harmonic':
        # The instruction's 50/100 percent shifts are initial guesses.  Fit
        # the physical globe installation to the actual desired 3D skin ring,
        # eliminating Y analytically and retaining a 0.01 equator clearance.
        def objective(candidate):
            offsets = xz-candidate
            trial_q = radius**2-np.einsum('ij,ij->i',offsets,offsets)
            if trial_q.min() <= .0001:
                return float('inf'), None, None
            trial_root = np.sqrt(trial_q)
            trial_y = float(np.mean(ring[:,1]+trial_root))
            error = trial_y-trial_root-ring[:,1]
            return float(np.mean(error*error)), trial_y, error
        initial=c[(0,2), ].copy();best=initial.copy()
        value, _, initial_error=objective(best)
        initial_feasible = np.isfinite(value)
        if not initial_feasible:
            best=center.copy()
            value,_,initial_error=objective(best)
            if not np.isfinite(value):
                raise ValueError('No feasible globe installation at the desired eye rim')
        for step in (.004,.002,.001,.0005,.00025,.000125,.0000625,.00003125):
            for _ in range(40):
                options=[(best+step*np.array([i,j])) for i,j in
                         ((-1,-1),(-1,0),(-1,1),(0,-1),(0,1),(1,-1),(1,0),(1,1))]
                scores=[objective(option)[0] for option in options]
                index=int(np.argmin(scores))
                if scores[index] >= value-1e-14:
                    break
                best=options[index];value=scores[index]
        _,_,best_error=objective(best)
        c[(0,2), ]=best
        installation_fit={'method':'actual skin rim sphere-front Y residual minimization',
            'initial_center_xz':initial.tolist(),'optimized_center_xz':best.tolist(),
            'initial_center_feasible':bool(initial_feasible),
            'fallback_initial_center_xz':None if initial_feasible else center.tolist(),
            'initial_Y_rms':float(np.sqrt(np.mean(initial_error**2))),
            'optimized_Y_rms':float(np.sqrt(value)),
            'optimized_Y_max':float(np.max(np.abs(best_error))),
            'fixed_radius_scale':radius_scale,'minimum_ring_root':.010}
    dxz = xz-c[(0, 2), ]
    q = radius**2-np.einsum('ij,ij->i', dxz, dxz)
    if q.min() <= .0001:
        raise ValueError('Aperture reaches too close to globe equator')
    roots = np.sqrt(q)
    # Best Y burial for this real rim minimizes the squared contact change.
    fitted_depth = float(np.mean(ring[:, 1]+roots))
    c[1] = fitted_depth+depth_offset
    deviation = c[1]-roots-ring[:, 1]
    if installation_mode == 'aperture-bias':
        installation_fit.update({
            'fitted_depth_Y_before_offset': fitted_depth,
            'additional_depth_offset_Y': depth_offset,
            'actual_sphere_center': c.tolist(),
            'actual_Y_rms': float(np.sqrt(np.mean(deviation**2))),
            'actual_Y_max': float(np.max(np.abs(deviation))),
            'actual_minimum_ring_root': float(roots.min()),
        })
    columns = [np.ones(len(theta))]
    harmonic = contact_method in ('harmonic_rim','optimized_harmonic')
    orders = rim_orders if harmonic else 8
    for order in range(1, orders+1):
        columns.extend((np.cos(order*theta), np.sin(order*theta)))
    basis = np.column_stack(columns)
    coefficients = np.linalg.lstsq(basis, deviation, rcond=None)[0]
    fitted = basis@coefficients
    return {'side': side, 'R': radius, 'sphere_center': c.tolist(),
            'source_sphere_center': c0.tolist(), 'source_R': r0,
            'source_sphere_residual_max': residual,
            'sphere_center_shift_fraction': (shift_fraction if installation_mode == 'source-fit' else None),
            'radius_scale': radius_scale,
            'installation_mode': installation_mode,
            'installation_outset': installation_outset,
            'installation_rise': installation_rise,
            'aperture_center_xz': center.tolist(), 'target_D': D0,
            'target_axis_ratio': target_axis_ratio,
            'contact_gap_slope': contact_gap_slope,
            'contact_method': contact_method,
            'contact_depth_fit_Y': fitted_depth,
            'additional_depth_offset_Y': depth_offset,
            'installation_fit': installation_fit,
            'hidden_sphere_continuation_root': min(.028, .7*float(roots.min())),
            'rim_fourier_orders': orders,
            'rim_orders_requested': rim_orders,
            'rim_high_order_extension': ('c2_matched_damped'
                if harmonic and rim_orders > 3 else 'legacy_quadratic'),
            'rim_high_order_extension_definition': ('For order n>3 only: outside rho=1, '
                'exp(-n*d)*(1+2*n*d+(2*n*n-0.5*n)*d*d), d=max(rho-1,0). '
                'Value and first two radial derivatives match rho**n at the rim. '
                'Orders <=3 retain the historical quadratic continuation.'),
            'rim_fourier_coefficients': coefficients.tolist(),
            'rim_fourier_fit_residual_max': float(np.max(np.abs(fitted-deviation))),
            'contact_core_width': .004,
            'contact_transition_width': transition,
            'contact_circle_Y_fit_rms': float(np.sqrt(np.mean(deviation**2))),
            'contact_circle_Y_fit_max': float(np.max(np.abs(deviation))),
            'contact_ring_xz': xz.tolist(),
            'contact_ring_source_Y': ring[:, 1].tolist()}


def modify_skin(body, parameters):
    points = array(body.data.vertices, 'co', 3, np.float32)
    original = points.copy()
    normals = array(body.data.vertices, 'normal', 3, np.float32)
    result = points.astype(float)
    changed = np.zeros(len(points), bool)
    for par in parameters.values():
        center = np.asarray(par['aperture_center_xz'])
        distance = np.linalg.norm(points[:, (0, 2)]-center, axis=1)
        eligible = ((distance < D0/2.+par['contact_transition_width']+.01)
                    & (points[:, 1] < -.18) & (normals[:, 1] < -.02))
        ids = np.flatnonzero(eligible)
        if not len(ids):
            continue
        y, weight = contact_target(points[ids], par)
        # Both eye annuli refer to the same frozen source.  Wide annuli can
        # overlap at the bridge, so compose their residuals rather than letting
        # the second eye erase a first-eye correction where its own weight=0.
        result[ids, 1] += y-points[ids, 1]
        changed[ids] |= weight > 0.
    body.data.vertices.foreach_set('co', result.astype(np.float32).ravel())
    body.data.update()
    actual = array(body.data.vertices, 'co', 3, np.float32)-original
    return {'changed_vertices': int(np.count_nonzero(np.any(actual != 0, axis=1))),
            'maximum_abs_Y': float(np.max(np.abs(actual[:, 1]))),
            'X_Z_exactly_unchanged': bool(np.array_equal(original[:, (0, 2)],
                                                         result.astype(np.float32)[:, (0, 2)])),
            'scope': 'Local front skin eye annuli only',
            'original_points': original, 'actual_delta': actual}


def gaze_axis(side, outward_degrees, up_degrees):
    a = math.radians(outward_degrees)
    b = math.radians(up_degrees)
    sign = -1. if side == 'L' else 1.
    return np.array([sign*math.sin(a)*math.cos(b),
                     -math.cos(a)*math.cos(b), math.sin(b)])


def calibrate_pupil(cache, par, gaze, ratio=.53):
    c = np.asarray(par['sphere_center'])
    lo, hi = .005, math.radians(85.)
    for _ in range(30):
        angle = (lo+hi)/2.
        measured = cache.metrics(c, par['R'], angle, gaze)['pupil_ratio']
        if measured < ratio:
            lo = angle
        else:
            hi = angle
    return (lo+hi)/2.


def apply_eye(par, gaze, pupil_angle):
    side = par['side']
    eye = bpy.data.objects[side+' concentric iris and pupil']
    assert np.allclose(np.asarray(eye.matrix_world), np.eye(4), atol=1e-8), (
        'E2 sphere construction expects the frozen identity eye transform', side)
    old = eye.data
    colors = old.color_attributes['EyeColor']
    palette = array(colors.data, 'color', 4, np.float32)
    green = palette[np.flatnonzero(palette[:, 1] > .04)[0]].tolist()
    black = palette[np.flatnonzero(palette[:, 1] < .04)[0]].tolist()
    materials = list(old.materials)
    mesh = sphere_mesh(eye.name, par['sphere_center'], par['R'], gaze,
                       pupil_angle, green, black)
    for material in materials:
        mesh.materials.append(material)
    eye.data = mesh
    group = eye.vertex_groups.get('Torso') or eye.vertex_groups.new(name='Torso')
    group.add(list(range(len(mesh.vertices))), 1., 'REPLACE')
    if old.users == 0:
        bpy.data.meshes.remove(old)
    eye['sphere_center'] = par['sphere_center']
    eye['sphere_radius'] = par['R']
    eye['complete_spherical_eye'] = True
    eye['complete_volume_eye'] = True
    eye['spherical_eye_revision'] = 'E2 true sphere with fitted local skin contact'
    eye['pupil_axis'] = gaze.tolist()
    eye['pupil_cap_half_angle'] = float(pupil_angle)
    eye['pupil_to_iris_diameter'] = .53
    eye['frontal_aperture_center'] = par['aperture_center_xz']
    par['gaze_axis'] = gaze.tolist()
    par['pupil_cap_half_angle_radians'] = float(pupil_angle)
    par['pupil_cap_half_angle_degrees'] = math.degrees(pupil_angle)
    # High light follows the new pupil axis and remains on the actual sphere.
    u = np.array([1., 0., 0.])
    u -= gaze*float(gaze@u)
    u /= np.linalg.norm(u)
    v = np.cross(gaze, u)
    normal = gaze-.09*D0/par['R']*u+.13*D0/par['R']*v
    normal /= np.linalg.norm(normal)
    glint = bpy.data.objects[side+' eye glint']
    glint.location = np.asarray(par['sphere_center'])+(par['R']+.00025)*normal
    glint.rotation_euler = Vector(normal).to_track_quat('Y', 'Z').to_euler()
    par['glint_anchor_normal'] = normal.tolist()
    par['glint_contact_offset'] = .00025
    bpy.context.view_layer.update()
    return eye
