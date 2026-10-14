from pathlib import Path
import re

out = Path(__file__).resolve().parent
driver = (out / 'revise_connection.py').read_text(encoding='utf-8-sig')
driver = driver.replace('from connection_sculpt import sculpt_connection, refit_mouth, section, LOWER_Z, CROWN_Z',
                        'from connection_sculpt import section\nfrom head_volume_sculpt import sculpt_head_volume, LOWER_Z, new_z')
start = driver.index('baseline = ')
end = driver.index('bpy.ops.wm.open_mainfile', start)
driver = driver[:start] + "baseline = out / 'iterations' / 'before_smaller_head_depth_revision' / 'Naiwa_Character.blend'\n" + driver[end:]
driver = driver.replace("shell = bpy.data.objects['Head and torso silhouette only']",
"""sample_z = [2.10, 2.16, 2.22, 2.28, 2.34, 2.361, 2.40, 2.46, 2.50]
old_sections = [section(body, z) for z in sample_z]
old_height = max(v.co.z for v in body.data.vertices)
shell = bpy.data.objects['Head and torso silhouette only']""")
driver = driver.replace("sculpt_connection(body, shell)\nrefit_mouth(body, bpy.data.objects['Short closed oral seam'])",
                        "sculpt_head_volume(body, shell)")
driver = driver.replace('if a.z <= LOWER_Z or a.z >= CROWN_Z', 'if a.z <= LOWER_Z')
driver = driver.replace('Protected crown/body moved', 'Protected torso/limbs moved')
driver = driver.replace('PROTECTED_CROWN_BODY_OK', 'PROTECTED_BODY_OK')
start = driver.index('report = {')
end = driver.index('scene = bpy.context.scene', start)
driver = driver[:start] + """report = {
    'revision': 'Reduced head width and front-back thickness',
    'protected_body_below_z': LOWER_Z,
    'protected_vertex_count': len(protected),
    'protected_max_position_error': max_error,
    'vertices': len(body.data.vertices),
    'height_before': old_height,
    'height_after': max(v.co.z for v in body.data.vertices),
    'sections': [{'source_z': z, 'new_z': new_z(z),
                  'old_width_front_rear': old,
                  'new_width_front_rear': section(body, new_z(z))}
                 for z, old in zip(sample_z, old_sections)]
}
(out / 'head_volume_validation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')

""" + driver[end:]
driver = driver.replace('CONNECTION_DELIVERABLES_OK', 'HEAD_VOLUME_DELIVERABLES_OK')
(out / 'revise_head_volume.py').write_text(driver, encoding='utf-8')

build = (out / 'build_naiwa.py').read_text(encoding='utf-8-sig')
anchor = '# Armature, with separated limbs so it can be used for a walk or laugh cycle.'
assert build.count(anchor) == 1 and 'sculpt_head_volume' not in build
build = build.replace(anchor, "# Reduce the head and refit the complete eye globes and oral seam.\nfrom head_volume_sculpt import sculpt_head_volume\nsculpt_head_volume(fused, body_only)\n\n" + anchor)
(out / 'build_naiwa.py').write_text(build, encoding='utf-8')
print('HEAD_VOLUME_FILES_PREPARED')

