"""Summarize a captured ordinary-input session; do not operate the game."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('session', type=Path)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
rows = []
lines = args.session.read_text(encoding='utf-8-sig').splitlines()
for index, line in enumerate(lines):
    try:
        rows.append(json.loads(line))
    except json.JSONDecodeError:
        if index != len(lines) - 1:
            raise
assert rows and all(not r.get('auto_test_mode') for r in rows)
events = [r for r in rows if r['event'] == 'gait_toggled']
modes = {}
for running, label, target, clip in ((False, 'walk', 150.0, '/AS_Naiwa_Walk.'),
                                    (True, 'run', 300.0, '/AS_Naiwa_Run.')):
    selected = [r for r in rows if r['event'] == 'sample' and r.get('running_gait') == running
                and r.get('animation_state') == 'Walk' and not r.get('is_falling')
                and abs(abs(r.get('velocity_x_cm_s', 0)) - target) <= 1.0
                and abs(r.get('max_walk_speed_cm_s', 0) - target) <= 1e-4
                and clip in r.get('moving_sequence', '')]
    modes[label] = {'stable_moving_samples': len(selected), 'speed_cm_s': target,
                    'actual_proxy_clip': selected[0]['moving_sequence'] if selected else None,
                    'first_sample_utc': selected[0]['timestamp_utc'] if selected else None,
                    'example': selected[0] if selected else None}
assert events and all(m['stable_moving_samples'] > 0 for m in modes.values()), modes
report = {'status': 'passed_ordinary_Windows_walk_run_toggle_and_actual_movement',
          'source_session': str(args.session.resolve()), 'auto_test_mode': False,
          'gait_toggle_events': len(events), 'modes': modes,
          'toggle_timeline': [{k: r[k] for k in ('timestamp_utc', 'gait_events', 'running_gait', 'max_walk_speed_cm_s')}
                              for r in events],
          'last_sample': rows[-1],
          'method': 'Read real ordinary-window JSONL, including user hardware interaction. Tool short modifier taps initially did not invoke the gait callback; subsequent user interaction supplied the stable moving samples.',
          'limits': 'Sampled runtime evidence, not every rendered frame. Speed thresholds exclude acceleration, braking, and airborne transitions.'}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'status': report['status'], 'toggles': len(events),
                  'walk_samples': modes['walk']['stable_moving_samples'],
                  'run_samples': modes['run']['stable_moving_samples']}))
