"""Compare genuine mesh pupil caps on an E2 trial; source is never saved."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
import numpy as np
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from e2_eye_sculpt import BODY, apply_eye, calibrate_pupil, gaze_axis
from geometry_diagnostic_utils import mesh_record
from inspect_e2_candidates import ContactCache
from render_head_reshape_comparison import prepare_scene
from spherical_reference_projection import axes, eye_pixels

FIXED = {'type': 'ORTHO', 'yaw': 50., 'pitch': -2.5,
         'target': [0., -.12, 2.425], 'distance': 7., 'scale': .86, 'lens': 50.}
REGISTER_SCALE = .41175526202447044


def setup(scene, pose, resolution=900):
    back, _, _ = axes(pose['yaw'], pose['pitch'])
    target = Vector(pose['target'])
    cam = scene.camera
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = pose['scale']
    cam.location = target+pose['distance']*Vector(back)
    cam.rotation_euler = (target-cam.location).to_track_quat('-Z', 'Y').to_euler()
    scene.render.resolution_x = scene.render.resolution_y = resolution
    bpy.context.view_layer.update()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--source', required=True, type=Path)
    ap.add_argument('--output', required=True, type=Path)
    ap.add_argument('--resolution', type=int, default=900)
    ap.add_argument('--no-render', action='store_true')
    args = ap.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    source = args.source.resolve()
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    params = json.loads((source.parent/'e2_build_report.json').read_text(encoding='utf-8'))['eyes']
    dest = args.output.resolve(); dest.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(source), load_ui=False)
    scene, engine = prepare_scene()
    body = mesh_record(bpy.data.objects[BODY], bpy.context.evaluated_depsgraph_get())
    caches = {s: ContactCache(body, par['aperture_center_xz']) for s, par in params.items()}
    results = []
    for outward in (20., 35.):
        for upward in (0., 10.):
            name = f'gaze_out{outward:.0f}_up{upward:.0f}'
            eyes = {}
            for side, par in params.items():
                gaze = gaze_axis(side, outward, upward)
                cap = calibrate_pupil(caches[side], par, gaze)
                apply_eye(par, gaze, cap)
                eyes[side] = mesh_record(bpy.data.objects[side+' concentric iris and pupil'],
                                        bpy.context.evaluated_depsgraph_get(), eye=True)
            record = {'candidate': name, 'outward_degrees': outward,
                      'upward_degrees': upward, 'views': {},
                      'native_mesh_cap': True, 'saved_model': False}
            for view, yaw in (('front', 0.), ('three_quarter', 45.),
                              ('reference50', 50.), ('side', 90.)):
                pose = dict(FIXED, yaw=yaw)
                if view != 'reference50':
                    pose.update(pitch=0., target=[0., -.12, 2.37], scale=.85)
                setup(scene, pose, args.resolution)
                metrics = {s: eye_pixels(eyes[s], body, pose,
                                       args.resolution, step=1.5) for s in ('L', 'R')}
                for side in metrics:
                    aperture, pupil = metrics[side]['aperture'], metrics[side]['pupil']
                    if aperture.get('visible') and pupil.get('visible'):
                        offset = np.asarray(pupil['center_pixels'])-np.asarray(aperture['center_pixels'])
                        metrics[side]['pupil_offset_render_pixels'] = offset.tolist()
                        if view == 'reference50':
                            metrics[side]['pupil_offset_registered_reference_pixels'] = (offset*REGISTER_SCALE).tolist()
                if metrics['L']['aperture'].get('visible') and metrics['R']['aperture'].get('visible'):
                    d = [metrics[s]['aperture']['D_pixels'] for s in ('L', 'R')]
                    metrics['far_near_D_ratio'] = min(d)/max(d)
                filename = f'{name}_{view}.png'
                record['views'][view] = {'pose': pose, 'eyes': metrics,
                                         'filename': filename if not args.no_render else None}
                if not args.no_render:
                    scene.render.filepath = str(dest/filename)
                    bpy.ops.render.render(write_still=True)
                print('E2_GAZE_VIEW', name, view, metrics, flush=True)
            results.append(record)
    assert hashlib.sha256(source.read_bytes()).hexdigest() == digest
    (dest/'gaze_comparison.json').write_text(json.dumps({'source': str(source),
        'source_sha256': digest, 'source_unchanged': True, 'engine': engine,
        'resolution': args.resolution, 'records': results,
        'reference_pupil_offset_target_pixels': [-5.6, 2.6],
        'selection_rule': 'Multiple-view native eye cap review; target reference alone does not choose gaze'}, indent=2)+'\n', encoding='utf-8')
    print('E2_GAZE_CANDIDATES_OK', flush=True)


if __name__ == '__main__':
    main()
