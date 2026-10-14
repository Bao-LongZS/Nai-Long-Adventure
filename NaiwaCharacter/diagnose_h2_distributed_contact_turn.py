"""Constructor-only diagnosis of the optional H2 central turn target.

This reads the frozen R1 grid already saved with N. It does not solve, deform,
render or validate a new mesh. Saved-mesh section and reflection review remain
required after building the new recipe.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from h2_balanced_turn import balanced_front_curves, balanced_rear_curves


def ease(a, b, values):
    t = np.clip((values - a) / (b - a), 0., 1.)
    return t ** 3 * (10. + t * (-15. + 6. * t))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--input', type=Path, required=True)
    ap.add_argument('--baseline', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    frozen = np.load(args.input)
    baseline = np.load(args.baseline)
    grid, zs, centers = (frozen[k] for k in
                         ('grid_source', 'grid_Z', 'grid_center_Y'))
    sy = ((grid[:, :, 1] - centers[:, None]) /
          np.maximum(np.hypot(grid[:, :, 0], grid[:, :, 1] - centers[:, None]), 1e-12))
    fw = (1. - ease(-.60, -.08, sy)).mean(axis=0)
    rw = ease(.08, .60, sy).mean(axis=0)
    old, old_report = balanced_front_curves(grid, zs, fw, contact_aware=True)
    new, new_report = balanced_front_curves(
        grid, zs, fw, contact_aware=True, distributed_contact=True)
    rear, _ = balanced_rear_curves(grid, zs, rw, rear_upper_z=2.61)
    zero, _ = balanced_front_curves(
        grid, zs, fw, strength=0., contact_aware=True, distributed_contact=True)
    regression = {
        'default_N_front_bitwise_equal_to_pre_edit_baseline': bool(
            np.array_equal(old, baseline['front'])),
        'rear_2_61_bitwise_equal_to_pre_edit_baseline': bool(
            np.array_equal(rear, baseline['rear'])),
        'zero_strength_exact_source_identity': bool(np.array_equal(zero, grid[:, :, 1])),
    }
    assert all(regression.values()), regression
    x_at_mouth = np.asarray([np.interp(2.40, zs, grid[:, j, 0])
                             for j in range(grid.shape[1])])
    untouched = (np.abs(x_at_mouth) >= .130) | (fw < 1e-5)
    regression['outer_columns_beyond_contact_recipe_exactly_unchanged'] = bool(
        np.array_equal(new[:, untouched], old[:, untouched]))
    assert regression['outer_columns_beyond_contact_recipe_exactly_unchanged']
    selected = []
    for sample_x in (0., .04, .08, .12):
        candidates = np.flatnonzero(fw > .95)
        j = int(candidates[np.argmin(np.abs(x_at_mouth[candidates] - sample_x))])
        old_col = next((c for c in old_report['columns'] if c['column'] == j), None)
        new_col = next((c for c in new_report['columns'] if c['column'] == j), None)
        levels = np.array([2.18, 2.26, 2.30, 2.31, 2.32, 2.33, 2.34,
                           2.35, 2.36, 2.37, 2.38, 2.39, 2.40])
        rows = []
        for q in levels:
            row = {'Z': float(q)}
            for label, yy in [('source', grid[:, j, 1]), ('N_target', old[:, j]),
                              ('distributed_target', new[:, j])]:
                row[label + '_Y'] = float(np.interp(q, zs, yy))
                row[label + '_beta_degrees'] = float(np.degrees(np.arctan(
                    -np.interp(q, zs, np.gradient(yy, zs)))))
            rows.append(row)
        selected.append({'column': j, 'mouth_level_X': float(x_at_mouth[j]),
                         'N_planned_angle_report': old_col,
                         'distributed_planned_angle_report': new_col,
                         'sampled_target_rows': rows})
    report = {
        'scope': 'Frozen source target constructor only; no saved new mesh and no visual acceptance',
        'input': str(args.input.resolve()),
        'input_sha256': hashlib.sha256(args.input.read_bytes()).hexdigest(),
        'baseline_sha256': hashlib.sha256(args.baseline.read_bytes()).hexdigest(),
        'flag': '--distributed-contact-turn',
        'requires': '--contact-aware-turn --balanced-turn --integrated-turn',
        'reason': 'N central Z2.31..2.34 near-straight run followed by concentrated mouth turn and new reflection hook',
        'rear_recipe': 'unchanged rear curve upper Z2.61',
        'validation_thresholds_changed': False,
        'regression': regression,
        'maximum_target_Y_change_from_N': float(np.max(np.abs(new - old))),
        'maximum_target_Y_delta_from_source': float(np.max(np.abs(new - grid[:, :, 1]))),
        'target_feasibility_note': 'Final mesh retains original 0.035 total displacement constraint; target excursions are soft objectives',
        'selected_front_columns': selected,
        'next_required': 'Build one saved candidate, read actual central mesh sections and same-protocol 45/90 reflection before complete delivery checks',
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('DISTRIBUTED_CONTACT_TARGET_DIAGNOSIS_OK', json.dumps(regression))


if __name__ == '__main__':
    main()
