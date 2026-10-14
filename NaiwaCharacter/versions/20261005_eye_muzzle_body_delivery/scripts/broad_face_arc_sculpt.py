"""Broaden the existing muzzle and distribute the throat/chest turn in space."""
import bpy
import numpy as np
from mathutils import Vector
from short_round_face_sculpt import profiles, refit_features
from longitudinal_surface_sculpt import coordinates, ease
from neck_direction_sculpt import quintic

ROOT_Z = 1.98
FRONT_JOIN_Z = 2.495
WIDTH_JOIN_Z = 2.425
THROAT_Z = 2.16
MOUTH_Z = 2.359

def reshape(obj):
    old = coordinates(obj)
    zs, source, at, jets = profiles(obj)
    f0, df0, ddf0 = jets(ROOT_Z, 1)
    f1, df1, ddf1 = jets(FRONT_JOIN_Z, 1)
    # Retain the short muzzle, soften its apex, and ease the throat forward.
    throat = at(2.185, 1) - 0.012
    mouth = at(MOUTH_Z, 1) + 0.006
    def front(z):
        return np.where(z <= THROAT_Z,
            quintic(z, ROOT_Z, THROAT_Z, f0, throat, df0, 0, ddf0, -2.0),
            np.where(z <= MOUTH_Z,
                quintic(z, THROAT_Z, MOUTH_Z, throat, mouth, 0, 0, -2.0, 3.2),
                quintic(z, MOUTH_Z, FRONT_JOIN_Z, mouth, f1, 0, df1, 3.2, ddf1)))
    z = old[:, 2].astype(float)
    sw = np.interp(z, zs, source[:, 0])
    sf = np.interp(z, zs, source[:, 1])
    sr = np.interp(z, zs, source[:, 2])
    theta = np.arctan2((old[:, 1] - (sf+sr)/2)/((sr-sf)/2), old[:, 0]/sw)
    anterior = np.maximum(0, -np.sin(theta))
    # C2 angular mask also has zero derivatives at the side of the neck.
    angular = ease(0, 1, anterior)
    shoulder = ease(.29, .395, np.abs(old[:, 0]))*ease(-.20, -.12, old[:, 1])*(1-ease(.19, .27, old[:, 1]))
    feather = 1-shoulder*(1-ease(2.09, 2.205, z))
    fy = (z > ROOT_Z) & (z < FRONT_JOIN_Z)
    wx = (z > ROOT_Z) & (z < WIDTH_JOIN_Z)
    new = old.copy()
    target = front(np.clip(z, ROOT_Z, FRONT_JOIN_Z))
    new[fy, 1] += ((target-sf)*angular*feather)[fy]
    # Spread the muzzle laterally without increasing the sagittal protrusion.
    cheek = 4*anterior*anterior*(1-anterior*anterior)
    face_mask = ease(2.16, 2.29, z)*(1-ease(2.40, FRONT_JOIN_Z, z))
    new[fy, 1] -= (.014*cheek*face_mask*feather)[fy]
    # A long volume adjustment fills the abrupt lower-neck flare.
    width_mask = ease(ROOT_Z, 2.22, z)*(1-ease(2.24, WIDTH_JOIN_Z, z))
    new[wx, 0] = old[wx, 0]*(1+(.011*width_mask[wx]/sw[wx])*feather[wx])
    protected = (z <= ROOT_Z) | ((z <= 2.09) & (np.abs(old[:, 0]) >= .395) & (old[:, 1] >= -.12) & (old[:, 1] <= .19))
    new[protected] = old[protected]
    assert np.array_equal(new[:, 2], old[:, 2])
    obj.data.vertices.foreach_set('co', new.reshape(-1))
    obj.data.update()
    return {'modified_vertices': int(np.count_nonzero(np.any(new != old, axis=1))),
            'maximum_displacement': float(np.max(np.linalg.norm(new-old, axis=1))),
            'all_heights_exact': True, 'mouth_apex_retraction': .006,
            'throat_forward_easing': .012, 'cheek_spread': .014,
            'root_z': ROOT_Z, 'front_join_z': FRONT_JOIN_Z, 'width_join_z': WIDTH_JOIN_Z}

def sculpt_broad_face_arc(body, shell=None, fit_features=True):
    assert not body.get('broad_face_arc_refined'), 'Start with the saved preceding version.'
    report = reshape(body)
    if shell is not None:
        reshape(shell)
    if fit_features:
        # The existing seam already has the intended narrow height.
        report['eyes'] = refit_features(body, seam_height_scale=1.0)
    body['broad_face_arc_refined'] = True
    body['head_revision'] = 'Broad blunt muzzle, distributed lower-face/throat curve and gradual chest flare'
    return report
