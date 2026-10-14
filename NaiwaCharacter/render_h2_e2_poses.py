"""Read-only fixed R1/H2/E2 Walk/Laugh/Jump shoulder and feature views."""
import argparse
import json
from pathlib import Path
import sys
import bpy

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from render_h2_e2_comparisons import BASE, BASE_HASH, OUT
from render_head_reshape_comparison import prepare_scene, gray_override
from render_head_neck_comparisons import camera, digest

POSES = [('Naiwa_Walk', 9), ('Naiwa_Laugh', 7), ('Naiwa_Jump', 6)]
VIEWS = [('three_quarter', 45.), ('side', 90.)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--labels', nargs='+', choices=['R1', 'H2', 'E2'], default=['R1', 'H2', 'E2'])
    parser.add_argument('--output', type=Path, default=OUT)
    parser.add_argument('--candidate', type=Path, help='Override one H2 or E2 source for independent trial review.')
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    if args.candidate:
        assert len(args.labels)==1 and args.labels[0]!='R1'
    folder = args.output.resolve() / 'pose_comparison'
    folder.mkdir(parents=True, exist_ok=True)
    protocol_path = folder / 'pose_protocol.json'
    protocol = json.loads(protocol_path.read_text(encoding='utf-8')) if protocol_path.exists() else {
        'saved_source_models': False, 'same_camera_light_exposure': True,
        'sources': {}, 'files': []}
    assert digest(BASE) == BASE_HASH
    for label in args.labels:
        path = BASE if label == 'R1' else (args.candidate or OUT / f'candidate_{label}/Naiwa_Character.blend')
        source_sha = digest(path)
        bpy.ops.wm.open_mainfile(filepath=str(path), load_ui=False)
        scene, _ = prepare_scene()
        rig = bpy.data.objects['Naiwa_Rig']
        protocol['sources'][label] = {'path': str(path), 'sha256': source_sha}
        protocol['files'] = [r for r in protocol['files'] if r['model'] != label]
        for action_name, frame in POSES:
            rig.animation_data.action = bpy.data.actions[action_name]
            rig.animation_data.use_nla = False
            rig.data.pose_position = 'POSE'
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            with gray_override(scene):
                for view, yaw in VIEWS:
                    pose = {'type': 'ORTHO', 'yaw': yaw, 'pitch': 0.,
                            'target': [0., -.12, 2.22], 'distance': 7.,
                            'scale': 1.3, 'lens': 50., 'resolution': [900, 1000]}
                    camera(scene, pose)
                    filename = f'{label}_{action_name}_{frame}_{view}.png'
                    scene.render.filepath = str(folder / filename)
                    bpy.ops.render.render(write_still=True)
                    protocol['files'].append({'model': label, 'source_sha256': source_sha,
                        'action': action_name, 'frame': frame, 'pose': pose, 'file': filename,
                        'engine': scene.render.engine, 'exposure': scene.view_settings.exposure})
        assert digest(path) == source_sha
        protocol_path.write_text(json.dumps(protocol, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    assert digest(BASE) == BASE_HASH
    print('H2_E2_POSE_COMPARISONS_OK', flush=True)


if __name__ == '__main__':
    main()
