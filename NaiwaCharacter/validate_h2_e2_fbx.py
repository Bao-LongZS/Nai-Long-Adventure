"""Reimport H2/E2 FBX and independently compare real REST surfaces and skinning.

The six bound meshes are matched by name, but neither vertex order nor FBX's
vertex splitting is assumed. Every REST vertex and triangle centroid is checked
in both directions. Actions are baked by FBX, so complete clip/bone coverage and
sampled world deformation matrices are checked rather than identical F-curves.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sys
import bpy
import numpy as np
from mathutils import Vector
from mathutils.kdtree import KDTree

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from geometry_diagnostic_utils import mesh_record, array

DEFAULT_FOLDER = ROOT / 'versions/20261007_H2_E2_overall_refinement/candidate_H2'
SURFACE_TOLERANCE = 1e-5
WEIGHT_TOLERANCE = 1e-5
ACTION_MATRIX_TOLERANCE = 1e-3
ACTION_NAMES = ('Naiwa_Idle', 'Naiwa_Walk', 'Naiwa_Laugh', 'Naiwa_Jump')
COLOR_TOLERANCE=1e-5


def eye_triangle_colors(obj, srgb_byte_quantization=False):
    """Actual triangle-corner EyeColor means, not material names alone."""
    attribute=obj.data.color_attributes.get('EyeColor')
    if attribute is None:
        return None
    obj.data.calc_loop_triangles()
    colors=array(attribute.data,'color',4,np.float32).astype(float)
    if srgb_byte_quantization:
        # Blender's FBX importer uses BYTE_COLOR: the documented usable color
        # representation stores 8-bit sRGB, then exposes decoded linear RGB.
        # Predict that representation rather than treating quantization as a
        # displaced pupil or silently widening the numerical tolerance.
        rgb=colors[:,:3]
        srgb=np.where(rgb<=.0031308,12.92*rgb,1.055*np.maximum(rgb,0.)**(1./2.4)-.055)
        srgb=np.rint(np.clip(srgb,0.,1.)*255.)/255.
        colors[:,:3]=np.where(srgb<=.04045,srgb/12.92,((srgb+.055)/1.055)**2.4)
        colors[:,3]=np.rint(np.clip(colors[:,3],0.,1.)*255.)/255.
    if attribute.domain=='CORNER':
        indices=array(obj.data.loop_triangles,'loops',3,np.int32)
    else:
        assert attribute.domain=='POINT'
        indices=array(obj.data.loop_triangles,'vertices',3,np.int32)
    return colors[indices].mean(axis=1).astype(float)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tree_for(points):
    tree = KDTree(len(points))
    for index, point in enumerate(points):
        tree.insert(Vector(point), index)
    tree.balance()
    return tree


def nearest_points(points, tree):
    indices = np.empty(len(points), dtype=np.int32)
    distances = np.empty(len(points), dtype=float)
    for index, point in enumerate(points):
        _, neighbor, distance = tree.find(Vector(point))
        indices[index] = neighbor
        distances[index] = distance
    return indices, distances


def nearest_surface(points, bvh):
    distances = np.empty(len(points), dtype=float)
    for index, point in enumerate(points):
        result = bvh.find_nearest(Vector(point))
        assert result[0] is not None, 'Empty triangle surface'
        distances[index] = result[3]
    return distances


def weight_matrix(obj, bone_names):
    bone_indices = {name: i for i, name in enumerate(bone_names)}
    result = np.zeros((len(obj.data.vertices), len(bone_names)), dtype=float)
    for vertex in obj.data.vertices:
        for group in vertex.groups:
            name = obj.vertex_groups[group.group].name
            if name in bone_indices:
                result[vertex.index, bone_indices[name]] += group.weight
    sums = result.sum(axis=1)
    assert (sums > 0).all(), ('Exported bound mesh has an unbound vertex', obj.name)
    # Effective skinning uses normalized positive deform-bone weights.
    return result / sums[:, None], sums


def action_curves(action):
    result = []
    if hasattr(action, 'fcurves'):
        result.extend(action.fcurves)
    for layer in getattr(action, 'layers', []):
        for strip in layer.strips:
            for bag in getattr(strip, 'channelbags', []):
                result.extend(bag.fcurves)
    return result


def action_bones(action):
    result = set()
    for curve in action_curves(action):
        match = re.match(r'pose\.bones\["([^\"]+)"\]', curve.data_path)
        if match:
            result.add(match.group(1))
    return result


def deformation_matrices(rig, rest_world_inverse):
    return {bone.name: np.asarray(rig.matrix_world @ rig.pose.bones[bone.name].matrix
        @ bone.matrix_local.inverted() @ rest_world_inverse, dtype=float)
        for bone in rig.data.bones}


def apply_action(rig, action, frame):
    rig.animation_data_create()
    rig.animation_data.action = action
    rig.animation_data.use_nla = False
    rig.data.pose_position = 'POSE'
    integer_frame = math.floor(float(frame))
    bpy.context.scene.frame_set(integer_frame, subframe=float(frame) - integer_frame)
    bpy.context.view_layer.update()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--blend', type=Path, default=DEFAULT_FOLDER / 'Naiwa_Character.blend')
    parser.add_argument('--fbx', type=Path, default=DEFAULT_FOLDER / 'Naiwa_Character.fbx')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--label', choices=['R1','H2','E2'], default='H2')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    blend, fbx = args.blend.resolve(strict=True), args.fbx.resolve(strict=True)
    output = (args.output or blend.parent / 'fbx_validation.json').resolve()
    report = {'status': 'failed', 'candidate': args.label, 'source_blend': str(blend),
        'fbx': str(fbx), 'source_blend_sha256': digest(blend), 'fbx_sha256': digest(fbx),
        'surface_tolerance_world_units': SURFACE_TOLERANCE,
        'effective_weight_tolerance': WEIGHT_TOLERANCE,
        'baked_action_world_deformation_matrix_tolerance': ACTION_MATRIX_TOLERANCE,
        'mesh_surface_checks': {}, 'action_checks': {}}
    try:
        bpy.ops.wm.open_mainfile(filepath=str(blend))
        rigs = [obj for obj in bpy.data.objects if obj.type == 'ARMATURE']
        assert len(rigs) == 1
        source_rig = rigs[0]
        source_mesh_objects = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH'
            and any(mod.type == 'ARMATURE' and mod.object == source_rig for mod in obj.modifiers)]
        assert len(source_mesh_objects) == 6
        bone_names = sorted(source_rig.data.bones.keys())
        assert len(bone_names) == 10
        source_bones = {bone.name: {'parent': bone.parent.name if bone.parent else None,
            'use_deform': bool(bone.use_deform),
            'world_head': list(source_rig.matrix_world @ bone.head_local)} for bone in source_rig.data.bones}
        source_rig.data.pose_position = 'REST'
        bpy.context.scene.frame_set(1)
        bpy.context.view_layer.update()
        source_rest_world_inverse = source_rig.matrix_world.inverted().copy()
        dg = bpy.context.evaluated_depsgraph_get()
        source_meshes = {}
        for obj in source_mesh_objects:
            record = mesh_record(obj, dg)
            weights, sums = weight_matrix(obj, bone_names)
            assert len(weights) == len(record['points']), ('Unexpected source evaluated topology', obj.name)
            source_meshes[obj.name] = {'record': record, 'weights': weights, 'weight_sums': sums,
                'tree': tree_for(record['points']),
                'eye_triangle_colors':eye_triangle_colors(obj),
                'eye_triangle_colors_srgb_byte':eye_triangle_colors(obj,srgb_byte_quantization=True),
                'color_attributes': {a.name: {'domain': a.domain, 'data_type': a.data_type}
                    for a in obj.data.color_attributes}}
        source_actions = {name: bpy.data.actions[name] for name in ACTION_NAMES}
        assert len(bpy.data.actions) == 4
        source_samples = {}
        source_action_meta = {}
        for name, action in source_actions.items():
            start, end = action.frame_range
            # Preserve true source-quarter positions and include fractional
            # frames between the proposed 0.25-frame baking samples.
            frames = {float(start + t * (end - start)) for t in (0., .25, .5, .75, 1.)}
            frames.update((min(float(end), float(start) + 1.375),
                min(float(end), math.floor((float(start) + float(end)) / 2.) + .125),
                max(float(start), float(end) - .625)))
            frames = sorted(frames)
            source_action_meta[name] = {'frame_range': [float(start), float(end)],
                'curves': len(action_curves(action)), 'animated_bones': sorted(action_bones(action)),
                'sample_frames': frames}
            for frame in frames:
                apply_action(source_rig, action, frame)
                source_samples[(name, frame)] = deformation_matrices(source_rig, source_rest_world_inverse)

        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(fbx), use_anim=True)
        meshes = [obj for obj in bpy.data.objects if obj.type == 'MESH']
        rigs = [obj for obj in bpy.data.objects if obj.type == 'ARMATURE']
        assert len(meshes) == 6 and len(rigs) == 1
        rig = rigs[0]
        assert sorted(rig.data.bones.keys()) == bone_names, 'FBX bone inventory changed'
        assert len(bpy.data.actions) == 4, 'FBX complete action inventory is not four clips'
        imported_actions = {}
        for name in ACTION_NAMES:
            matches = [action for action in bpy.data.actions if action.name.endswith(name)]
            assert len(matches) == 1, ('Missing/duplicate FBX clip', name)
            imported_actions[name] = matches[0]
        for obj in meshes:
            arm = [mod for mod in obj.modifiers if mod.type == 'ARMATURE' and mod.object == rig]
            assert len(arm) == 1, ('FBX mesh effective armature binding missing', obj.name)
        rig.data.pose_position = 'REST'
        # Imported baked clips may contain object transforms. Keep the default
        # import/start frame for the rest comparison, as the exporter did.
        bpy.context.scene.frame_set(1)
        bpy.context.view_layer.update()
        imported_rest_world_inverse = rig.matrix_world.inverted().copy()
        for name, original in source_bones.items():
            bone = rig.data.bones[name]
            assert (bone.parent.name if bone.parent else None) == original['parent'], ('Bone parent changed', name)
            assert bool(bone.use_deform) == original['use_deform'], ('Bone deform flag changed', name)
            head = np.asarray(rig.matrix_world @ bone.head_local)
            assert np.linalg.norm(head - original['world_head']) <= SURFACE_TOLERANCE, ('Bone rest world head changed', name)
        dg = bpy.context.evaluated_depsgraph_get()
        imported_records = {}
        for name, source in source_meshes.items():
            matches = [obj for obj in meshes if obj.name == name or obj.name.startswith(name + '.')]
            assert len(matches) == 1, ('Missing/duplicate FBX exported mesh', name)
            obj = matches[0]
            current = mesh_record(obj, dg)
            imported_records[name] = current
            assert np.isfinite(current['points']).all(), ('Nonfinite FBX actual coordinates', name)
            source_record = source['record']
            current_tree = tree_for(current['points'])
            to_source, forward = nearest_points(current['points'], source['tree'])
            to_imported, backward = nearest_points(source_record['points'], current_tree)
            assert forward.max() <= SURFACE_TOLERANCE, ('FBX-to-native actual REST vertex distance', name, float(forward.max()))
            assert backward.max() <= SURFACE_TOLERANCE, ('Native-to-FBX actual REST vertex distance', name, float(backward.max()))
            # Vertex equivalence alone would miss missing or altered faces.
            # Every actual triangulated face's centroid is also checked against
            # the other actual triangle surface, in both directions.
            current_centroids = current['triangle_points'].mean(axis=1)
            original_centroids = source_record['triangle_points'].mean(axis=1)
            surface_forward = nearest_surface(current_centroids, source_record['bvh'])
            surface_backward = nearest_surface(original_centroids, current['bvh'])
            assert surface_forward.max() <= SURFACE_TOLERANCE, ('FBX actual triangle surface differs', name, float(surface_forward.max()))
            assert surface_backward.max() <= SURFACE_TOLERANCE, ('Native actual triangle surface missing from FBX', name, float(surface_backward.max()))
            current_weights, current_sums = weight_matrix(obj, bone_names)
            assert len(current_weights) == len(current['points']), ('FBX evaluated topology differs from skinned base', name)
            weight_differences = np.max(np.abs(current_weights - source['weights'][to_source]), axis=1)
            ambiguous = np.flatnonzero(weight_differences > WEIGHT_TOLERANCE)
            # Duplicated seam vertices can share coordinates but differ in
            # weights. Choose the matching source among the real coincident
            # source vertices rather than assuming arbitrary KD tie order.
            for index in ambiguous:
                options = source['tree'].find_range(Vector(current['points'][index]), SURFACE_TOLERANCE)
                assert options, ('No native point for imported effective weights', name, int(index))
                weight_differences[index] = min(float(np.max(np.abs(current_weights[index] - source['weights'][neighbor])))
                    for _, neighbor, _ in options)
            assert weight_differences.max() <= WEIGHT_TOLERANCE, ('Effective bone weights changed', name, float(weight_differences.max()))
            reverse_weight_differences = np.max(np.abs(source['weights'] - current_weights[to_imported]), axis=1)
            ambiguous = np.flatnonzero(reverse_weight_differences > WEIGHT_TOLERANCE)
            for index in ambiguous:
                options = current_tree.find_range(Vector(source_record['points'][index]), SURFACE_TOLERANCE)
                assert options, ('No FBX point for native effective weights', name, int(index))
                reverse_weight_differences[index] = min(float(np.max(np.abs(source['weights'][index] - current_weights[neighbor])))
                    for _, neighbor, _ in options)
            assert reverse_weight_differences.max() <= WEIGHT_TOLERANCE, ('Native effective weight variant missing from FBX', name, float(reverse_weight_differences.max()))
            imported_color_names = set(obj.data.color_attributes.keys())
            assert set(source['color_attributes']).issubset(imported_color_names), ('Mesh color attribute dropped', name)
            actual_eye_color_check=None
            if source['eye_triangle_colors'] is not None:
                current_colors=eye_triangle_colors(obj)
                assert current_colors is not None,('Actual eye green/black color data dropped',name)
                color_indices,color_distances=nearest_points(current_centroids,tree_for(original_centroids))
                reverse_indices,reverse_distances=nearest_points(original_centroids,tree_for(current_centroids))
                assert color_distances.max()<=SURFACE_TOLERANCE and reverse_distances.max()<=SURFACE_TOLERANCE, ('Eye face triangulation correspondence unavailable for actual EyeColor audit',name)
                raw_color_delta=np.abs(current_colors-source['eye_triangle_colors'][color_indices])
                byte_representation=obj.data.color_attributes['EyeColor'].data_type=='BYTE_COLOR'
                expected=source['eye_triangle_colors_srgb_byte'] if byte_representation else source['eye_triangle_colors']
                color_delta=np.abs(current_colors-expected[color_indices])
                reverse_color_delta=np.abs(expected-current_colors[reverse_indices])
                assert color_delta.max()<=COLOR_TOLERANCE and reverse_color_delta.max()<=COLOR_TOLERANCE,('Actual exported eye green/black mask or RGB changed',name)
                actual_eye_color_check={'all_actual_triangle_colors_checked_both_directions':True,
                    'maximum_FBX_to_native_RGBA_difference':float(color_delta.max()),
                    'maximum_native_to_FBX_RGBA_difference':float(reverse_color_delta.max()),
                    'maximum_raw_linear_RGBA_quantization_difference':float(raw_color_delta.max()),
                    'FBX_import_BYTE_COLOR':byte_representation,
                    'color_representation_comparison':'linear float source -> 8-bit sRGB -> decoded linear RGB' if byte_representation else 'linear float source unchanged',
                    'actual_green_black_mask_and_RGBA_values_preserved':True,
                    'tolerance':COLOR_TOLERANCE}
            report['mesh_surface_checks'][name] = {
                'native_REST_vertex_count': len(source_record['points']), 'FBX_REST_vertex_count': len(current['points']),
                'native_actual_triangle_count': len(source_record['triangles']), 'FBX_actual_triangle_count': len(current['triangles']),
                'all_vertices_checked_both_directions': True,
                'maximum_FBX_to_native_vertex_distance': float(forward.max()),
                'maximum_native_to_FBX_vertex_distance': float(backward.max()),
                'all_triangle_centroids_checked_both_directions': True,
                'maximum_FBX_to_native_actual_surface_distance': float(surface_forward.max()),
                'maximum_native_to_FBX_actual_surface_distance': float(surface_backward.max()),
                'maximum_effective_bone_weight_difference': float(weight_differences.max()),
                'maximum_native_to_FBX_effective_bone_weight_difference': float(reverse_weight_differences.max()),
                'native_weight_sum_range': [float(source['weight_sums'].min()), float(source['weight_sums'].max())],
                'FBX_weight_sum_range': [float(current_sums.min()), float(current_sums.max())],
                'all_vertices_effectively_bound': True,
                'source_color_attributes': source['color_attributes'],
                'actual_eye_green_black_color_check':actual_eye_color_check,
                'FBX_color_attributes': {a.name: {'domain': a.domain, 'data_type': a.data_type, 'element_count': len(a.data)}
                    for a in obj.data.color_attributes}}
        for name, action in imported_actions.items():
            metadata = source_action_meta[name]
            frame_range = np.asarray(action.frame_range, dtype=float)
            assert np.max(np.abs(frame_range - metadata['frame_range'])) <= .001, ('FBX clip range changed', name, frame_range)
            bones = action_bones(action)
            assert set(metadata['animated_bones']).issubset(bones), ('FBX lost animated bone channels', name)
            assert len(action_curves(action)) > 0, ('Empty FBX action', name)
            samples = []
            for frame in metadata['sample_frames']:
                apply_action(rig, action, frame)
                matrices = deformation_matrices(rig, imported_rest_world_inverse)
                differences_by_bone = {bone: float(np.max(np.abs(matrices[bone] - source_samples[(name, frame)][bone]))) for bone in bone_names}
                maximum = max(differences_by_bone.values())
                assert maximum <= ACTION_MATRIX_TOLERANCE, ('Baked action bone deformation changed', name, frame, differences_by_bone)
                samples.append({'frame': frame, 'maximum_world_deformation_matrix_entry_difference': maximum,
                    'bone_differences': differences_by_bone})
            report['action_checks'][name] = {'FBX_action_name': action.name, 'native': metadata,
                'FBX_frame_range': frame_range.tolist(), 'FBX_curve_count': len(action_curves(action)),
                'FBX_animated_bones': sorted(bones), 'sampled_bone_deformations': samples,
                'complete_clip_and_source_bone_channel_coverage': True}
        report['meshes'] = len(meshes)
        report['bones'] = len(rig.data.bones)
        report['actions'] = sorted(action.name for action in bpy.data.actions)
        report['bone_names'] = bone_names
        report['all_meshes_have_armature_modifier'] = True
        report['source_blend_unchanged'] = digest(blend) == report['source_blend_sha256']
        report['FBX_unchanged'] = digest(fbx) == report['fbx_sha256']
        assert report['source_blend_unchanged'] and report['FBX_unchanged']
        report['status'] = 'passed'
    except Exception as error:
        report['error'] = str(error)
        report['source_blend_unchanged'] = digest(blend) == report['source_blend_sha256']
        report['FBX_unchanged'] = digest(fbx) == report['fbx_sha256']
        raise
    finally:
        output.parent.mkdir(exist_ok=True, parents=True)
        output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print('H2_E2_FBX_SURFACE_REIMPORT_OK', flush=True)


if __name__ == '__main__':
    main()

