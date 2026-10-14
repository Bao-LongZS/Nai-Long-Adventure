from pathlib import Path

out=Path(__file__).resolve().parent
driver=(out/'revise_head_volume.py').read_text(encoding='utf-8-sig')
driver=driver.replace('from head_volume_sculpt import sculpt_head_volume, LOWER_Z, new_z',
                      'from neck_extension_sculpt import extend_neck, NECK_BASE_Z, HEAD_BASE_Z, HEAD_RAISE, NECK_TOP_Z')
driver=driver.replace('before_smaller_head_depth_revision','before_raised_head_neck_revision')
driver=driver.replace('sculpt_head_volume(body, shell)','extend_neck(body, shell)')
driver=driver.replace('if a.z <= LOWER_Z','if a.z <= NECK_BASE_Z')
start=driver.index('report = {')
end=driver.index('scene = bpy.context.scene',start)
driver=driver[:start]+"""rigid = [(a, b.co) for a,b in zip(before,body.data.vertices) if a.z >= HEAD_BASE_Z]
rigid_error = max((b-a-Vector((0,0,HEAD_RAISE))).length for a,b in rigid)
assert rigid_error < 1e-6, f'Head shape changed by {rigid_error}'
report = {
    'revision': 'Rigid head raise with extended neck',
    'preserved_body_below_z': NECK_BASE_Z,
    'original_head_base_z': HEAD_BASE_Z,
    'head_raise': HEAD_RAISE,
    'neck_height_range': [NECK_BASE_Z, NECK_TOP_Z],
    'protected_vertex_count': len(protected),
    'protected_max_position_error': max_error,
    'head_rigid_translation_max_error': rigid_error,
    'height_before': old_height,
    'height_after': max(v.co.z for v in body.data.vertices),
    'sections': [{'z': NECK_BASE_Z+(NECK_TOP_Z-NECK_BASE_Z)*i/16,
                  'width_front_rear': section(body, NECK_BASE_Z+(NECK_TOP_Z-NECK_BASE_Z)*i/16)}
                 for i in range(17)]
}
(out/'neck_extension_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')

"""+driver[end:]
driver=driver.replace("render('naiwa_front.png', (0, -7, 1.31))","render('naiwa_front.png', (0, -7, 1.31))")
driver=driver.replace("    render(name, location, target, scale)\nprint('HEAD_VOLUME_DELIVERABLES_OK'",
"""    if name.startswith('naiwa_face'):
        location = tuple(value+(HEAD_RAISE if i==2 else 0) for i,value in enumerate(location))
        target = tuple(value+(HEAD_RAISE if i==2 else 0) for i,value in enumerate(target))
    render(name, location, target, scale)
render('naiwa_neck_front_close.png', (0,-7,2.25), (0,0,2.25), 0.82)
render('naiwa_neck_side_close.png', (6,-0.13,2.25), (0,-0.13,2.25), 0.82)
print('NECK_EXTENSION_DELIVERABLES_OK'""")
(out/'revise_neck_extension.py').write_text(driver,encoding='utf-8')
build=(out/'build_naiwa.py').read_text(encoding='utf-8-sig')
anchor='# Armature, with separated limbs so it can be used for a walk or laugh cycle.'
assert build.count(anchor)==1 and 'extend_neck' not in build
build=build.replace(anchor,'# Raise the preserved head and construct the longer neck above fixed shoulders.\nfrom neck_extension_sculpt import extend_neck\nextend_neck(fused, body_only)\n\n'+anchor)
(out/'build_naiwa.py').write_text(build,encoding='utf-8')
print('NECK_EXTENSION_SCRIPTS_PREPARED')

