"""Change only the saved keyboard movement mapping; keep the P0 scene/assets."""
import json
import shutil
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
records = root / 'UE5/Records/p0_completion_20261013'
records.mkdir(parents=True, exist_ok=True)
mapping = unreal.load_asset('/Game/Naiwa/Input/IMC_Naiwa')
move = unreal.load_asset('/Game/Naiwa/Input/IA_NaiwaMove')
assert mapping and move

asset_file = root / 'UE5/NaiwaP0/Content/Naiwa/Input/IMC_Naiwa.uasset'
backup = records / 'IMC_Naiwa_before_shift.uasset'
if not backup.exists():
    shutil.copy2(asset_file, backup)

def describe(entries):
    result = []
    for entry in entries:
        action = entry.get_editor_property('action')
        result.append({
            'key': str(entry.get_editor_property('key').get_editor_property('key_name')),
            'action': action.get_path_name() if action else None,
            'modifiers': [m.get_class().get_name() for m in entry.get_editor_property('modifiers')],
            'triggers': [t.get_class().get_name() for t in entry.get_editor_property('triggers')],
        })
    return result

data = mapping.get_editor_property('default_key_mappings')
old_entries = list(data.get_editor_property('mappings'))
before = describe(old_entries)
gate_path = '/Game/Naiwa/Input/IA_NaiwaShiftGate'
gate = unreal.load_asset(gate_path)
if not gate:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.InputAction)
    gate = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'IA_NaiwaShiftGate', '/Game/Naiwa/Input', unreal.InputAction, factory)
assert gate
gate.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
gate.set_editor_property('trigger_when_paused', False)

entries = []
for entry in old_entries:
    action = entry.get_editor_property('action')
    key = str(entry.get_editor_property('key').get_editor_property('key_name'))
    if action == gate or (action == move and key in ('A', 'D', 'Left', 'Right')):
        continue
    entries.append(entry)

def key_mapping(action, name):
    key = unreal.Key()
    key.set_editor_property('key_name', name)
    return unreal.EnhancedActionKeyMapping(action=action, key=key)

for name in ('LeftShift', 'RightShift'):
    entries.append(key_mapping(gate, name))
for name in ('Left', 'Right'):
    entry = key_mapping(move, name)
    chord = unreal.InputTriggerChordAction(outer=mapping)
    chord.set_editor_property('chord_action', gate)
    entry.set_editor_property('triggers', [chord])
    if name == 'Left':
        negate = unreal.InputModifierNegate(outer=mapping)
        negate.set_editor_properties({'x': True, 'y': False, 'z': False})
        entry.set_editor_property('modifiers', [negate])
    entries.append(entry)
mapping.set_editor_property('default_key_mappings', unreal.InputMappingContextMappingData(mappings=entries))
assert unreal.EditorAssetLibrary.save_loaded_asset(gate, only_if_is_dirty=False)
assert unreal.EditorAssetLibrary.save_loaded_asset(mapping, only_if_is_dirty=False)
after_entries = list(mapping.get_editor_property('default_key_mappings').get_editor_property('mappings'))
after = describe(after_entries)
assert not any(e['action'] == move.get_path_name() and e['key'] in ('A', 'D') for e in after)
for name in ('Left', 'Right'):
    matches = [e for e in after_entries if e.get_editor_property('action') == move and
               str(e.get_editor_property('key').get_editor_property('key_name')) == name]
    assert len(matches) == 1
    triggers = matches[0].get_editor_property('triggers')
    assert len(triggers) == 1 and triggers[0].get_editor_property('chord_action') == gate
preserved_before = [e for e in before if e['action'] != move.get_path_name() and e['action'] != gate.get_path_name()]
preserved_after = [e for e in after if e['action'] != move.get_path_name() and e['action'] != gate.get_path_name()]
assert preserved_before == preserved_after
report = {
    'status': 'saved_shift_plus_arrows_pending_runtime_validation',
    'keyboard_movement': 'Hold either Shift and Left/Right; A/D removed; arrows alone do not move.',
    'other_controls_preserved': True,
    'scene_character_and_animation_unchanged': True,
    'mapping': mapping.get_path_name(), 'gate_action': gate.get_path_name(),
    'before': before, 'after': after, 'backup': str(backup),
}
(records / 'shift_input_mapping.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('NAIWA_SHIFT_MAPPING_SAVED')
