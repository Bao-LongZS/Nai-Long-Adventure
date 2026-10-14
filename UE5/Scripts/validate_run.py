"""Read-only audit of imported Run, belly curves and shared reference skeleton."""
import hashlib
import json
import pathlib
import traceback
import unreal

ROOT = pathlib.Path(__file__).resolve().parents[2]
report = {'engine': unreal.SystemLibrary.get_engine_version(), 'checks': {}, 'curves': {}, 'limitations': []}

def check(name, passed, evidence=None):
    report['checks'][name] = {'pass': bool(passed), 'evidence': evidence}

def transform(value):
    return list(value.translation.to_tuple()) + list(value.rotation.to_tuple()) + list(value.scale3d.to_tuple())

try:
    mesh = unreal.load_asset('/Game/Naiwa/Gameplay/SK_Naiwa_Elastic')
    run = unreal.load_asset('/Game/Naiwa/Gameplay/AS_Naiwa_Run')
    original = unreal.load_asset('/Game/Naiwa/SK_Naiwa')
    assert mesh and run and original
    skeleton = original.get_editor_property('skeleton')
    check('shared_skeleton', mesh.get_editor_property('skeleton') == skeleton and run.get_editor_property('skeleton') == skeleton,
          skeleton.get_path_name())
    check('run_duration', abs(run.get_play_length() - 2.0 / 3.0) < 0.0001, run.get_play_length())
    morphs = mesh.get_editor_property('morph_targets')
    names = [m.get_name() for m in morphs]
    report['morph_names'] = names
    required = ['Belly_Compress', 'Belly_Rebound', 'Belly_SoftLag']
    check('three_local_belly_morphs', sorted(names) == sorted(required), names)
    curve_names = [str(n) for n in unreal.AnimationLibrary.get_animation_curve_names(run, unreal.RawCurveTrackTypes.RCT_FLOAT)]
    report['animation_curve_names'] = curve_names
    for name in required:
        times, values = unreal.AnimationLibrary.get_float_keys(run, name)
        samples = [float(unreal.AnimationLibrary.get_float_value_at_time(run, name, index / 96.0)) for index in range(65)]
        report['curves'][name] = {'times': list(times), 'key_values': list(values), 'samples_96hz': samples,
                                'minimum': min(samples), 'maximum': max(samples)}
        check(name + '.animated', name.casefold() in {n.casefold() for n in curve_names} and len(values) > 2 and max(samples) - min(samples) > 0.1)
        check(name + '.seam', abs(samples[0] - samples[-1]) < 0.002, [samples[0], samples[-1]])
    components = []
    for obj in (original, mesh):
        c = unreal.SkeletalMeshComponent()
        c.set_skeletal_mesh_asset(obj)
        components.append(c)
    old, new = components
    bones = [str(old.get_bone_name(i)) for i in range(old.get_num_bones())]
    new_bones = [str(new.get_bone_name(i)) for i in range(new.get_num_bones())]
    check('same_bones', bones == new_bones, new_bones)
    errors = []
    for index in range(min(old.get_num_bones(), new.get_num_bones())):
        a, b = transform(old.get_ref_pose_transform(index)), transform(new.get_ref_pose_transform(index))
        errors.append(max(abs(x - y) for x, y in zip(a, b)))
    check('same_reference_pose', max(errors, default=999) < 0.0001, {'max_transform_component_error': max(errors, default=999)})
    imported = json.loads((ROOT / 'UE5/Records/import_run.json').read_text(encoding='utf-8'))
    for path, hashes in imported['preserved_asset_hashes'].items():
        current = hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()
        check(pathlib.Path(path).stem + '.preserved_package', current == hashes['before'], current)
    bp = unreal.load_asset('/Game/Naiwa/Blueprints/BP_NaiwaCharacter')
    cdo = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(bp))
    check('character_gameplay_mesh', cdo.get_editor_property('skeletal_mesh_asset') == mesh)
    check('character_run', cdo.get_editor_property('run_sequence') == run)
    report['mesh'] = mesh.get_path_name()
    report['run'] = run.get_path_name()
    report['has_vertex_colors'] = mesh.has_vertex_colors()
    material_graphs = []
    for slot in mesh.get_editor_property('materials'):
        mat = slot.get_editor_property('material_interface')
        name = str(slot.get_editor_property('material_slot_name')).casefold()
        if 'glint' in name or 'mouth' in name:
            continue
        expected = 'M_NaiwaEye_Gameplay' if 'eye surface' in name else 'M_NaiwaBody_Gameplay'
        node = unreal.MaterialEditingLibrary.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
        code = node.get_editor_property('code') if isinstance(node, unreal.MaterialExpressionCustom) else ''
        check(expected + '.saved_color_decode', mat.get_name() == expected and '12.92' in code and '2.4' in code and '0.04045' in code,
              mat.get_path_name())
        material_graphs.append({'material': mat.get_path_name(), 'base_color_node': node.get_class().get_name(), 'code': code})
    report['gameplay_material_graphs'] = material_graphs
    report['limitations'] = ['Curve/shape existence does not prove rendered runtime deformation; inspect morph_weights.csv and actual screenshots.',
                             'Reference skeleton comparison checks the six partial-bind-pose import warnings; visual skinning still requires runtime review.']
    report['status'] = 'data_verified_pending_runtime' if all(c['pass'] for c in report['checks'].values()) else 'failed'
    assert report['status'] != 'failed', 'Run import checks failed'
    unreal.log('NAIWA_RUN_AUDIT_OK')
except Exception:
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
    unreal.log_error(report['error'])
finally:
    (ROOT / 'UE5/Records/run_import_validation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
if report['status'] == 'failed':
    raise RuntimeError(report.get('error', 'Run audit failed'))
