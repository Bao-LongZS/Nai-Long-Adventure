"""Import the pinned E2 FBX in UE 5.8; preserve source and report actual assets."""
import hashlib
import json
import pathlib
import traceback
import unreal

ROOT = pathlib.Path(__file__).resolve().parents[2]
RECORDS = ROOT / 'UE5' / 'Records'
RECORDS.mkdir(parents=True, exist_ok=True)
SOURCE = ROOT / 'NaiwaCharacter/versions/20261007_H2_E2_overall_refinement/candidate_E2/Naiwa_Character.fbx'
EXPECTED = '8dae9ce28c4b44465dcb8e73dd462804c3a561daee9eebaf79842900a3005147'
report = {'phase': 'import', 'engine': unreal.SystemLibrary.get_engine_version(),
          'source': str(SOURCE), 'importer': 'Legacy FBX', 'warnings': []}

def prop(obj, name, value, required=True):
    try:
        obj.set_editor_property(name, value)
    except Exception as exc:
        report['warnings'].append(f'{type(obj).__name__}.{name}: {exc}')
        if required:
            raise

def save_report():
    (RECORDS / 'import_e2.json').write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')

try:
    report['source_sha256'] = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    assert report['source_sha256'] == EXPECTED, 'Pinned E2 FBX hash differs'
    options = unreal.FbxImportUI()
    prop(options, 'automated_import_should_detect_type', False)
    prop(options, 'mesh_type_to_import', unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    prop(options, 'import_as_skeletal', True)
    prop(options, 'import_mesh', True)
    prop(options, 'import_animations', True)
    prop(options, 'create_physics_asset', False)
    prop(options, 'import_materials', False)
    prop(options, 'import_textures', False)
    skin = options.get_editor_property('skeletal_mesh_import_data')
    prop(skin, 'vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    prop(skin, 'normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    prop(skin, 'import_uniform_scale', 1.0)
    prop(skin, 'convert_scene', True)
    prop(skin, 'convert_scene_unit', True)
    prop(skin, 'update_skeleton_reference_pose', False)
    prop(skin, 'use_t0_as_ref_pose', False)
    animation = options.get_editor_property('anim_sequence_import_data')
    prop(animation, 'use_default_sample_rate', False)
    prop(animation, 'animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    task = unreal.AssetImportTask()
    for name, value in dict(filename=str(SOURCE), destination_path='/Game/Naiwa',
                            destination_name='SK_Naiwa', automated=True,
                            replace_existing=False, save=True, options=options).items():
        prop(task, name, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    report['imported_paths'] = list(task.get_editor_property('imported_object_paths'))
    assets = []
    for path in unreal.EditorAssetLibrary.list_assets('/Game/Naiwa', recursive=True, include_folder=False):
        obj = unreal.load_asset(path)
        if obj is None:
            continue
        item = {'path': obj.get_path_name(), 'class': obj.get_class().get_name()}
        if isinstance(obj, unreal.AnimSequence):
            for action in ('Idle', 'Walk', 'Jump', 'Laugh'):
                if f'Naiwa_{action}' in obj.get_name():
                    desired = '/Game/Naiwa/AS_Naiwa_' + action
                    if obj.get_path_name().split('.')[0] != desired:
                        unreal.EditorAssetLibrary.rename_asset(obj.get_path_name(), desired)
                        obj = unreal.load_asset(desired)
                    item.update(path=obj.get_path_name(), action=action,
                                duration=obj.get_play_length(), skeleton=obj.get_editor_property('skeleton').get_path_name())
                    break
        if isinstance(obj, unreal.SkeletalMesh):
            item['skeleton'] = obj.get_editor_property('skeleton').get_path_name()
            item['materials'] = [str(m.get_editor_property('material_slot_name')) for m in obj.get_editor_property('materials')]
            for method in ('get_bounds', 'get_imported_bounds'):
                if hasattr(obj, method):
                    bounds = getattr(obj, method)()
                    item['bounds'] = {'origin': list(bounds.origin.to_tuple()), 'extent': list(bounds.box_extent.to_tuple())}
                    break
            if 'bounds' not in item:
                try:
                    bounds = obj.get_editor_property('imported_bounds')
                    item['bounds'] = {'origin': [bounds.origin.x, bounds.origin.y, bounds.origin.z],
                                      'extent': [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z]}
                except Exception as exc:
                    item['bounds_error'] = str(exc)
            item['available_mesh_methods'] = [x for x in dir(obj) if any(k in x for k in ('lod', 'bounds', 'vertex', 'material'))]
        assets.append(item)
    report['assets'] = assets
    report['api'] = {name: [x for x in dir(getattr(unreal, name)) if not x.startswith('_')]
                     for name in ('EditorSkeletalMeshLibrary', 'AnimBlueprintFactory', 'InputMappingContext', 'InputAction')
                     if hasattr(unreal, name)}
    unreal.EditorAssetLibrary.save_directory('/Game/Naiwa', only_if_is_dirty=False, recursive=True)
    report['source_unchanged'] = hashlib.sha256(SOURCE.read_bytes()).hexdigest() == EXPECTED
    report['status'] = 'imported_pending_UE_visual_runtime_validation'
    assert any(a['class'] == 'SkeletalMesh' for a in assets), 'No SkeletalMesh imported'
    assert {a.get('action') for a in assets} >= {'Idle', 'Walk', 'Jump', 'Laugh'}, 'Not all four clips imported'
    save_report()
    unreal.log('NAIWA_E2_IMPORT_OK')
except Exception:
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
    save_report()
    unreal.log_error(report['error'])
    raise
