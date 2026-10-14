"""Package the verified current character, renders and reproducible source."""
import ast,json,zipfile
from pathlib import Path
OUT=Path(__file__).resolve().parent
report=json.loads((OUT/'short_round_face_validation.json').read_text(encoding='utf-8'))
assert report['independent_validation']=='passed'
for file,marker in [('short_round_face_build.log','SHORT_ROUND_FACE_REVISION_OK'),
                    ('short_round_face_independent_validation.log','SHORT_ROUND_FACE_INDEPENDENT_VALIDATION_OK'),
                    ('short_round_face_fbx_validation.log','FBX_REIMPORT_OK'),
                    ('iterations/short_round_face_rebuild_check/build.log','NAIWA_BUILD_OK')]:
    assert marker in (OUT/file).read_text(encoding='utf-8-sig'),file
scripts=['build_naiwa.py','connection_sculpt.py','head_volume_sculpt.py','neck_extension_sculpt.py',
         'neck_blend_sculpt.py','neck_direction_sculpt.py','longitudinal_surface_sculpt.py','short_round_face_sculpt.py',
         'revise_short_round_face.py','render_character_delivery.py','render_surface_diagnostics.py',
         'render_details.py','validate_fbx.py','validate_head_revision.py','validate_short_round_face.py',
         'finish_short_round_face.py']
for name in scripts: ast.parse((OUT/name).read_text(encoding='utf-8-sig'),filename=name)
files=['Naiwa_Character.blend','Naiwa_Character.fbx','README_使用说明.md','short_round_face_validation.json',
       'short_round_face_build_report.json','short_round_face_camera_settings.json',
       'short_round_face_independent_validation.log','short_round_face_fbx_validation.log']+scripts
files+=sorted(p.name for p in OUT.glob('naiwa_*.png'))
assert len([p for p in files if p.endswith('.png')])==20
files+=['iterations/before_short_round_face_revision/Naiwa_Character.blend',
        'iterations/before_short_round_face_revision/README_使用说明.md']
archive=OUT/'Naiwa_Character_Preview.zip'
temporary=OUT/'Naiwa_Character_Preview.current.zip'
with zipfile.ZipFile(temporary,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for name in files: z.write(OUT/name,arcname=name)
with zipfile.ZipFile(temporary) as z: assert z.testzip() is None
temporary.replace(archive)
print('SHORT_ROUND_FACE_PACKAGE_OK',len(files),'files',archive.stat().st_size,'bytes')
