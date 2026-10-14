"""Add a restrained shadowless camera-side fill for the P0 graybox character."""
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/Naiwa/Maps/L_NaiwaGraybox')
matching = [a for a in actors.get_all_level_actors() if a.get_actor_label() == 'P0_CharacterReadabilityFill']
assert len(matching) <= 1
fill = matching[0] if matching else actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 500, 600))
assert isinstance(fill, unreal.DirectionalLight)
fill.set_actor_label('P0_CharacterReadabilityFill')
fill.set_editor_property('tags', [unreal.Name('NaiwaP0Generated')])
fill.set_actor_rotation(unreal.Rotator(-25, -90, 0), False)
component = fill.get_component_by_class(unreal.DirectionalLightComponent)
component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
component.set_editor_property('intensity', 0.85)
component.set_editor_property('cast_shadows', False)
assert levels.save_current_level()
report = {'map': '/Game/Naiwa/Maps/L_NaiwaGraybox', 'actor': fill.get_actor_label(),
          'intensity': 0.85, 'rotation_pitch_yaw_roll': [-25, -90, 0], 'cast_shadows': False,
          'reason': 'Old frame_000123: platform shadow hides face/belly/foot color. Keep existing key light and add weaker camera-side fill.',
          'status': 'saved_pending_rendered_review', 'source_E2_changed': False}
(root / 'UE5/Records/p0_scene_settings.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('NAIWA_P0_REVIEW_FILL_SAVED')
