"""Import the gameplay Run and local belly morphs without replacing pinned E2."""
import hashlib
import json
import pathlib
import traceback
import unreal

ROOT = pathlib.Path(__file__).resolve().parents[2]
RECORDS = ROOT / 'UE5/Records'
SOURCE = ROOT / 'UE5/Animation/Run_v1/Naiwa_Elastic_Run.fbx'
EXPECTED = 'f5688581ae103f27901bab67a98ba9deef983156ec985308435bb453e2621d2e'
FOLDER = '/Game/Naiwa/Gameplay'
report = {'phase': 'gameplay_run_import', 'engine': unreal.SystemLibrary.get_engine_version(),
          'source': str(SOURCE), 'importer': 'Legacy FBX', 'warnings': []}

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def save():
    (RECORDS / 'import_run.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')

try:
    report['source_sha256'] = digest(SOURCE)
    assert report['source_sha256'] == EXPECTED, 'Run FBX hash differs'
    original_mesh = unreal.load_asset('/Game/Naiwa/SK_Naiwa')
    skeleton = original_mesh.get_editor_property('skeleton')
    preserved = [ROOT / 'UE5/NaiwaP0/Content/Naiwa' / (name + '.uasset')
                 for name in ('SK_Naiwa', 'AS_Naiwa_Idle', 'AS_Naiwa_Walk', 'AS_Naiwa_Jump', 'AS_Naiwa_Laugh')]
    before = {str(path): digest(path) for path in preserved}
    assert not unreal.EditorAssetLibrary.does_asset_exist(FOLDER + '/SK_Naiwa_Elastic'), 'Gameplay mesh already exists; inspect before reimporting'
    options = unreal.FbxImportUI()
    options.set_editor_properties({'automated_import_should_detect_type': False,
                                  'mesh_type_to_import': unreal.FBXImportType.FBXIT_SKELETAL_MESH,
                                  'import_as_skeletal': True, 'import_mesh': True, 'import_animations': True,
                                  'create_physics_asset': False, 'import_materials': False, 'import_textures': False,
                                  'skeleton': skeleton})
    skin = options.get_editor_property('skeletal_mesh_import_data')
    skin.set_editor_properties({'vertex_color_import_option': unreal.VertexColorImportOption.REPLACE,
                                'normal_import_method': unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS,
                                'import_uniform_scale': 1.0, 'convert_scene': True, 'convert_scene_unit': True,
                                'update_skeleton_reference_pose': False, 'use_t0_as_ref_pose': False,
                                'import_morph_targets': True})
    animation = options.get_editor_property('anim_sequence_import_data')
    animation.set_editor_properties({'use_default_sample_rate': False,
                                     'animation_length': unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME,
                                     'import_custom_attribute': True})
    task = unreal.AssetImportTask()
    task.set_editor_properties({'filename': str(SOURCE), 'destination_path': FOLDER,
                                'destination_name': 'SK_Naiwa_Elastic', 'automated': True,
                                'replace_existing': False, 'save': False, 'options': options})
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    report['imported_paths'] = list(task.get_editor_property('imported_object_paths'))
    loaded = [unreal.load_asset(path) for path in unreal.EditorAssetLibrary.list_assets(FOLDER, recursive=True, include_folder=False)]
    meshes = [obj for obj in loaded if isinstance(obj, unreal.SkeletalMesh)]
    clips = [obj for obj in loaded if isinstance(obj, unreal.AnimSequence)]
    assert len(meshes) == 1 and len(clips) == 1, f'Expected 1 mesh and 1 Run; got {len(meshes)}, {len(clips)}'
    mesh, run = meshes[0], clips[0]
    desired_run = FOLDER + '/AS_Naiwa_Run'
    if run.get_path_name().split('.')[0] != desired_run:
        assert unreal.EditorAssetLibrary.rename_asset(run.get_path_name(), desired_run)
        run = unreal.load_asset(desired_run)
    assert mesh.get_editor_property('skeleton') == skeleton and run.get_editor_property('skeleton') == skeleton
    slots = mesh.get_editor_property('materials')
    original_slots = original_mesh.get_editor_property('materials')
    existing = {str(slot.get_editor_property('material_slot_name')).casefold(): slot.get_editor_property('material_interface')
                for slot in original_slots}
    for index, slot in enumerate(slots):
        name = str(slot.get_editor_property('material_slot_name'))
        assert name.casefold() in existing and existing[name.casefold()], f'Unexpected or unassigned slot: {name}'
        slot.set_editor_property('material_interface', existing[name.casefold()])
        slots[index] = slot
    mesh.set_editor_property('materials', slots)
    bounds = mesh.get_imported_bounds()
    original_bounds = original_mesh.get_imported_bounds()
    assert abs(bounds.box_extent.z - original_bounds.box_extent.z) < 0.01, 'Reference height changed'
    bp = unreal.load_asset('/Game/Naiwa/Blueprints/BP_NaiwaCharacter')
    assert unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(bp))
    cdo.set_editor_properties({'skeletal_mesh_asset': mesh, 'run_sequence': run})
    assert unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    # ABP uses the same skeleton; its preview now displays the gameplay mesh.
    abp = unreal.load_asset('/Game/Naiwa/Blueprints/ABP_Naiwa')
    if hasattr(abp, 'set_preview_mesh'):
        abp.set_preview_mesh(mesh)
    assert unreal.EditorAssetLibrary.save_directory(FOLDER, only_if_is_dirty=False, recursive=True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    assert unreal.EditorAssetLibrary.save_loaded_asset(skeleton, only_if_is_dirty=False)
    report.update(mesh=mesh.get_path_name(), run=run.get_path_name(), duration_s=run.get_play_length(),
                  skeleton=skeleton.get_path_name(), character=bp.get_path_name(),
                  material_slots=[{'slot': str(s.get_editor_property('material_slot_name')),
                                   'material': s.get_editor_property('material_interface').get_path_name()} for s in slots],
                  source_unchanged=digest(SOURCE) == EXPECTED,
                  preserved_asset_hashes={str(path): {'before': before[str(path)], 'after': digest(path)} for path in preserved})
    assert all(item['before'] == item['after'] for item in report['preserved_asset_hashes'].values()), 'Pinned mesh/clip package changed'
    report['status'] = 'imported_pending_morph_audit_and_runtime_validation'
    save()
    unreal.log('NAIWA_RUN_IMPORT_OK')
except Exception:
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
    save()
    unreal.log_error(report['error'])
    raise
