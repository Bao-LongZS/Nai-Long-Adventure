"""Read-only E2 installation study on a pinned source; never saves a model."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
import numpy as np

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from e2_eye_sculpt import BODY, D0, contact_target, make_parameters, gaze_axis
from geometry_diagnostic_utils import mesh_record
from spherical_eye_sculpt import ProjectionCache
from eye_projection_measure import visible_metrics


def stats(x, z, mask, step):
    points = np.column_stack((x[mask], z[mask]))
    if len(points) < 10:
        return {'visible': False}
    covariance = np.cov(points.T, bias=True)
    values, vectors = np.linalg.eigh(covariance)
    axis = vectors[:, 1]
    angle = math.degrees(math.atan2(axis[1], axis[0]))
    if angle > 90.: angle -= 180.
    if angle < -90.: angle += 180.
    return {'visible': True, 'area': float(len(points)*step**2),
            'D': math.sqrt(4.*len(points)*step**2/math.pi),
            'center_xz': points.mean(axis=0).tolist(),
            'axis_ratio': math.sqrt(float(values[1]/values[0])),
            'major_axis_degrees': angle}


class ContactCache(ProjectionCache):
    def __init__(self, body, target, step=.0006):
        super().__init__(body, target, step)
        self.source_skin = self.skin.copy()

    def with_contact(self, par):
        self.skin = self.source_skin.copy()
        finite = np.isfinite(self.source_skin)
        points = np.column_stack((self.X[finite], self.source_skin[finite], self.Z[finite]))
        self.skin[finite], _ = contact_target(points, par)
        return self

    def shape(self, par, gaze=None, angle=None):
        c = np.asarray(par['sphere_center'])
        dx = self.X-c[0]; dz = self.Z-c[2]
        q = par['R']**2-dx*dx-dz*dz
        root = np.sqrt(np.maximum(q, 0.))
        visible = (q > 0.) & (c[1]-root < self.skin-5e-7)
        result = {'aperture': stats(self.X, self.Z, visible, self.step)}
        if gaze is not None:
            dot = (dx*gaze[0]-root*gaze[1]+dz*gaze[2])/par['R']
            black = visible & (dot >= math.cos(angle))
            result['black'] = stats(self.X, self.Z, black, self.step)
            result['pupil_ratio'] = math.sqrt(float(black.sum())/max(1, float(visible.sum())))
        return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--source', required=True, type=Path)
    ap.add_argument('--source-sha')
    ap.add_argument('--output', required=True, type=Path)
    ap.add_argument('--step', type=float, default=.0006)
    ap.add_argument('--contact-method', choices=['rim_fit', 'surface_gap', 'hybrid_gap', 'continuous_gap', 'harmonic_rim', 'optimized_harmonic'], default='rim_fit')
    ap.add_argument('--gap-slope', type=float, default=.9)
    ap.add_argument('--transition', type=float, default=.043)
    ap.add_argument('--depth-offset', type=float, default=0.)
    args = ap.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    source = args.source.resolve()
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    if args.source_sha: assert digest == args.source_sha
    bpy.ops.wm.open_mainfile(filepath=str(source), load_ui=False)
    rig = bpy.data.objects['Naiwa_Rig']
    rig.animation_data.action = bpy.data.actions['Naiwa_Idle']
    rig.animation_data.use_nla = False
    rig.data.pose_position = 'POSE'
    bpy.context.scene.frame_set(1); bpy.context.view_layer.update()
    body = mesh_record(bpy.data.objects[BODY], bpy.context.evaluated_depsgraph_get())
    depsgraph = bpy.context.evaluated_depsgraph_get()
    source_metrics = {s: visible_metrics(
        bpy.data.objects[s+' concentric iris and pupil'].evaluated_get(depsgraph),
        bpy.data.objects[BODY].evaluated_get(depsgraph), step=args.step)
        for s in ('L', 'R')}
    centers = {s: source_metrics[s]['aperture']['center'] for s in ('L', 'R')}
    caches = {s: ContactCache(body, centers[s], step=args.step) for s in ('L', 'R')}
    records = []
    for shift in (.5, 1.):
        for ratio in (1.10, 1.12, 1.15):
            for radius_scale in (.98, 1., 1.02):
                eyes = {}
                for side in ('L', 'R'):
                    try:
                        par = make_parameters(body, side, centers[side], shift,
                                              radius_scale, ratio, args.gap_slope,
                                              args.transition, args.contact_method,
                                              args.depth_offset)
                        cache = caches[side].with_contact(par)
                        met = cache.shape(par)
                        par['predicted_frontal_metrics'] = met
                        finite = np.isfinite(cache.source_skin)
                        par['maximum_predicted_skin_Y_change'] = float(np.max(
                            np.abs(cache.skin[finite]-cache.source_skin[finite])))
                        eyes[side] = par
                    except ValueError as exc:
                        eyes[side] = {'failed': str(exc)}
                records.append({'shift_fraction': shift, 'radius_scale': radius_scale,
                                'target_axis_ratio': ratio, 'eyes': eyes})
                print('E2_READONLY_CONTACT_TRIAL', shift, ratio, radius_scale,
                      {s: eyes[s].get('predicted_frontal_metrics', {}) for s in eyes}, flush=True)
    assert hashlib.sha256(source.read_bytes()).hexdigest() == digest
    out = args.output.resolve()
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps({'source': str(source), 'source_sha256': digest,
                              'read_only': True, 'model_saved': False,
                              'sampling_step': args.step,
                              'contact_method': args.contact_method,
                              'contact_gap_slope': args.gap_slope,
                              'contact_transition_width': args.transition,
                              'additional_depth_offset_Y': args.depth_offset,
                              'source_H2_actual_frontal_metrics': source_metrics,
                              'coordinate_units': 'World project units, frontal XZ projection',
                              'method': 'Analytic true sphere against actual source skin ray cache and proposed local contact surface; native candidate validation required',
                              'records': records}, indent=2)+'\n', encoding='utf-8')
    print('E2_CANDIDATES_READONLY_OK', flush=True)


if __name__ == '__main__':
    main()
