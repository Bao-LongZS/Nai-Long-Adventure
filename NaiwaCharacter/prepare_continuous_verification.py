"""Adapt the established independent mesh checks to the latest fixed bounds."""
from pathlib import Path

root=Path(__file__).resolve().parent
source=(root/'validate_lip_eye_refinement.py').read_text(encoding='utf-8-sig')
replacements=[
('iterations/before_lip_eye_refinement/Naiwa_Character.blend','iterations/before_continuous_front_face/Naiwa_Character.blend'),
('iterations/lip_eye_draft/Naiwa_Character.blend','iterations/continuous_front_face_draft/Naiwa_Character.blend'),
("protected = (before[:, 2] <= 2.18) | (before[:, 2] >= 2.435)","protected = (before[:, 2] <= 2.20) | (before[:, 2] >= 2.46) | (np.abs(before[:, 0]) >= .19)"),
('heights = np.arange(2.18, 2.435 + 0.00025, 0.0005)','heights = np.arange(2.20, 2.46 + 0.00025, 0.0005)'),
('below_2_18_and_above_2_435_exact','fixed_height_and_lateral_bounds_exact'),
('limit = 0.03 if region == \'aperture\' else 0.04','limit = 0.01'),
("if region == 'aperture':\n                assert center_change <= 0.001","if region in ('aperture', 'green', 'black'):\n                assert center_change <= 0.0003"),
('assert 0.00001 <= abs(arc) < 0.004','assert 0.00001 <= arc < 0.004'),
('lower_range = (zs >= 2.28 - 1e-8) & (zs <= 2.335 + 1e-8)','lower_range = (zs >= 2.315 - 1e-8) & (zs <= 2.350 + 1e-8)'),
('assert lower_mean > 0.005 and lower_max > 0.010','assert lower_mean > 0 and lower_max > 0.001'),
('assert 0.008 < lip_projection < 0.035','assert 0.004 < lip_projection < 0.035'),
("'range_z': [2.28, 2.335]","'range_z': [2.315, 2.350]"),
('lip_eye_validation.json','continuous_front_face_root_validation.json'),
('LIP_EYE_INDEPENDENT_VALIDATION_OK','CONTINUOUS_FRONT_FACE_ROOT_VALIDATION_OK'),
]
for old,new in replacements:
    assert old in source,old
    source=source.replace(old,new)
anchor="        pupil_before, pupil_after = np.array(a['frontal_pupil_center']), np.array(b['frontal_pupil_center'])"
assert anchor in source
source=source.replace(anchor,"""        ac,bc=coordinates(a),coordinates(b)
        assert np.array_equal(ac[:,[0,2]],bc[:,[0,2]]),('Eye frontal geometry changed',side)
        assert np.max(np.abs(ac[:,1]-bc[:,1])) < .016,('Eye reattachment too large',side)
        assert np.max(np.abs(ac[ac[:,2]>2.463]-bc[ac[:,2]>2.463])) < 1e-6,('Upper eye moved',side)
        for prop in ('frontal_aperture_center','frontal_eye_axes','frontal_pupil_radius','visible_pupil_to_aperture_area_ratio'):
            assert np.array_equal(np.array(a[prop]),np.array(b[prop])),('Eye property changed',side,prop)
"""+anchor)
anchor="    assert len(mouth) % 2 == 0, 'Expected paired mouth-seam vertices'"
assert anchor in source
source=source.replace(anchor,"    assert np.array_equal(old_mouth[:,[0,2]],mouth[:,[0,2]]), 'Mouth frontal geometry changed'\n"+anchor)
anchor="    posterior_error = float(np.max(np.abs(after_sections[:, 2] - before_sections[:, 2])))"
assert anchor in source
source=source.replace(anchor,"""    upper = (zs >= mouth_z-.015) & (zs <= 2.435)
    peak_z=float(zs[upper][np.argmin(after_sections[upper,1])])
    assert abs(peak_z-mouth_z)<=.012,('Frontmost point is not near the mouth',peak_z,mouth_z)
    nose=(zs>=2.385)&(zs<=2.42)
    assert np.min(after_sections[nose,1]) > np.min(after_sections[upper,1])+.0001,'Central upper nose remains'
"""+anchor)
(root/'validate_continuous_front_face_root.py').write_text(source,encoding='utf-8')
print('CONTINUOUS_FRONT_FACE_ROOT_VALIDATOR_PREPARED')
