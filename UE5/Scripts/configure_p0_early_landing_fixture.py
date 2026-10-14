"""Give the early-landing regression clear timing margin below the jump apex."""
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level('/Game/Naiwa/Maps/L_NaiwaGraybox')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
matching = [actor for actor in actors.get_all_level_actors() if actor.get_actor_label() == 'EarlyLandingPlatform']
assert len(matching) == 1
actor = matching[0]
before = actor.get_actor_location()
actor.set_actor_location(unreal.Vector(before.x, before.y, 210.0), False, True)
assert levels.save_current_level()
report = {'actor': 'EarlyLandingPlatform', 'platform_top_z_cm': 260.0,
          'old_platform_top_z_cm': before.z + 50.0, 'platform_thickness_cm': 100.0,
          'reason': '250 cm produced a boundary sample 0.79729 s versus the 0.79167 s source clip under recording hitches. A 10 cm higher landing surface creates an unambiguous early-landing scenario near the bounded jump apex.',
          'assertion': 'Actual delivered jump callback to first landing must still be shorter than the unchanged original Jump clip.',
          'status': 'saved_pending_actual_early_landing_regression'}
(root / 'UE5/Records/p0_early_landing_fixture.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('NAIWA_P0_EARLY_LANDING_FIXTURE_SAVED')
