"""Restore original movement and map Shift to the walk/run toggle only."""
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
records = root / 'UE5/Records/p0_completion_20261013'
mapping = unreal.load_asset('/Game/Naiwa/Input/IMC_Naiwa')
move = unreal.load_asset('/Game/Naiwa/Input/IA_NaiwaMove')
bp = unreal.load_asset('/Game/Naiwa/Blueprints/BP_NaiwaCharacter')
assert mapping and move and bp
gait = unreal.load_asset('/Game/Naiwa/Input/IA_NaiwaGait')
if not gait:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.InputAction)
    gait = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'IA_NaiwaGait', '/Game/Naiwa/Input', unreal.InputAction, factory)
assert gait
gait.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
gait.set_editor_property('trigger_when_paused', False)

entries = []
before = []
for entry in mapping.get_editor_property('default_key_mappings').get_editor_property('mappings'):
    action = entry.get_editor_property('action')
    name = str(entry.get_editor_property('key').get_editor_property('key_name'))
    path = action.get_path_name() if action else ''
    before.append({'action': path, 'key': name})
    if 'IA_NaiwaShiftGate' in path or action == gait:
        continue
    if action and action.get_name() == 'IA_NaiwaPause' and name == 'Escape':
        continue
    if action == move and name in ('A', 'D', 'Left', 'Right'):
        continue
    entries.append(entry)

def key_mapping(action, name):
    key = unreal.Key()
    key.set_editor_property('key_name', name)
    return unreal.EnhancedActionKeyMapping(action=action, key=key)

for name in ('A', 'D', 'Left', 'Right'):
    entry = key_mapping(move, name)
    if name in ('A', 'Left'):
        negate = unreal.InputModifierNegate(outer=mapping)
        negate.set_editor_properties({'x': True, 'y': False, 'z': False})
        entry.set_editor_property('modifiers', [negate])
    entries.append(entry)
for name in ('LeftShift', 'RightShift'):
    entries.append(key_mapping(gait, name))
mapping.set_editor_property('default_key_mappings', unreal.InputMappingContextMappingData(mappings=entries))
assert unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(bp))
cdo.set_editor_property('gait_action', gait)
assert unreal.BlueprintEditorLibrary.compile_blueprint(bp)
for asset in (gait, mapping, bp):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
cdo = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(bp))
assert cdo.get_editor_property('gait_action') == gait
assert cdo.get_editor_property('collision_radius_cm') == 86.0
after = []
for entry in mapping.get_editor_property('default_key_mappings').get_editor_property('mappings'):
    action = entry.get_editor_property('action')
    name = str(entry.get_editor_property('key').get_editor_property('key_name'))
    assert 'IA_NaiwaShiftGate' not in action.get_path_name()
    if action == move and name in ('A', 'D', 'Left', 'Right'):
        assert not entry.get_editor_property('triggers'), name
    after.append({'action': action.get_path_name(), 'key': name})
assert sorted(e['key'] for e in after if e['action'] == move.get_path_name() and e['key'] != 'Gamepad_LeftX') == ['A', 'D', 'Left', 'Right']
assert sorted(e['key'] for e in after if e['action'] == gait.get_path_name()) == ['LeftShift', 'RightShift']
assert not any(e['action'].endswith('IA_NaiwaPause') and e['key'] == 'Escape' for e in after)
report = {
    'status': 'saved_walk_run_toggle_pending_runtime_validation',
    'movement': 'A/D or Left/Right, exactly as before; no Shift chord required',
    'shift': 'Press either Shift once to toggle Walk 150 / Run 300 cm/s; default Run',
    'gait_action': gait.get_path_name(), 'character_gait_action': cdo.get_editor_property('gait_action').get_path_name(),
    'before': before, 'after': after,
    'preserved': ['Jump', 'Pause', 'Reset', 'gamepad mappings', '86cm capsule', 'scene', 'model', 'animations'],
}
records.mkdir(parents=True, exist_ok=True)
(records / 'gait_input_mapping.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('NAIWA_GAIT_MAPPING_SAVED')
