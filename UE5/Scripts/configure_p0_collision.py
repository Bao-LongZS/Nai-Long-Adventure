"""Size the saved gameplay capsule from actual UE phase-review surface extents."""
import json
import math
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
source = root / 'UE5/Records/clip_review_20261011/summary.json'
measurements = json.loads(source.read_text(encoding='utf-8'))
extent = max(measurements['maximum_surface_extents_cm'].values())
margin = 4.0
radius = math.ceil(extent) + margin
bp = unreal.load_asset('/Game/Naiwa/Blueprints/BP_NaiwaCharacter')
cdo = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(bp))
old_radius = cdo.get_editor_property('collision_radius_cm')
half_height = cdo.get_editor_property('collision_half_height_cm')
assert radius < half_height, (radius, half_height)
cdo.set_editor_property('collision_radius_cm', radius)
assert unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
reloaded = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(bp))
assert abs(reloaded.get_editor_property('collision_radius_cm') - radius) < 0.001
report = {'phase': 'measured_gameplay_capsule_resize', 'source_measurement': str(source),
          'sampled_poses': measurements['sampled_poses'], 'surface_extent_max_cm': extent,
          'rounding': 'ceil(maximum forward/rear extent) + 4 cm', 'margin_cm': margin,
          'old_radius_cm': old_radius, 'radius_cm': radius, 'half_height_cm': half_height,
          'status': 'saved_pending_wall_platform_and_jump_regression',
          'limits': 'Based on held phase samples. Actual wall motion and platform landing must pass regression; this is not a bound on arbitrary future animation.',
          'source_E2_changed': False}
(root / 'UE5/Records/p0_collision_settings.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('NAIWA_P0_COLLISION_SAVED radius=' + str(radius))
