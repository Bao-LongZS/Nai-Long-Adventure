"""Build one real E2 trial from the hash-pinned selected H2, without accumulation."""
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
from e2_eye_sculpt import *
from eye_projection_measure import visible_metrics
from inspect_e2_candidates import ContactCache
from spherical_eye_sculpt import frozen_snapshot
from saved_delivery_metadata import preserved_links, differences

ap = argparse.ArgumentParser()
ap.add_argument('--source', required=True, type=Path)
ap.add_argument('--source-sha', required=True)
ap.add_argument('--output', required=True, type=Path)
ap.add_argument('--shift-fraction', type=float, choices=[.5, 1.], default=.5)
ap.add_argument('--radius-scale', type=float, default=1.)
ap.add_argument('--installation-mode', choices=['source-fit', 'aperture-bias'], default='source-fit',
                help='source-fit preserves the existing installation solver; aperture-bias fixes aperture-relative globe XZ.')
ap.add_argument('--installation-outset', type=float,
                help='aperture-bias only: Cx=actual aperture_x-side_sign*outset; positive values face the visible cap outward.')
ap.add_argument('--installation-rise', type=float,
                help='aperture-bias only: Cz=actual aperture_z-rise; positive values face the visible cap upward.')
ap.add_argument('--axis-ratio', type=float, default=1.12)
ap.add_argument('--gap-slope', type=float, default=.9)
ap.add_argument('--transition', type=float, default=.043)
ap.add_argument('--depth-offset', type=float, default=0.,
                help='Additional globe burial along +Y after skin-rim depth fit; contact skin follows.')
ap.add_argument('--contact-method', choices=['rim_fit', 'surface_gap', 'hybrid_gap', 'continuous_gap', 'harmonic_rim', 'optimized_harmonic'], default='rim_fit')
ap.add_argument('--rim-orders', type=int, choices=[3, 4, 5, 6], default=3,
                help='Harmonic contact only: Fourier order count; default 3 preserves legacy geometry. Orders above 3 use C2-matched damped outer continuation.')
ap.add_argument('--gaze-out', type=float, choices=[20., 35.], default=20.)
ap.add_argument('--gaze-up', type=float, choices=[0., 10.], default=0.)
ap.add_argument('--skip-detailed-metrics', action='store_true')
args = ap.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
if args.installation_mode == 'aperture-bias':
    if args.installation_outset is None or args.installation_rise is None:
        ap.error('aperture-bias requires --installation-outset and --installation-rise')
    if not all(math.isfinite(value) and value >= 0.
               for value in (args.installation_outset, args.installation_rise)):
        ap.error('installation offsets must be finite and nonnegative')
elif args.installation_outset is not None or args.installation_rise is not None:
    ap.error('--installation-outset/--installation-rise require --installation-mode aperture-bias')
source = args.source.resolve()
assert hashlib.sha256(source.read_bytes()).hexdigest() == args.source_sha
dest = args.output.resolve()
assert dest != source.parent and dest/'Naiwa_Character.blend' != source
assert not (dest/'Naiwa_Character.blend').exists(), 'Refuse to overwrite an existing E2 candidate'
dest.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(source), load_ui=False)
rig = bpy.data.objects['Naiwa_Rig']
rig.animation_data.action = bpy.data.actions['Naiwa_Idle']
rig.animation_data.use_nla = False
rig.data.pose_position = 'POSE'
bpy.context.scene.frame_set(1); bpy.context.view_layer.update()
body = bpy.data.objects[BODY]
source_record = mesh_record(body, bpy.context.evaluated_depsgraph_get())
unchanged = frozen_snapshot()
unchanged.pop(BODY, None); unchanged.pop(SHELL, None)
source_links = preserved_links()
# The selected H2 can slightly alter the real skin aperture while keeping its
# globe fixed. Anchor E2 to that saved mesh's visible aperture, not stale R1
# constants or intended construction centers.
source_metrics = {s: visible_metrics(
    bpy.data.objects[s+' concentric iris and pupil'].evaluated_get(
        bpy.context.evaluated_depsgraph_get()),
    body.evaluated_get(bpy.context.evaluated_depsgraph_get()), step=.0006)
    for s in ('L', 'R')}
centers = {s: source_metrics[s]['aperture']['center'] for s in ('L', 'R')}
r1_centers = {'L': [-.11728029557883501, 2.505195917599275],
              'R': [.11728283728748391, 2.5053236253856577]}
parameters = {s: make_parameters(source_record, s, centers[s], args.shift_fraction,
                                args.radius_scale, args.axis_ratio,
                                args.gap_slope, args.transition,
                                args.contact_method, args.depth_offset,
                                args.installation_mode, args.installation_outset,
                                args.installation_rise, args.rim_orders) for s in ('L', 'R')}
skin_report = modify_skin(body, parameters)
source_points = skin_report.pop('original_points')
body_delta = skin_report.pop('actual_delta')
# A historical silhouette shell follows the same local contact change, while
# remaining hidden in actual renders; it is never substituted for body mesh.
shell_report = None
if SHELL in bpy.data.objects:
    shell = bpy.data.objects[SHELL]
    shell_report = modify_skin(shell, parameters)
    shell_report.pop('original_points'); shell_report.pop('actual_delta')
    shell.hide_render = True
bpy.context.view_layer.update()
revised_record = mesh_record(body, bpy.context.evaluated_depsgraph_get())
# A preflight complements the separate saved-mesh validator; it does not
# replace it.  Refuse a reversed source-relative face before saving a trial.
normal_dot = np.einsum('ij,ij->i', revised_record['triangle_normals'],
                       source_record['triangle_normals'])
assert np.isfinite(revised_record['points']).all(), 'Nonfinite E2 contact skin'
assert normal_dot.min() > 0., ('E2 contact preflight: reversed surface slope',
                               float(normal_dot.min()),
                               int(np.count_nonzero(normal_dot<=0.)))
skin_report['source_relative_normal_preflight'] = {
    'minimum_triangle_normal_dot': float(normal_dot.min()),
    'triangle_count': len(normal_dot), 'flipped_triangle_count': 0}
for side, par in parameters.items():
    cache = ContactCache(revised_record, centers[side])
    # This cache already comes from the actual modified mesh.  Do not apply the
    # contact function twice while calibrating the visible black pupil.
    gaze = gaze_axis(side, args.gaze_out, args.gaze_up)
    cap = calibrate_pupil(cache, par, gaze)
    eye = apply_eye(par, gaze, cap)
    par['gaze_outward_degrees'] = args.gaze_out
    par['gaze_upward_degrees'] = args.gaze_up
    par['actual_frontal_metrics'] = (visible_metrics(eye, body, step=.0006)
                                      if not args.skip_detailed_metrics else
                                      {'analytic_from_actual_skin': cache.shape(par, gaze, cap)})
    if not args.skip_detailed_metrics:
        met = par['actual_frontal_metrics']
        eye['frontal_aperture_center'] = met['aperture']['center']
        eye['frontal_pupil_center'] = [met['black']['center'][0],
                                      par['sphere_center'][1]-par['R'],
                                      met['black']['center'][1]]
        eye['frontal_pupil_radius'] = float(np.sqrt(met['black']['area']/np.pi))
    par['source_H2_actual_frontal_metrics'] = source_metrics[side]
    par['source_H2_center_minus_R1_center'] = (
        np.asarray(centers[side])-np.asarray(r1_centers[side])).tolist()
    print('E2_NATIVE_EYE', side, par['actual_frontal_metrics'], flush=True)
check = frozen_snapshot(); check.pop(BODY, None); check.pop(SHELL, None)
assert check == unchanged, 'E2 changed a frozen non-eye asset or rig'
link_differences = differences(source_links, preserved_links())
assert not link_differences, ('E2 changed materials, relationships or animation curves',
                               link_differences[:6])
bpy.context.preferences.filepaths.save_version = 0
saved = dest/'Naiwa_Character.blend'
bpy.ops.wm.save_as_mainfile(filepath=str(saved), compress=True)
build_options = {'shift_fraction': args.shift_fraction, 'radius_scale': args.radius_scale,
                 'axis_ratio': args.axis_ratio, 'gap_slope': args.gap_slope,
                 'depth_offset': args.depth_offset,
                 'transition': args.transition, 'gaze_out': args.gaze_out,
                 'gaze_up': args.gaze_up, 'contact_method': args.contact_method,
                 'rim_orders': args.rim_orders,
                 'installation_mode': args.installation_mode}
if args.installation_mode == 'aperture-bias':
    # The generic rebuild recipe forwards every option.  Keep absent offsets
    # out of source-fit recipes so no null option is forwarded as a value.
    build_options.update(installation_outset=args.installation_outset,
                         installation_rise=args.installation_rise)
report = {'source': str(source), 'source_sha256': args.source_sha,
          'saved_blend_sha256': hashlib.sha256(saved.read_bytes()).hexdigest(),
          'stage': 'E2 eye trial, final selection requires multi-view review',
          'eyes': parameters, 'skin': skin_report, 'shell': shell_report,
          'non_eye_assets_and_rig_unchanged': True,
          'native_colors': 'EyeColor per-corner face colors on a complete real sphere',
          'body_topology_UV_colors_weights': 'preserved; only local eye contact Y coordinates changed',
          'options': build_options}
(dest/'e2_build_report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
np.savez_compressed(dest/'e2_skin_deformation.npz', source_points=source_points,
                    actual_delta=body_delta)
assert hashlib.sha256(source.read_bytes()).hexdigest() == args.source_sha
print('E2_EYE_REFINEMENT_BUILD_OK', flush=True)
