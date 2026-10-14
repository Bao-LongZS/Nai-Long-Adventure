"""Limited C3 review renders; saved source projects are never overwritten.

Blender --background --python-exit-code 1 --python render_c3_review.py --
    --root VERSION_ROOT --source SAVED.blend --label E2 --mode key

Outputs: VERSION_ROOT/review/MODE/LABEL/*.png and protocol.json.
Completed, hash-identical mode outputs are reused. No model parameter search,
camera fitting, hidden reference registration, or complete legacy render suite.
"""
import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import sys

import bpy
from mathutils import Vector

SCRIPT_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_ROOT))
from render_head_reshape_comparison import prepare_scene, gray_override
from render_head_neck_comparisons import camera, body_id, COMMON
from render_n2_round_comparisons import (
    digest, light_snapshot, diagnostic_lighting, STRIPES, GRAZING)

VIEWS = ('front', 'three_quarter', 'side')
POSES = (('Naiwa_Walk', 9), ('Naiwa_Laugh', 7), ('Naiwa_Jump', 6))
COUNTS = {'baseline_diagnostic': 4, 'key': 9, 'selected': 6, 'poses': 6}
REGISTRATION = {'scale': .41175526202447044,
                'translation': [245.38813136509236, -17.159665982101984]}
MOVED_KEY = [-3., -4., 5.]


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n',
                    encoding='utf-8')


def fixed_cameras():
    path = SCRIPT_ROOT / 'versions/20261006_spherical_eyes/comparisons/camera_protocol.json'
    old = json.loads(path.read_text(encoding='utf-8-sig'))
    heads = []
    for view in VIEWS:
        record = next(r for r in old['files'] if r['model'] == 'A'
                      and r['kind'] == 'head' and not r['gray'] and view in r['file'])
        heads.append((view, {'type': 'ORTHO', 'yaw': record['yaw'],
            'pitch': record['pitch'], 'target': record['target'], 'distance': 7.,
            'scale': record['ortho_scale'], 'lens': 50.,
            'resolution': record['resolution'], 'sensor_fit': 'AUTO'}))
    record = next(r for r in old['files'] if r['model'] == 'A' and r['kind'] == 'full')
    full = {'type': 'ORTHO', 'yaw': 45., 'pitch': 0., 'target': record['target'],
            'distance': 7., 'scale': record['ortho_scale'], 'lens': 50.,
            'resolution': record['resolution'], 'sensor_fit': 'AUTO'}
    return path, digest(path), heads, full


@contextmanager
def explicit_ao_off(scene):
    """Plain gray Cycles diagnosis, with no added ambient-occlusion effect.

    Physical self-shadowing and indirect illumination remain. This intentionally
    separate diagnostic uses 32 fixed CPU samples; it does not relabel a dark
    physical shadow as an AO artifact. Legacy AO switches are disabled when
    available; gray_override supplies an AO-free Principled material.
    """
    old_engine = scene.render.engine
    old_cycles = (scene.cycles.samples, scene.cycles.seed,
                  scene.cycles.use_animated_seed, scene.cycles.device)
    changed = []
    for holder in (getattr(scene, 'eevee', None),
                   getattr(scene.world, 'light_settings', None),
                   getattr(scene.world, 'cycles', None)):
        if holder is None:
            continue
        for name in ('use_gtao', 'use_ambient_occlusion'):
            if hasattr(holder, name):
                changed.append((holder, name, getattr(holder, name)))
                setattr(holder, name, False)
    try:
        scene.render.engine = 'CYCLES'
        scene.cycles.samples = 32
        scene.cycles.seed = 0
        scene.cycles.use_animated_seed = False
        scene.cycles.device = 'CPU'
        yield {'added_AO': False, 'material_AO_node': False,
               'engine': 'CYCLES', 'samples': 32, 'seed': 0, 'device': 'CPU',
               'physical_shadows_preserved': True,
               'legacy_AO_switches_disabled': [name for _, name, _ in changed]}
    finally:
        for holder, name, original in changed:
            setattr(holder, name, original)
        scene.render.engine = old_engine
        (scene.cycles.samples, scene.cycles.seed,
         scene.cycles.use_animated_seed, scene.cycles.device) = old_cycles


@contextmanager
def moved_key(scene):
    key = bpy.data.objects['Surface soft key']
    location, rotation = tuple(key.location), tuple(key.rotation_euler)
    try:
        key.location = MOVED_KEY
        key.rotation_euler = (Vector((0., 0., 2.2)) - key.location).to_track_quat('-Z', 'Y').to_euler()
        bpy.context.view_layer.update()
        yield {'light': key.name, 'source_location': list(location),
               'location': list(key.location), 'target': [0., 0., 2.2],
               'other_lighting_unchanged': True}
    finally:
        key.location, key.rotation_euler = location, rotation
        bpy.context.view_layer.update()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root', type=Path, required=True)
    ap.add_argument('--source', type=Path, required=True)
    ap.add_argument('--label', choices=('E2', 'A', 'B'), required=True)
    ap.add_argument('--mode', choices=tuple(COUNTS), required=True)
    args = ap.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    root = args.root.resolve()
    source = args.source.resolve(strict=True)
    if source.suffix.lower() != '.blend':
        raise ValueError('Expected a saved .blend source')
    before = digest(source)
    original, original_hash, heads, full = fixed_cameras()
    out = root / 'review' / args.mode / args.label
    contract_path = root / 'c3_render_contract.json'
    protocol_path = out / 'protocol.json'
    bpy.ops.wm.open_mainfile(filepath=str(source), load_ui=False)
    scene, engine = prepare_scene()
    lights = light_snapshot(scene)
    bg = next(n for n in scene.world.node_tree.nodes if n.type == 'BACKGROUND')
    world = {'color': list(bg.inputs['Color'].default_value),
             'strength': bg.inputs['Strength'].default_value}
    contract = {'schema_version': 1, 'source_camera_protocol': str(original),
        'source_camera_protocol_sha256': original_hash,
        'heads': [{'view': view, 'pose': pose} for view, pose in heads],
        'full_three_quarter': full, 'common_reference_camera': COMMON,
        'fixed_reference_registration': REGISTRATION,
        'animation': 'Naiwa_Idle', 'frame': 1, 'pose_position': 'POSE', 'NLA': False,
        'standard_engine': engine, 'lights': lights, 'world': world,
        'view_transform': scene.view_settings.view_transform,
        'look': scene.view_settings.look, 'exposure': scene.view_settings.exposure,
        'gamma': scene.view_settings.gamma, 'resolution_percentage': 100,
        'pixel_aspect': [1., 1.], 'baseline_diagnostic': {
            'added_AO': False, 'engine': 'CYCLES', 'samples': 32, 'seed': 0,
            'device': 'CPU', 'moved_key_location': MOVED_KEY},
        'reflection': STRIPES, 'grazing': GRAZING,
        'pose_views': {'yaw': [45., 90.], 'pitch': 0., 'target': [0., -.12, 2.22],
                       'scale': 1.3, 'resolution': [900, 1000]},
        'source_models_saved': False}
    root.mkdir(parents=True, exist_ok=True)
    if contract_path.exists():
        existing_contract = json.loads(contract_path.read_text(encoding='utf-8-sig'))
        if existing_contract != contract:
            raise RuntimeError('Prepared camera/light/exposure protocol differs from frozen C3 contract')
    else:
        write_json(contract_path, contract)
    contract_hash = digest(contract_path)
    if protocol_path.exists():
        old = json.loads(protocol_path.read_text(encoding='utf-8-sig'))
        reusable = (old.get('complete') and old.get('source_sha256') == before
                    and old.get('contract_sha256') == contract_hash
                    and old.get('script_sha256') == digest(Path(__file__))
                    and len(old.get('files', [])) == COUNTS[args.mode]
                    and all((out / r['file']).is_file()
                            and digest(out / r['file']) == r['sha256'] for r in old['files']))
        if reusable:
            assert digest(source) == before
            print('C3_REVIEW_REUSED', args.label, args.mode, str(out), flush=True)
            return
    out.mkdir(parents=True, exist_ok=True)
    protocol = {'label': args.label, 'mode': args.mode, 'source': str(source),
        'source_sha256': before, 'contract_sha256': contract_hash,
        'script_sha256': digest(Path(__file__)), 'saved_source_model': False,
        'complete': False, 'expected_image_count': COUNTS[args.mode],
        'common_reference_camera': COMMON, 'fixed_reference_registration': REGISTRATION,
        'geometry': 'evaluated saved mesh at Idle frame1; Raw is measured separately',
        'automatic_visual_acceptance': False, 'files': []}
    write_json(protocol_path, protocol)

    def render(view, pose, mode, diagnostic=None, action='Naiwa_Idle', frame=1):
        camera(scene, pose)
        filename = f'{args.label}_{mode}_{view}.png'
        target = out / filename
        scene.render.filepath = str(target)
        bpy.ops.render.render(write_still=True)
        protocol['files'].append({'file': filename, 'sha256': digest(target),
            'model': args.label, 'mode': mode, 'view': view, 'pose': pose,
            'location': list(scene.camera.location), 'source_sha256': before,
            'action': action, 'frame': frame, 'engine': scene.render.engine,
            'view_transform': scene.view_settings.view_transform,
            'exposure': scene.view_settings.exposure, 'gamma': scene.view_settings.gamma,
            'diagnostic': diagnostic})
        write_json(protocol_path, protocol)
        print('C3_REVIEW_IMAGE', args.label, args.mode, filename, flush=True)

    if args.mode == 'baseline_diagnostic':
        with explicit_ao_off(scene) as ao, gray_override(scene):
            for view, pose in heads:
                render(view, pose, 'head_gray_AO_off', ao)
            with moved_key(scene) as light:
                render('three_quarter_moved_key', heads[1][1], 'head_gray_AO_off',
                       dict(ao, moved_light=light))
    elif args.mode == 'key':
        for view, pose in heads:
            render(view, pose, 'head_color')
        render('three_quarter', full, 'full_color')
        common = dict(COMMON, sensor_fit='HORIZONTAL')
        render('50', common, 'common_ORTHO')
        with gray_override(scene):
            for view, pose in heads:
                render(view, pose, 'head_gray')
        with body_id(scene):
            render('50', common, 'common_body_id')
    elif args.mode == 'selected':
        for variant in ('A', 'B'):
            with diagnostic_lighting(scene, 'grazing', variant) as config:
                for view, pose in heads[1:]:
                    render(view, pose, 'grazing_' + variant, config)
        with diagnostic_lighting(scene, 'reflection') as config:
            for view, pose in heads[1:]:
                render(view, pose, 'reflection', config)
    else:
        rig = bpy.data.objects['Naiwa_Rig']
        for action, frame in POSES:
            rig.animation_data.action = bpy.data.actions[action]
            rig.animation_data.use_nla = False
            rig.data.pose_position = 'POSE'
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            with gray_override(scene):
                for view, yaw in (('three_quarter', 45.), ('side', 90.)):
                    pose = {'type': 'ORTHO', 'yaw': yaw, 'pitch': 0.,
                        'target': [0., -.12, 2.22], 'distance': 7., 'scale': 1.3,
                        'lens': 50., 'resolution': [900, 1000], 'sensor_fit': 'AUTO'}
                    render(view, pose, f'{action}_{frame}_gray', action=action, frame=frame)
    assert len(protocol['files']) == COUNTS[args.mode]
    assert light_snapshot(scene) == lights, 'Temporary light state not restored'
    assert digest(source) == before, 'Read-only review changed saved source'
    assert digest(original) == original_hash and digest(contract_path) == contract_hash
    protocol['complete'] = True
    protocol['source_hash_verified_after_render'] = True
    write_json(protocol_path, protocol)
    print('C3_REVIEW_OK', args.label, args.mode, COUNTS[args.mode], str(out), flush=True)


if __name__ == '__main__':
    main()
