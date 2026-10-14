"""Create the saved P0 materials, input assets, blueprints and graybox map."""
import json
import pathlib
import traceback
import unreal

ROOT = pathlib.Path(__file__).resolve().parents[2]
RECORDS = ROOT / 'UE5/Records'
report = {'phase': 'saved_graybox', 'engine': unreal.SystemLibrary.get_engine_version(), 'assets': {}, 'parameters': {}}
tools = unreal.AssetToolsHelpers.get_asset_tools()

def create(name, folder, cls, factory):
    path = folder + '/' + name
    return unreal.load_asset(path) or tools.create_asset(name, folder, cls, factory)

def material(name, color=None, roughness=0.65):
    mat = create(name, '/Game/Naiwa/Materials', unreal.Material, unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    if color is None:
        expression = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -400, 0)
        # UE 5.8 VertexColor's RGB output is unnamed; an empty name selects pin 0.
        output = ''
    else:
        expression = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
        expression.set_editor_property('constant', unreal.LinearColor(*color, 1.0))
        output = ''
    assert unreal.MaterialEditingLibrary.connect_material_property(expression, output, unreal.MaterialProperty.MP_BASE_COLOR)
    for value, prop in ((roughness, unreal.MaterialProperty.MP_ROUGHNESS), (0.2, unreal.MaterialProperty.MP_SPECULAR)):
        scalar = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 200)
        scalar.set_editor_property('r', value)
        assert unreal.MaterialEditingLibrary.connect_material_property(scalar, '', prop)
    unreal.MaterialEditingLibrary.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, True)
    errors = unreal.MaterialEditingLibrary.recompile_material(mat)
    if errors:
        raise RuntimeError(f'{name}: material compile errors: {errors}')
    return mat

def data_asset(name, cls):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', cls)
    return create(name, '/Game/Naiwa/Input', cls, factory)

def blueprint(name, parent):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', parent)
    return create(name, '/Game/Naiwa/Blueprints', unreal.Blueprint, factory)

def default(bp):
    assert unreal.BlueprintEditorLibrary.compile_blueprint(bp), f'Blueprint compilation failed: {bp.get_path_name()}'
    return unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(bp))

try:
    mesh = unreal.load_asset('/Game/Naiwa/SK_Naiwa')
    assert mesh is not None
    skeleton = mesh.get_editor_property('skeleton')
    mats = {'body': material('M_NaiwaBody'), 'eye': material('M_NaiwaEye', roughness=0.4),
            'glint': material('M_NaiwaGlint', (0.95, 0.96, 0.85), 0.15),
            'mouth': material('M_NaiwaMouth', (0.035, 0.014, 0.010), 0.85)}
    slots = mesh.get_editor_property('materials')
    for index, slot in enumerate(slots):
        name = str(slot.get_editor_property('material_slot_name')).lower()
        key = 'glint' if 'glint' in name else 'eye' if 'eye surface' in name else 'mouth' if 'mouth' in name else 'body'
        slot.set_editor_property('material_interface', mats[key])
        # Unreal Array struct iteration yields a copy; write the changed struct back.
        slots[index] = slot
        report['assets'].setdefault('material_slots', []).append({'slot': name, 'material': mats[key].get_path_name()})
    mesh.set_editor_property('materials', slots)

    actions = {}
    for label, kind in (('Move', unreal.InputActionValueType.AXIS1D), ('Jump', unreal.InputActionValueType.BOOLEAN),
                        ('Pause', unreal.InputActionValueType.BOOLEAN), ('Reset', unreal.InputActionValueType.BOOLEAN)):
        action = data_asset('IA_Naiwa' + label, unreal.InputAction)
        action.set_editor_property('value_type', kind)
        action.set_editor_property('trigger_when_paused', label in ('Pause', 'Reset'))
        actions[label] = action
    mapping = data_asset('IMC_Naiwa', unreal.InputMappingContext)
    entries = []
    for label, key, negate in [('Move', 'D', False), ('Move', 'Right', False), ('Move', 'A', True), ('Move', 'Left', True),
                                ('Move', 'Gamepad_LeftX', False), ('Jump', 'SpaceBar', False), ('Jump', 'Gamepad_FaceButton_Bottom', False),
                                ('Pause', 'Escape', False), ('Pause', 'P', False), ('Reset', 'R', False)]:
        input_key = unreal.Key()
        input_key.set_editor_property('key_name', key)
        entry = unreal.EnhancedActionKeyMapping(action=actions[label], key=input_key)
        if negate:
            modifier = unreal.InputModifierNegate(outer=mapping)
            modifier.set_editor_properties({'x': True, 'y': False, 'z': False})
            entry.set_editor_property('modifiers', [modifier])
        entries.append(entry)
    mapping.set_editor_property('default_key_mappings', unreal.InputMappingContextMappingData(mappings=entries))

    anim_parent = unreal.load_class(None, '/Script/NaiwaRuntime.NaiwaAnimInstance')
    character_parent = unreal.load_class(None, '/Script/NaiwaRuntime.NaiwaCharacter')
    game_parent = unreal.load_class(None, '/Script/NaiwaRuntime.NaiwaGameMode')
    assert anim_parent and character_parent and game_parent
    anim_factory = unreal.AnimBlueprintFactory()
    anim_factory.set_editor_properties({'parent_class': anim_parent, 'target_skeleton': skeleton,
                                       'preview_skeletal_mesh': mesh, 'template': False})
    abp = create('ABP_Naiwa', '/Game/Naiwa/Blueprints', unreal.AnimBlueprint, anim_factory)
    anim_default = default(abp)
    sequences = {label: unreal.load_asset('/Game/Naiwa/AS_Naiwa_' + label) for label in ('Idle', 'Walk', 'Jump', 'Laugh')}
    for label, sequence in sequences.items():
        assert sequence
        anim_default.set_editor_property(label.lower() + '_sequence', sequence)
    anim_default.set_editor_properties({'transition_seconds': 0.12, 'walk_reference_speed': 300.0, 'jump_air_hold_fraction': 0.55})
    assert unreal.BlueprintEditorLibrary.compile_blueprint(abp)

    bp = blueprint('BP_NaiwaCharacter', character_parent)
    character_default = default(bp)
    bounds = mesh.get_imported_bounds()
    imported_height = 2.0 * bounds.box_extent.z
    scale = 180.0 / imported_height
    source_min_z = bounds.origin.z - bounds.box_extent.z
    properties = {'skeletal_mesh_asset': mesh, 'animation_class': unreal.BlueprintEditorLibrary.generated_class(abp),
                  'reference_height_cm': 180.0, 'mesh_scale': unreal.Vector(scale, scale, scale),
                  'mesh_offset': unreal.Vector(0.0, 0.0, -90.0 - source_min_z * scale),
                  'collision_radius_cm': 42.0, 'collision_half_height_cm': 90.0,
                  'mesh_right_yaw': -90.0, 'input_mapping': mapping}
    for label, sequence in sequences.items():
        properties[label.lower() + '_sequence'] = sequence
    for label, action in actions.items():
        properties[label.lower() + '_action'] = action
    character_default.set_editor_properties(properties)
    assert unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    game = blueprint('BP_NaiwaGameMode', game_parent)
    game_default = default(game)
    game_default.set_editor_property('default_pawn_class', unreal.BlueprintEditorLibrary.generated_class(bp))
    assert unreal.BlueprintEditorLibrary.compile_blueprint(game)
    report['parameters'] = {'imported_height_cm': imported_height, 'height_cm': 180.0, 'mesh_uniform_scale': scale,
                            'source_min_z_cm': source_min_z, 'mesh_offset_z_cm': properties['mesh_offset'].z,
                            'capsule_radius_cm': 42.0, 'capsule_half_height_cm': 90.0,
                            'plane_y_cm': 0.0, 'plane_y_tolerance_cm': 0.1, 'idle_foot_gap_tolerance_cm': 3.0,
                            'stop_speed_tolerance_cm_s': 1.0, 'target_fps': 60, 'resolution': [1920, 1080],
                            'move_speed_cm_s': 300.0, 'jump_z_velocity_cm_s': 650.0, 'gravity_scale': 1.6,
                            'animation_transition_seconds': 0.12, 'mesh_right_yaw_degrees': -90.0,
                            'camera_relative_location_cm': [0.0, 900.0, 40.0],
                            'camera_relative_rotation_degrees': [0.0, -90.0, 0.0], 'camera_fov_degrees': 40.0}
    unreal.EditorAssetLibrary.save_directory('/Game/Naiwa', only_if_is_dirty=False, recursive=True)

    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    map_path = '/Game/Naiwa/Maps/L_NaiwaGraybox'
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        assert levels.load_level(map_path)
        for actor in actors.get_all_level_actors():
            if 'NaiwaP0Generated' in [str(tag) for tag in actor.tags]:
                assert actors.destroy_actor(actor)
    else:
        assert levels.new_level(map_path)
    def own(actor, label):
        actor.set_actor_label(label)
        actor.set_editor_property('tags', [unreal.Name('NaiwaP0Generated')])
        return actor
    cube = unreal.load_asset('/Engine/BasicShapes/Cube')
    floor_mat = material('M_GrayboxFloor', (0.12, 0.22, 0.28), 0.8)
    platform_mat = material('M_GrayboxPlatform', (0.24, 0.45, 0.5), 0.75)
    geometry = []
    def block(label, x, z_top, width, depth=700.0, thickness=100.0, mat=platform_mat):
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, 0.0, z_top - thickness / 2.0))
        own(actor, label)
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        component.set_static_mesh(cube)
        component.set_material(0, mat)
        actor.set_actor_scale3d(unreal.Vector(width / 100.0, depth / 100.0, thickness / 100.0))
        geometry.append({'name': label, 'x': x, 'top_z': z_top, 'width': width, 'depth': depth, 'thickness': thickness})
    block('Ground', 1700.0, 0.0, 5800.0, 1200.0, 100.0, floor_mat)
    block('Wall_Right', 900.0, 180.0, 80.0, 450.0, 180.0)
    block('Wall_Left', -700.0, 300.0, 80.0, 450.0, 300.0)
    block('LowPlatform', 1150.0, 80.0, 200.0)
    block('JumpPlatform', 1700.0, 180.0, 400.0)
    block('DropPlatform', 2350.0, 300.0, 350.0)
    block('EarlyLandingPlatform', 3750.0, 250.0, 300.0)
    player = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0.0, 0.0, 92.2))
    own(player, 'Naiwa_P0_Start')
    light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(-45, -30, 0))
    own(light, 'P0_KeyLight')
    light.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('intensity', 3.0)
    own(actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 600)), 'P0_SkyLight')
    own(actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)), 'P0_Sky')
    world = unreal.EditorLevelLibrary.get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', unreal.BlueprintEditorLibrary.generated_class(game))
    assert levels.save_current_level()
    unreal.EditorAssetLibrary.save_directory('/Game/Naiwa', only_if_is_dirty=False, recursive=True)
    report['geometry'] = geometry
    report['assets'].update(mesh=mesh.get_path_name(), skeleton=skeleton.get_path_name(),
                            character=bp.get_path_name(), animation_blueprint=abp.get_path_name(),
                            game_mode=game.get_path_name(), input_mapping=mapping.get_path_name(), map='/Game/Naiwa/Maps/L_NaiwaGraybox')
    report['status'] = 'saved_pending_visual_and_runtime_validation'
    (RECORDS / 'build_graybox.json').write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
    unreal.log('NAIWA_GRAYBOX_BUILD_OK')
except Exception:
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
    (RECORDS / 'build_graybox.json').write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
    unreal.log_error(report['error'])
    raise
