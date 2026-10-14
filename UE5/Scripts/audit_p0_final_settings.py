"""Read saved P0 settings in a fresh editor process; never modify assets."""
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
report = {'engine': unreal.SystemLibrary.get_engine_version(), 'mode': 'fresh_editor_saved_asset_read'}
game_class = unreal.load_class(None, '/Game/Naiwa/Blueprints/BP_NaiwaGameMode.BP_NaiwaGameMode_C')
game = unreal.get_default_object(game_class)
hud = game.get_editor_property('hud_class')
report['hud_class'] = hud.get_path_name() if hud else None
assert report['hud_class'] == '/Script/NaiwaRuntime.NaiwaHUD', report['hud_class']
character_class = game.get_editor_property('default_pawn_class')
character = unreal.get_default_object(character_class)
report['character_class'] = character_class.get_path_name()
report['capsule_radius_cm'] = character.get_editor_property('collision_radius_cm')
report['capsule_half_height_cm'] = character.get_editor_property('collision_half_height_cm')
assert report['capsule_radius_cm'] == 86.0 and report['capsule_half_height_cm'] == 90.0
movement = character.get_editor_property('character_movement')
report['movement_defaults'] = {name: movement.get_editor_property(name) for name in
    ('max_walk_speed', 'max_acceleration', 'braking_deceleration_walking', 'ground_friction',
     'jump_z_velocity', 'gravity_scale', 'air_control', 'max_step_height', 'apply_gravity_while_jumping')}
report['jump_max_hold_time_s'] = character.get_editor_property('jump_max_hold_time')
report['runtime_jump_policy'] = 'Native ApplyCharacterAssets enforces apply_gravity_while_jumping=False.'
report['assets'] = {}
for property_name in ('skeletal_mesh_asset', 'animation_class', 'idle_sequence', 'walk_sequence',
                      'jump_sequence', 'laugh_sequence', 'run_sequence', 'input_mapping'):
    value = character.get_editor_property(property_name)
    assert value, property_name
    report['assets'][property_name] = value.get_path_name()
report['materials'] = []
for key in ('Body', 'Eye'):
    path = '/Game/Naiwa/Gameplay/Materials/M_Naiwa' + key + '_Gameplay'
    material = unreal.load_asset(path)
    skeletal = material.get_editor_property('used_with_skeletal_mesh')
    morph = material.get_editor_property('used_with_morph_targets')
    report['materials'].append({'path': material.get_path_name(), 'skeletal_usage': skeletal, 'morph_usage': morph})
    assert skeletal and morph, path
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level('/Game/Naiwa/Maps/L_NaiwaGraybox')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
fills = [a for a in actors.get_all_level_actors() if a.get_actor_label() == 'P0_CharacterReadabilityFill']
assert len(fills) == 1
light = fills[0].get_component_by_class(unreal.DirectionalLightComponent)
report['readability_fill'] = {'intensity': light.get_editor_property('intensity'),
                              'cast_shadows': light.get_editor_property('cast_shadows')}
assert abs(report['readability_fill']['intensity'] - 0.85) < 0.0001
assert not report['readability_fill']['cast_shadows']
report['status'] = 'passed_saved_HUD_morph_materials_capsule_and_fill'
(root / 'UE5/Records/p0_saved_settings_validation.json').write_text(
    json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('NAIWA_FINAL_SAVED_SETTINGS_OK')
