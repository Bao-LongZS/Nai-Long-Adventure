"""Head sculpt from one immutable current-version baseline; no accumulated transforms."""
import math
import bpy
import numpy as np
from mathutils import Vector

from eye_projection_measure import visible_metrics
from eye_muzzle_sculpt import skin, volume_eye, deep_seam
from longitudinal_surface_sculpt import coordinates, ease
from connection_sculpt import section


def plane_width(obj, height):
    points = coordinates(obj).astype(float)
    edges = np.empty(len(obj.data.edges) * 2, dtype=np.int32)
    obj.data.edges.foreach_get('vertices', edges)
    a, b = points[edges.reshape(-1, 2)].transpose(1, 0, 2)
    crossed = ((a[:, 2] <= height) & (b[:, 2] >= height)) | ((b[:, 2] <= height) & (a[:, 2] >= height))
    crossed &= np.abs(b[:, 2] - a[:, 2]) > 1e-12
    a, b = a[crossed], b[crossed]
    t = (height - a[:, 2]) / (b[:, 2] - a[:, 2])
    xs = a[:, 0] + t * (b[:, 0] - a[:, 0])
    return {'width': float(xs.max() - xs.min()), 'midline_x': float((xs.max() + xs.min()) / 2),
            'minimum_x': float(xs.min()), 'maximum_x': float(xs.max())}


def measure_baseline(body):
    eyes = {}
    for side in ('L', 'R'):
        eye = bpy.data.objects[side + ' concentric iris and pupil']
        metrics = visible_metrics(eye, body, step=.0003)
        eyes[side] = {'diameter': math.sqrt(4 * metrics['aperture']['area'] / math.pi),
                      'center_xz': list(eye['frontal_aperture_center']), 'visible_metrics': metrics}
    D0 = float(np.mean([e['diameter'] for e in eyes.values()]))
    eye_z = float(np.mean([e['center_xz'][1] for e in eyes.values()]))
    width = plane_width(body, eye_z)
    mouth = coordinates(bpy.data.objects['Short closed oral seam'])
    # The central seam vertices occur at X=0, with a symmetric opening in Z.
    center = mouth[np.abs(mouth[:, 0]) < 1e-7]
    mouth_z = float((center[:, 2].min() + center[:, 2].max()) / 2)
    pts = coordinates(body)
    return {'D0': D0, 'W0': width['width'], 'midline_x': width['midline_x'], 'eye_z': eye_z,
            'eyes': eyes, 'mouth_z': mouth_z, 'crown_z': float(pts[:, 2].max()),
            'D0_definition': 'Mean equivalent frontal outer-aperture diameter of both current green eyes; black pupil included inside the outer outline, not in the diameter definition.',
            'W0_definition': 'Exact edge intersections with the yellow body plane at mean eye-center Z; eye meshes excluded.',
            'axes': {'Z': [0, 0, 1], 'F': [0, -1, 0]},
            'targets': {'D1': 1.15 * D0, 'pupil_diameter': .53 * 1.15 * D0,
                        'eye_center_distance': 2 * 1.15 * D0, 'eye_line_head_width': 1.10 * width['width'],
                        'eye_projection': .20 * 1.15 * D0, 'muzzle_projection': .15 * 1.15 * D0,
                        'mouth_width': 1.25 * 1.15 * D0, 'mouth_opening': .06 * 1.15 * D0}}


def width_factor(z, rec):
    # C2 taper: peak at eye line, zero at mouth/shoulders and crown.
    peak = rec['eye_z']; low = rec['mouth_z'] - .035
    rise = ease(low, peak, z)
    fall = 1 - ease(peak, rec['crown_z'], z)
    # Above the mouth this delayed shoulder makes the mouth width change <2%.
    return .10 * rise * fall


def reshape_body(body, smooth_base, rec):
    assert not body.get('head_reshape_revision'), 'Load the pinned input before applying this sculpt.'
    old = coordinates(body)
    ref = coordinates(smooth_base)
    assert old.shape == ref.shape
    assert np.array_equal(old[:, [0, 2]], ref[:, [0, 2]]), 'Smooth-face reference topology/height must match the current body.'
    z = old[:, 2].astype(float)
    mx = rec['midline_x']
    zs = np.linspace(2.12, rec['crown_z'] - .001, 310)
    profiles = np.asarray([section(body, float(v)) for v in zs])
    front = np.interp(z, zs, profiles[:, 1]); rear = np.interp(z, zs, profiles[:, 2])
    depth = np.maximum((rear - front) / 2, .001)
    normalized_y = (old[:, 1] - (front + rear) / 2) / depth
    # Protect posterior core; temples and side poles receive full expansion.
    lateral_weight = 1 - ease(.12, .70, normalized_y)
    new = old.copy()
    expansion = width_factor(z, rec) * lateral_weight
    active = expansion > 0
    new[active, 0] = mx + (old[active, 0] - mx) * (1 + expansion[active])
    # Revert only the preceding muzzle/throat Y deformation, keeping current
    # colors, shoulders, belly, limbs, weights, pose and all Z coordinates.
    restore = ease(2.17, 2.22, z) * (1 - ease(2.58, 2.635, z))
    new[:, 1] += (ref[:, 1] - old[:, 1]) * restore
    front_weight = 1 - ease(-.85, -.05, normalized_y)
    D1 = rec['targets']['D1']; mz = rec['mouth_z']
    # Broad C2 ellipsoidal bump replaces the former sharp quintic beak.
    q = ((new[:, 0] - mx) / (1.65 * D1))**2 + ((z - mz) / (1.10 * D1))**2
    bump = np.maximum(0, 1 - q)**3
    projection = .15 * D1 * bump * front_weight
    new[:, 1] -= projection
    body.data.vertices.foreach_set('co', new.ravel()); body.data.update()
    smooth_mouth_y = skin(smooth_base, mx, mz)
    smooth_throat_y = skin(smooth_base, mx, mz - .9 * rec['D0'])
    # A shallow depression inside the actual dark opening, without an
    # independent lower-face pit or projecting lower lip.
    u = (new[:, 0] - mx) / (.625 * D1)
    arc = mz + .012 * D1 * u * u
    half = .03 * D1 * np.maximum(0, 1 - u * u)**.65
    v = (z - arc) / (half + .012 * D1)
    groove = .012 * D1 * np.maximum(0, 1 - u * u)**.7 * np.maximum(0, 1 - v * v)**3 * front_weight
    pre_carve_y = skin(body, mx, mz)
    new[:, 1] += groove
    body.data.vertices.foreach_set('co', new.ravel()); body.data.update()
    body['head_reshape_revision'] = True
    body['head_revision'] = 'Wider supported temples, larger separated volumetric eyes, continuous rounded short face'
    return {'changed_vertices': int(np.count_nonzero(np.any(new != old, axis=1))),
            'all_Z_exact': bool(np.array_equal(new[:, 2], old[:, 2])),
            'body_below_z_2_17_exact': bool(np.array_equal(new[z <= 2.17], old[z <= 2.17])),
            'crown_exact': bool(new[:, 2].max() == old[:, 2].max()),
            'eye_line_width': plane_width(body, rec['eye_z']),
            'mouth_line_width': plane_width(body, mz),
            'max_width_expansion_factor': float(width_factor(np.asarray([rec['eye_z']]), rec)[0]),
            'mouth_expansion_factor': float(width_factor(np.asarray([mz]), rec)[0]),
            'muzzle_F_projection_before_shallow_groove': smooth_mouth_y - pre_carve_y,
            'muzzle_F_projection_at_seam': smooth_mouth_y - skin(body, mx, mz),
            'throat_y_after': skin(body, mx, mz - .9 * rec['D0']),
            'throat_smooth_reference_y': smooth_throat_y,
            'shallow_mouth_groove': skin(body, mx, mz) - pre_carve_y}


def reshape_eyes_mouth(body, rec):
    D1 = rec['targets']['D1']; mx = rec['midline_x']
    report = {}
    for side, direction in (('L', -1), ('R', 1)):
        cx = mx + direction * D1
        cz = rec['eyes'][side]['center_xz'][1]
        spec = {'properties': {'frontal_aperture_center': [cx, cz], 'frontal_pupil_center': [cx, 0, cz]}}
        report[side] = volume_eye(bpy.data.objects[side + ' concentric iris and pupil'],
                                  bpy.data.objects[side + ' eye glint'], body, spec, D1, 0, rim_inset=0.0)
    report['mouth'] = deep_seam(body, D1, rec['mouth_z'])
    return report
