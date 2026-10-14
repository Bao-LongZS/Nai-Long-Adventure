"""Package the current validated editable character and its matching previews."""
import ast,json,zipfile
from pathlib import Path
OUT=Path(__file__).resolve().parent
report=json.loads((OUT/'lip_eye_validation.json').read_text(encoding='utf-8'))
assert report['independent_validation']=='passed'
for file,marker in [('lip_eye_build.log','LIP_EYE_REVISION_OK'),
    ('lip_eye_independent_validation.log','LIP_EYE_INDEPENDENT_VALIDATION_OK'),
    ('lip_eye_fbx_validation.log','FBX_REIMPORT_OK'),
    ('iterations/lip_eye_rebuild_check/build.log','NAIWA_BUILD_OK')]:
    assert marker in (OUT/file).read_text(encoding='utf-8-sig'),file
scripts=['build_naiwa.py','connection_sculpt.py','head_volume_sculpt.py','neck_extension_sculpt.py',
    'neck_blend_sculpt.py','neck_direction_sculpt.py','longitudinal_surface_sculpt.py',
    'short_round_face_sculpt.py','broad_face_arc_sculpt.py','lip_eye_sculpt.py','eye_projection_measure.py',
    'revise_lip_eye.py','preview_lip_eye.py','render_character_delivery.py','render_surface_diagnostics.py',
    'render_details.py','validate_fbx.py','validate_head_revision.py','validate_lip_eye_refinement.py','finish_lip_eye.py']
for name in scripts: ast.parse((OUT/name).read_text(encoding='utf-8-sig'),filename=name)
files=['Naiwa_Character.blend','Naiwa_Character.fbx','README_使用说明.md','lip_eye_validation.json',
    'lip_eye_build_report.json','lip_eye_camera_settings.json','lip_eye_independent_validation.log',
    'lip_eye_fbx_validation.log','iterations/lip_eye_rebuild_check/build.log']+scripts
images=sorted(p.name for p in OUT.glob('naiwa_*.png'))
assert len(images)==23,len(images)
files+=images+['iterations/before_lip_eye_refinement/Naiwa_Character.blend',
    'iterations/before_lip_eye_refinement/README_使用说明.md',
    'iterations/before_lip_eye_refinement/broad_face_arc_camera_settings.json']
temporary=OUT/'Naiwa_Character_Preview.current.zip'
archive=OUT/'Naiwa_Character_Preview.zip'
with zipfile.ZipFile(temporary,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for name in files: z.write(OUT/name,arcname=name)
with zipfile.ZipFile(temporary) as z: assert z.testzip() is None
temporary.replace(archive)
print('LIP_EYE_PACKAGE_OK',len(files),'files',archive.stat().st_size,'bytes')
