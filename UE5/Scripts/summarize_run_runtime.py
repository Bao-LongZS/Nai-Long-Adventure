"""Summarize actual runtime evidence; never substitute this for visual review."""
import csv
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--summary', type=Path, default=ROOT / 'UE5/Records/runtime_run_summary.json')
parser.add_argument('--output', type=Path, default=ROOT / 'UE5/Records/run_runtime_validation.json')
args = parser.parse_args()
summary = json.loads(args.summary.read_text(encoding='utf-8-sig'))
directory = Path(summary['output_directory'])
groups = {}
last_context = None
state_start = 0.0
with (directory / 'morph_weights.csv').open(encoding='utf-8-sig', newline='') as stream:
    for row in csv.DictReader(stream):
        state, name = row['state'], row['morph']
        context = (row['round'], row['case'], state)
        if context != last_context:
            state_start = float(row['time_s'])
            last_context = context
        # Idle samples near a transition can contain a valid 0.12-second crossfade.
        if state == 'Idle' and float(row['time_s']) - state_start < 0.2:
            continue
        group = groups.setdefault((state, name), {'samples': 0, 'minimum': float('inf'), 'maximum': -float('inf'),
                                                 'maximum_curve_component_error': 0.0})
        final = float(row['final_component_weight'])
        group['samples'] += 1
        group['minimum'] = min(group['minimum'], final)
        group['maximum'] = max(group['maximum'], final)
        group['maximum_curve_component_error'] = max(group['maximum_curve_component_error'], abs(final - float(row['anim_curve_value'])))
result = {'run_id': summary['run_id'], 'functional_status': summary['status'],
          'late_idle_definition': 'At least 0.2 seconds continuously in Idle in one case; the configured crossfade is 0.12 seconds.',
          'morph_runtime': [{'state': state, 'morph': name, **data} for (state, name), data in groups.items()],
          'performance': summary['performance_samples'],
          'limitations': ['Curve observations are actual component render weights after animation evaluation. Manual GetMorphTarget values alone are not evidence for animation-driven morphs.',
                          'Walk is the existing native state name; with Run assigned its actual sampled moving clip is AS_Naiwa_Run.',
                          'This editor/game performance comparison uses actual flat-ground movement and excludes screenshot capture. It does not establish packaged performance or other-device results.']}
moving = [item for item in result['morph_runtime'] if item['state'] == 'Walk']
result['three_morphs_animate_in_game'] = len(moving) == 3 and all(item['maximum'] - item['minimum'] > 0.1 for item in moving)
idle = [item for item in result['morph_runtime'] if item['state'] == 'Idle']
result['late_idle_weights_return_zero'] = len(idle) == 3 and all(max(abs(item['minimum']), abs(item['maximum'])) < 0.001 for item in idle)
result['component_weights_match_anim_curves'] = all(item['maximum_curve_component_error'] < 0.001 for item in result['morph_runtime'])
args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({key: value for key, value in result.items() if key not in ('morph_runtime', 'limitations')}, ensure_ascii=False, indent=2))
