"""Build and preview an in-place Naiwa run in an isolated E2 copy.

Run with Blender --background --python-exit-code 1 --python this.py -- --probe.
The pinned E2 source is never saved. A new action is the only rig change.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector
import numpy as np

WORKSPACE = Path(__file__).resolve().parents[2]
SOURCE = WORKSPACE / 'NaiwaCharacter/versions/20261007_H2_E2_overall_refinement/candidate_E2/Naiwa_Character.blend'
SOURCE_SHA = '317f62278d63217636bf393f831e2fb41ce21acfefbe21beed6ad6d4dc75f322'
OUT = WORKSPACE / 'UE5/Animation/Run_v1'
FPS = 24
CYCLE = 16 / FPS
GAME_HEIGHT = 1.8
SOURCE_HEIGHT = 2.636830637455
GAME_SCALE = GAME_HEIGHT / SOURCE_HEIGHT
SOURCE_SPEED = 3.0 / GAME_SCALE


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def probe(rig):
    result = {'bones': {}, 'colors': {}}
    for bone in rig.data.bones:
        result['bones'][bone.name] = {
            'head': list(bone.head_local), 'tail': list(bone.tail_local),
            'parent': bone.parent.name if bone.parent else None,
            'matrix': [list(row) for row in bone.matrix_local],
        }
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH' or not any(m.type == 'ARMATURE' for m in obj.modifiers):
            continue
        colors = {}
        for attribute in obj.data.color_attributes:
            counts = {}
            for d in attribute.data:
                key = tuple(round(c, 4) for c in d.color)
                counts[key] = counts.get(key, 0) + 1
            colors[attribute.name] = {
                'domain': attribute.domain, 'type': attribute.data_type,
                'most_common_linear': sorted(counts.items(), key=lambda x: -x[1])[:8],
                'first_srgb': list(attribute.data[0].color_srgb) if attribute.data else None,
            }
        result['colors'][obj.name] = colors
    return result


def cyclic_table(phase, table):
    """Smooth recovery arcs; the stance translation is separately linear."""
    phase %= 1.0
    for index in range(len(table) - 1):
        a, b = table[index], table[index + 1]
        if a[0] <= phase <= b[0]:
            u = (phase - a[0]) / (b[0] - a[0])
            u = u * u * (3.0 - 2.0 * u)
            return a[1] * (1 - u) + b[1] * u
    return table[-1][1]


def world_delta(bone, rotation):
    rest = bone.bone.matrix_local.to_quaternion()
    bone.rotation_euler = (rest.inverted() @ rotation @ rest).to_euler('XYZ')


def rotate_x(bone, degrees, abduct=0.0):
    rotation = Quaternion(Vector((1, 0, 0)), math.radians(degrees))
    if abduct:
        rotation = Quaternion(Vector((0, 1, 0)), math.radians(abduct)) @ rotation
    world_delta(bone, rotation)


def leg_ik(rig, side, phase, contact_adjust=0.0):
    """Bake a two-link leg; retain rest lengths and the original bone hierarchy."""
    thigh = rig.pose.bones['Thigh.' + side]
    shin = rig.pose.bones['Shin.' + side]
    hip = thigh.bone.head_local.copy()
    target = shin.bone.tail_local.copy()
    if phase <= .16:
        # The source has no ankle bone. Counter-rotate the shin during support
        # so its foot stays flat; its short stance then matches capsule speed.
        target.y = -.22 + SOURCE_SPEED * CYCLE * phase + contact_adjust
        rest_vector = thigh.bone.tail_local - hip
        angle = math.asin(max(-.98, min(.98, (target.y - hip.y) / math.hypot(rest_vector.y, rest_vector.z)))) - math.atan2(rest_vector.y, -rest_vector.z)
        upper_rotation = Quaternion(Vector((1, 0, 0)), angle)
        world_delta(thigh, upper_rotation)
        world_delta(shin, upper_rotation.inverted())
        bpy.context.view_layer.update()
        return list(shin.tail)
    else:
        target.y = cyclic_table(phase, [(.16, .249), (.32, .42), (.53, .06), (.76, -.30), (1, -.22)])
        target.z = cyclic_table(phase, [(.16, .155), (.33, .57), (.57, .62), (.80, .31), (1, .16)])
    # A slightly flexed knee remains forward of the line from hip to ankle.
    v = Vector((0.0, target.y - hip.y, target.z - hip.z))
    l1, l2 = thigh.bone.length, shin.bone.length
    distance = min(v.length, l1 + l2 - .003)
    direction = v.normalized()
    cosine = max(-1.0, min(1.0, (l1*l1 + distance*distance - l2*l2) / (2*l1*distance)))
    along = l1 * cosine
    perpendicular = Vector((0.0, direction.z, -direction.y)).normalized()
    knee = hip + direction * along + perpendicular * math.sqrt(max(0.0, l1*l1 - along*along))
    knee.x = shin.bone.head_local.x
    target.x = shin.bone.tail_local.x
    upper_rotation = (thigh.bone.tail_local - hip).rotation_difference(knee - hip)
    lower_rotation = (shin.bone.tail_local - shin.bone.head_local).rotation_difference(target - knee)
    world_delta(thigh, upper_rotation)
    # Remove the inherited upper-leg delta to obtain the shin's local delta.
    world_delta(shin, upper_rotation.inverted() @ lower_rotation)
    return list(target)


def create_belly_morphs(body):
    """Deform only torso-weighted abdominal vertices; preserve face and limbs."""
    positions = np.empty(len(body.data.vertices) * 3, dtype=np.float32)
    body.data.vertices.foreach_get('co', positions)
    positions = positions.reshape(-1, 3)
    x, y, z = positions.T
    # X/Z falloff avoids suppressing all exterior belly surface points, which
    # sit near the boundary of a solid 3-D ellipsoid rather than its centre.
    radius = (x / .72) ** 2 + ((z - 1.10) / .73) ** 2
    mask = np.clip(1 - radius, 0, 1)
    mask = mask * mask * (3 - 2 * mask)
    # Guard windows isolate abdomen from lower legs, neck, shoulders and arms.
    upper = np.clip((1.70 - z) / .22, 0, 1)
    lower = np.clip((z - .69) / .20, 0, 1)
    side = np.clip((.635 - np.abs(x)) / .13, 0, 1)
    guards = upper * upper * (3-2*upper) * lower * lower * (3-2*lower) * side * side * (3-2*side)
    torso_index = body.vertex_groups['Torso'].index
    torso_weight = np.zeros(len(positions), dtype=np.float32)
    limb_weight = np.zeros(len(positions), dtype=np.float32)
    limb_ids = {g.index for g in body.vertex_groups if g.name.startswith(('Thigh.', 'Shin.', 'UpperArm.', 'Forearm.'))}
    for vertex in body.data.vertices:
        for group in vertex.groups:
            if group.group == torso_index:
                torso_weight[vertex.index] = group.weight
            elif group.group in limb_ids:
                limb_weight[vertex.index] += group.weight
    mask *= guards * np.clip((torso_weight - .30) / .65, 0, 1) * np.clip((.35-limb_weight)/.25, 0, 1)
    if body.data.shape_keys is None:
        body.shape_key_add(name='Basis')
    compress_delta = np.column_stack((x*.055, (y+.065)*.055, -(z-1.10)*.105)) * mask[:, None]
    stretch_delta = np.column_stack((-x*.036, -(y+.065)*.036, (z-1.10)*.075)) * mask[:, None]
    lag_delta = np.column_stack((np.zeros(len(mask)), .060*mask, -.027*mask))
    morphs = {}
    for name, delta in [('Belly_Compress', compress_delta), ('Belly_Rebound', stretch_delta), ('Belly_SoftLag', lag_delta)]:
        key = body.shape_key_add(name=name)
        key.slider_min, key.slider_max = (-1, 1) if name == 'Belly_SoftLag' else (0, 1)
        key.data.foreach_set('co', (positions + delta).ravel())
        morphs[name] = {'max_delta_source_m': float(np.linalg.norm(delta, axis=1).max()),
                        'max_delta_game_cm': float(np.linalg.norm(delta, axis=1).max()*GAME_SCALE*100),
                        'delta_bounds_source_m': [delta.min(axis=0).tolist(), delta.max(axis=0).tolist()],
                        'changed_vertex_count': int(np.count_nonzero(np.linalg.norm(delta, axis=1) > 1e-7)),
                        'head_vertices_changed': int(np.count_nonzero((z >= 1.70) & (np.linalg.norm(delta, axis=1) > 1e-7))),
                        'low_leg_vertices_changed': int(np.count_nonzero((z <= .69) & (np.linalg.norm(delta, axis=1) > 1e-7))),
                        'outside_arm_width_vertices_changed': int(np.count_nonzero((np.abs(x) >= .635) & (np.linalg.norm(delta, axis=1) > 1e-7)))}
    return morphs


def belly_weights(phase):
    phase = phase % .5 * 2
    return {
        'Belly_Compress': cyclic_table(phase, [(0, .12), (.22, 1), (.46, 0), (.73, .04), (1, .12)]),
        'Belly_Rebound': cyclic_table(phase, [(0, 0), (.27, 0), (.62, 1), (.82, .25), (1, 0)]),
        'Belly_SoftLag': cyclic_table(phase, [(0, .15), (.25, .8), (.54, -.7), (.80, .25), (1, .15)]),
    }


def posed_feet(body, foot_indices):
    evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh = evaluated.to_mesh()
    try:
        vertices = np.empty(len(mesh.vertices) * 3, dtype=np.float32)
        mesh.vertices.foreach_get('co', vertices)
        vertices = vertices.reshape(-1, 3)
        return {side: vertices[ids].copy() for side, ids in foot_indices.items()}
    finally:
        evaluated.to_mesh_clear()


def make_run(rig, body):
    original_actions = sorted(action.name for action in bpy.data.actions)
    vertices = np.empty(len(body.data.vertices) * 3, dtype=np.float32)
    body.data.vertices.foreach_get('co', vertices)
    vertices = vertices.reshape(-1, 3)
    feet = {
        'L': np.flatnonzero((vertices[:, 2] < .19) & (vertices[:, 0] < -.19)),
        'R': np.flatnonzero((vertices[:, 2] < .19) & (vertices[:, 0] > .19)),
    }
    assert min(len(ids) for ids in feet.values()) > 100
    action = bpy.data.actions.new('Naiwa_Run')
    action.use_fake_user = True
    rig.animation_data.action = action
    rig.animation_data.use_nla = False
    for bone in rig.pose.bones:
        bone.rotation_mode = 'XYZ'
        bone.location = (0, 0, 0)
        bone.rotation_euler = (0, 0, 0)
        bone.scale = (1, 1, 1)
    samples = []
    # 96 Hz baked poses preserve snappy arm timing and explicit foot phases.
    for sample in range(65):
        frame = 1 + sample / 4
        phase = sample / 64
        for bone in rig.pose.bones:
            bone.location = (0, 0, 0)
            bone.rotation_euler = (0, 0, 0)
        lean = cyclic_table(phase % .5 * 2, [(0, 18), (.18, 20), (.48, 19), (.75, 18), (1, 18)])
        rotate_x(rig.pose.bones['Torso'], lean)
        targets = {}
        for side, offset, outward in [('L', 0.0, 6.0), ('R', .5, -6.0)]:
            limb_phase = (phase + offset) % 1
            targets[side] = leg_ik(rig, side, limb_phase)
            # Opposite hand/foot: the arm swings backward while its foot lands in front.
            swing = cyclic_table(limb_phase, [(0, 32), (.16, 45), (.37, 8), (.54, -65), (.68, -61), (.86, -12), (1, 32)])
            elbow = cyclic_table(limb_phase, [(0, -70), (.16, -82), (.38, -65), (.54, -52), (.72, -57), (.90, -65), (1, -70)])
            rotate_x(rig.pose.bones['UpperArm.' + side], swing, outward)
            rotate_x(rig.pose.bones['Forearm.' + side], elbow)
        bpy.context.view_layer.update()
        posed = posed_feet(body, feet)
        leg_phase = {'L': phase % 1, 'R': (phase + .5) % 1}
        support = next((side for side, value in leg_phase.items() if value <= .16), None)
        if support:
            desired_height = .008
            root_height = desired_height - float(posed[support][:, 2].min())
        else:
            flight_phase = ((phase % .5) - .16) / .34
            desired_height = .008 + .075 * math.sin(math.pi * max(0, min(1, flight_phase)))
            root_height = desired_height - min(float(row[:, 2].min()) for row in posed.values())
        # Root local Y points world Z. Horizontal components remain exactly zero.
        rig.pose.bones['Root'].location = (0, root_height, 0)
        for bone in rig.pose.bones:
            bone.keyframe_insert(data_path='rotation_euler', frame=frame, group=bone.name)
            if bone.name == 'Root':
                bone.keyframe_insert(data_path='location', frame=frame, group=bone.name)
        bpy.context.view_layer.update()
        posed = posed_feet(body, feet)
        weights = belly_weights(phase)
        for name, weight in weights.items():
            key = body.data.shape_keys.key_blocks[name]
            key.value = weight
            key.keyframe_insert(data_path='value', frame=frame)
        row = {'frame': frame, 'phase': phase, 'support': support, 'torso_lean_deg': lean,
               'root_world_xyz_m': [0, 0, root_height], 'foot_targets_m': targets,
               'morph_weights': weights,
               'feet': {side: {'min_z_m': float(points[:, 2].min()),
                                'mean_y_m': float(points[:, 1].mean()),
                                'contact_y_m': float(points[points[:, 2] < points[:, 2].min()+.005, 1].mean())} for side, points in posed.items()}}
        samples.append(row)
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for key in curve.keyframe_points:
                        key.interpolation = 'LINEAR'
    return action, samples, original_actions


def render_previews(scene, rig, action, quick=False):
    sys.path.insert(0, str(WORKSPACE / 'NaiwaCharacter'))
    from render_head_reshape_comparison import prepare_scene
    from render_head_neck_comparisons import camera
    prepare_scene()
    rig.animation_data.action = action
    scene.render.resolution_x, scene.render.resolution_y = 512, 512
    if hasattr(scene, 'eevee') and hasattr(scene.eevee, 'taa_render_samples'):
        scene.eevee.taa_render_samples = 16
    files = []
    for view, yaw in [('side', 90), ('three_quarter', 45)]:
        folder = OUT / view
        folder.mkdir(exist_ok=True)
        camera(scene, {'type': 'ORTHO', 'yaw': yaw, 'pitch': 0, 'target': [0, -.13, 1.22],
                       'distance': 7, 'scale': 3.45, 'lens': 50, 'resolution': [512, 512]})
        for frame in ([1, 3, 5, 7, 9, 11, 13, 15] if quick else range(1, 17)):
            scene.frame_set(frame)
            scene.render.filepath = str(folder / f'pose_{frame:04d}.png')
            bpy.ops.render.render(write_still=True)
            files.append(str(scene.render.filepath))
    return files


def encode_previews():
    """Native Blender sequencer encodes three exact cycles from saved renders."""
    for view in ['side', 'three_quarter']:
        files = [OUT / view / f'pose_{frame:04d}.png' for frame in range(1, 17)]
        if not all(path.exists() for path in files):
            continue
        bpy.ops.wm.read_factory_settings(use_empty=True)
        scene = bpy.context.scene
        sequence = scene.sequence_editor_create()
        strip = sequence.strips.new_image('Naiwa_Run_preview', str(files[0]), channel=1, frame_start=1)
        for path in files[1:] + files + files:
            strip.elements.append(path.name)
        strip.blend_type = 'REPLACE'
        strip.frame_final_duration = 48
        strip.colorspace_settings.name = 'sRGB'
        scene.frame_start, scene.frame_end = 1, 48
        scene.render.resolution_x = scene.render.resolution_y = 512
        scene.render.resolution_percentage = 100
        scene.render.fps = FPS
        if hasattr(scene.render.image_settings, 'media_type'):
            scene.render.image_settings.media_type = 'VIDEO'
        scene.render.image_settings.file_format = 'FFMPEG'
        scene.render.ffmpeg.format = 'MPEG4'
        scene.render.ffmpeg.codec = 'H264'
        scene.render.ffmpeg.constant_rate_factor = 'MEDIUM'
        scene.render.ffmpeg.ffmpeg_preset = 'GOOD'
        scene.render.use_sequencer = True
        scene.view_settings.view_transform = 'Standard'
        scene.render.filepath = str(OUT / f'Naiwa_Run_{view}.mp4')
        bpy.ops.render.render(animation=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', action='store_true')
    parser.add_argument('--quick', action='store_true')
    parser.add_argument('--no-render', action='store_true')
    parser.add_argument('--encode-only', action='store_true')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    if args.encode_only:
        encode_previews()
        record = json.loads((OUT / 'run_parameters.json').read_text(encoding='utf-8'))
        record['preview_files'] = [str(OUT / view / f'pose_{frame:04d}.png') for view in ['side', 'three_quarter'] for frame in range(1, 17)]
        record['preview_videos'] = [str(OUT / f'Naiwa_Run_{view}.mp4') for view in ['side', 'three_quarter']]
        record['source_hash_unchanged'] = digest(SOURCE) == SOURCE_SHA
        (OUT / 'run_parameters.json').write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf-8')
        print('NAIWA_RUN_PREVIEWS_ENCODED', flush=True)
        return
    assert digest(SOURCE) == SOURCE_SHA
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE), load_ui=False)
    rig = bpy.data.objects['Naiwa_Rig']
    OUT.mkdir(parents=True, exist_ok=True)
    record = probe(rig)
    (OUT / 'source_probe.json').write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf-8')
    if args.probe:
        print(json.dumps(record, ensure_ascii=False), flush=True)
        return
    scene = bpy.context.scene
    scene.render.fps, scene.render.fps_base = FPS, 1.0
    scene.frame_start, scene.frame_end = 1, 17
    body = bpy.data.objects['Naiwa unified golden body']
    morphs = create_belly_morphs(body)
    action, samples, original_actions = make_run(rig, body)
    scene.frame_set(1)
    output_blend = OUT / 'Naiwa_E2_Run.blend'
    bpy.ops.wm.save_as_mainfile(filepath=str(output_blend))
    bpy.ops.object.select_all(action='DESELECT')
    rig.hide_set(False)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    output_fbx = OUT / 'Naiwa_Run.fbx'
    bpy.ops.export_scene.fbx(filepath=str(output_fbx), use_selection=True,
        object_types={'ARMATURE'}, add_leaf_bones=False, apply_unit_scale=True,
        bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
        bake_anim_step=.25, bake_anim_simplify_factor=0.0, path_mode='AUTO')
    meshes = [obj for obj in scene.objects if obj.type == 'MESH' and any(mod.type == 'ARMATURE' and mod.object == rig for mod in obj.modifiers)]
    for mesh in meshes:
        mesh.hide_set(False)
        mesh.select_set(True)
    full_fbx = OUT / 'Naiwa_Elastic_Run.fbx'
    bpy.ops.export_scene.fbx(filepath=str(full_fbx), use_selection=True,
        object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False, apply_unit_scale=True,
        use_mesh_modifiers=False, bake_anim=True, bake_anim_use_all_actions=False,
        bake_anim_use_nla_strips=False, bake_anim_step=.25, bake_anim_simplify_factor=0.0, path_mode='AUTO')
    record.update({'source': str(SOURCE), 'source_sha256': SOURCE_SHA,
                   'blend_copy': str(output_blend), 'blend_sha256': digest(output_blend),
                   'fbx_animation_only': str(output_fbx), 'fbx_sha256': digest(output_fbx),
                   'fbx_mesh_morph_single_run': str(full_fbx), 'fbx_mesh_morph_sha256': digest(full_fbx),
                   'morphs': morphs, 'belly_mask_source_coordinates': {'center': [0, -.065, 1.10], 'xz_ellipse_radii': [.72, .73], 'z_window': [.69, 1.70], 'x_window': [-.635, .635], 'torso_skin_weights_only': True},
                   'morph_timing': {'two_bounces_per_run_cycle': True, 'compression_peak_cycle_phase': [.11, .61], 'rebound_peak_cycle_phase': [.31, .81], 'soft_lag_signed': True},
                   'original_actions_retained': original_actions, 'new_action': action.name,
                   'frame_start': 1, 'frame_end': 17, 'fps': FPS, 'cycle_duration_s': CYCLE,
                   'character_game_scale': GAME_SCALE, 'intended_capsule_speed_cm_s': 300,
                   'rig_hierarchy_unchanged': True, 'mesh_geometry_unchanged': True,
                   'root_horizontal_motion_m': 0.0,
                   'vertical_motion_bone': 'Root (below static FBX Naiwa_Rig root)',
                   'samples': samples,
                   'limitations': ['No ankle/foot bone in original rig; shin counter-rotation holds the existing foot flat during short support.',
                                   'UE integration and real gameplay perception require separate validation.']})
    (OUT / 'run_parameters.json').write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf-8')
    if not args.no_render:
        record['preview_files'] = render_previews(scene, rig, action, args.quick)
        if not args.quick:
            encode_previews()
    record['source_hash_unchanged'] = digest(SOURCE) == SOURCE_SHA
    (OUT / 'run_parameters.json').write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf-8')
    assert digest(SOURCE) == SOURCE_SHA
    print('NAIWA_RUN_BUILD_OK', flush=True)


if __name__ == '__main__':
    main()
