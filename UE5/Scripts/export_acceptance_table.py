"""Export actual case checks with wall-time positions in the review video."""
import csv
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RECORDS = ROOT / 'UE5/Records'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--summary', type=Path, default=RECORDS / 'runtime_run_summary.json')
parser.add_argument('--encoding', type=Path, default=RECORDS / 'Naiwa_Run_Runtime_Final_20261011_encoding_report.json')
parser.add_argument('--output', type=Path, default=RECORDS / 'run_acceptance.csv')
args = parser.parse_args()
summary = json.loads(args.summary.read_text(encoding='utf-8-sig'))
encoding = json.loads(args.encoding.read_text(encoding='utf-8-sig'))
assert summary['status'] == 'PASS' and summary['completed_cases'] == 24
assert encoding['status'] == 'passed'
first = encoding['timing']['first_wall_timestamp_s']
with args.output.open('w', encoding='utf-8-sig', newline='') as stream:
    writer = csv.writer(stream)
    writer.writerow(['run_id', 'round', 'case', 'status', 'video_start_s', 'video_end_s',
                     'max_abs_Y_cm', 'final_vx_cm_s', 'final_state', 'checks_passed', 'checks_total', 'screenshot'])
    for case in summary['cases']:
        frames = [frame for frame in summary['recorded_frames']
                  if frame['round'] == case['round'] and frame['case'] == case['case']]
        times = [frame['wall_time_s'] - first for frame in frames]
        writer.writerow([summary['run_id'], case['round'], case['case'], 'PASS' if case['pass'] else 'FAIL',
                         round(min(times), 3) if times else '', round(max(times), 3) if times else '',
                         case['maximum_abs_y_cm'], case['final_velocity_cm_s']['x'], case['final_anim_state'],
                         sum(check['pass'] for check in case['checks']), len(case['checks']), case['screenshot_path']])
print('Saved ' + str(args.output) + ': 24 actual cases with video positions')
