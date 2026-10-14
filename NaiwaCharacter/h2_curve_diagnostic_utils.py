"""Actual section diagnostics for the R1 -> H2 -> E2 overall reconstruction.

All measurements use the final triangle surface. Arc length, not the author's
displacement function, parameterizes tangent angle and signed curvature. The
recorded runs and turning widths are diagnostics, never automatic visual passes.
"""
import numpy as np

from n2_round_diagnostic_utils import triangle_plane_segments, sample_axis_branches

Z_VALUES = np.round(np.arange(1.980, 2.6450001, .0005), 8)
X_PLANES = (0., -.04, .04, -.08, .08, -.12, .12, -.18, .18)
HORIZONTAL_Z = (1.98, 2.05, 2.12, 2.18, 2.22, 2.28, 2.31, 2.34,
                2.36, 2.38, 2.40, 2.44, 2.48, 2.52, 2.56, 2.60)


def longitudinal_branches(points, triangles, x=0., z_values=Z_VALUES):
    segments = triangle_plane_segments(points, triangles, 0, x)
    branch = sample_axis_branches(segments, z_values, 2, 1)
    return segments, branch


def contiguous_runs(mask):
    ix = np.flatnonzero(mask)
    if not len(ix):
        return []
    splits = np.flatnonzero(np.diff(ix) > 1) + 1
    return list(np.split(ix, splits))


def arc_diagnostic(y, z, step=.0005, half_window=.010):
    """Cubic fits on a uniformly sampled *actual* section's arc length.

    The Y,Z branch is oriented from bottom to top. Signed curvature is
    (Y'Z''-Z'Y'')/(Y'^2+Z'^2)^(3/2), and tangent atan2(Z',Y') is unwrapped.
    Values absent above an off-center section's own cap are excluded, not filled.
    """
    y, z = np.asarray(y, float), np.asarray(z, float)
    finite = np.isfinite(y) & np.isfinite(z)
    runs = contiguous_runs(finite)
    if not runs:
        raise ValueError('No actual section samples')
    # Body branches in the head region are connected. If a real gap exists,
    # keep the largest connected interval and record omitted samples explicitly.
    use = max(runs, key=len)
    raw = np.column_stack((y[use], z[use]))
    length = np.linalg.norm(np.diff(raw, axis=0), axis=1)
    arc = np.r_[0., np.cumsum(length)]
    unique = np.r_[True, np.diff(arc) > 1e-10]
    raw, arc = raw[unique], arc[unique]
    s = np.arange(0., arc[-1] + step / 2., step)
    s = s[s <= arc[-1] + 1e-10]
    point = np.column_stack([np.interp(s, arc, raw[:, j]) for j in (0, 1)])
    fitted = np.empty_like(point)
    derivative, second = np.empty_like(point), np.empty_like(point)
    for i, si in enumerate(s):
        take = np.abs(s - si) < half_window - 1e-10
        # One-sided fits at the real section boundary preserve the endpoint,
        # without extrapolating fictitious geometry above the crown.
        t = (s[take] - si) / half_window
        design = np.column_stack((np.ones_like(t), t, t*t, t*t*t))
        coefficients = np.linalg.lstsq(design, point[take], rcond=None)[0]
        fitted[i] = coefficients[0]
        derivative[i] = coefficients[1] / half_window
        second[i] = 2.*coefficients[2] / half_window**2
    speed = np.linalg.norm(derivative, axis=1)
    curvature = (derivative[:, 0]*second[:, 1] - derivative[:, 1]*second[:, 0]) / np.maximum(speed, 1e-12)**3
    angle = np.unwrap(np.arctan2(derivative[:, 1], derivative[:, 0]))
    return {'s': s, 'position_YZ': point, 'fitted_YZ': fitted,
            'tangent_angle_degrees': np.degrees(angle),
            'signed_curvature': curvature, 'first_derivative': derivative,
            'second_derivative': second,
            'method': {'sample_source': 'actual triangle-plane segment intersections',
                       'input_Z_step': .0005, 'arc_length_step': step,
                       'cubic_half_window_arc_length': half_window,
                       'orientation': 'bottom-to-top in (Y,Z)',
                       'boundary_policy': 'one-sided cubic fit, no cap extrapolation',
                       'omitted_nonfinite_sample_count': int(np.count_nonzero(~finite)),
                       'connected_runs_in_input': len(runs)}}


def shortest_turning_width(s, mass, fraction=.8):
    """Shortest contiguous arc interval containing a given |turn| mass."""
    if len(s) < 2:
        return None
    increments = .5*(mass[:-1]+mass[1:])*np.diff(s)
    cumulative = np.r_[0., np.cumsum(increments)]
    total = cumulative[-1]
    if total <= 1e-12:
        return None
    best = (float(s[-1]-s[0]), 0, len(s)-1)
    for start in range(len(s)):
        end = np.searchsorted(cumulative, cumulative[start]+fraction*total)
        if end >= len(s):
            break
        candidate = (float(s[end]-s[start]), start, int(end))
        if candidate[0] < best[0]:
            best = candidate
    width, a, b = best
    return {'fraction_of_absolute_turn': fraction, 'arc_width': width,
            's_start': float(s[a]), 's_end': float(s[b]),
            'total_absolute_turn_degrees': float(np.degrees(total))}


def summarize_arc(curve, bounds=(1.98, 2.61)):
    s = curve['s']; point = curve['position_YZ']; z = point[:, 1]
    theta = curve['tangent_angle_degrees']; k = curve['signed_curvature']
    take = (z >= bounds[0]) & (z <= bounds[1]) & np.isfinite(k)
    indices = np.flatnonzero(take)
    if len(indices) < 3:
        return {'Z_range': list(bounds), 'insufficient_samples': True}
    si, ki = s[indices], k[indices]
    widths = {str(frac): shortest_turning_width(si, np.abs(ki), frac) for frac in (.5, .8)}
    for record in widths.values():
        if record:
            record['Z_start'] = float(np.interp(record['s_start'], s, z))
            record['Z_end'] = float(np.interp(record['s_end'], s, z))
    def describe_run(run):
        a,b = int(run[0]),int(run[-1])
        return {'Z_start':float(z[a]), 'Z_end':float(z[b]),
                's_start':float(s[a]), 's_end':float(s[b]),
                'arc_length':float(s[b]-s[a]),
                'tangent_start_degrees':float(theta[a]), 'tangent_end_degrees':float(theta[b]),
                'absolute_turn_degrees':float(np.degrees(np.trapezoid(np.abs(k[run]),s[run]))),
                'maximum_absolute_curvature':float(np.abs(k[run]).max())}
    straight = [describe_run(run) for run in contiguous_runs(take & (np.abs(k)<3.)) if s[run[-1]]-s[run[0]]>=.040]
    sharp = [describe_run(run) for run in contiguous_runs(take & (np.abs(k)>=12.)) if s[run[-1]]-s[run[0]]>=.002]
    near_horizontal = [describe_run(run) for run in contiguous_runs(take & (np.abs(np.sin(np.radians(theta)))<.30)) if s[run[-1]]-s[run[0]]>=.012]
    transitions=[]
    for a in straight:
        for b in sharp:
            gap=max(b['s_start']-a['s_end'], a['s_start']-b['s_end'], 0.)
            if gap<=.035 and b['arc_length']<=.045:
                transitions.append({'straight_run':a,'sharp_run':b,'separation_arc_length':gap})
    worst=indices[np.argmax(np.abs(k[indices]))]
    return {'Z_range':list(bounds), 'actual_Z_range':[float(z[indices[0]]),float(z[indices[-1]])],
            'arc_length':float(si[-1]-si[0]),
            'tangent_start_end_degrees':[float(theta[indices[0]]),float(theta[indices[-1]])],
            'total_absolute_turn_degrees':float(np.degrees(np.trapezoid(np.abs(ki),si))),
            'net_signed_turn_degrees':float(theta[indices[-1]]-theta[indices[0]]),
            'maximum_absolute_curvature':{'value':float(abs(k[worst])), 'signed':float(k[worst]),
                                          'Z':float(z[worst]), 's':float(s[worst])},
            'turning_concentration_width':widths, 'long_low_curvature_runs':straight,
            'short_high_curvature_runs':sharp, 'near_horizontal_runs':near_horizontal,
            'straight_sharp_alternation_candidates':transitions,
            'thresholds':{'straight_max_abs_k':3.,'straight_min_arc':.040,
                          'sharp_min_abs_k':12.,'adjacent_gap_max':.035,
                          'short_turn_max_arc':.045,'near_horizontal_abs_sin_tangent_max':.30},
            'automatic_visual_acceptance':False}


def curve_summary(curve):
    return {'whole_head_neck_shoulder':summarize_arc(curve),
            'mouth_underface_connection':summarize_arc(curve,(2.31,2.44)),
            'natural_front_neck':summarize_arc(curve,(2.05,2.31)),
            'rear_head_neck_shoulder':summarize_arc(curve,(2.12,2.53)),
            'upper_shell':summarize_arc(curve,(2.43,2.61))}


def outer_profiles(points, triangles, z_values=None):
    """True outer X/Y bounds of actual horizontal triangle intersections."""
    if z_values is None:
        z_values=np.round(np.arange(1.98,2.6450001,.002),8)
    limits=[]
    for z in z_values:
        segments=triangle_plane_segments(points,triangles,2,float(z))
        if len(segments):
            p=segments.reshape(-1,3)
            limits.append([p[:,0].min(),p[:,0].max(),p[:,1].min(),p[:,1].max()])
        else:
            limits.append([np.nan]*4)
    a=np.asarray(limits)
    return {'Z':np.asarray(z_values), 'left_X':a[:,0], 'right_X':a[:,1],
            'front_Y':a[:,2], 'rear_Y':a[:,3]}
