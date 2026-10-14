"""Package the independently verified current foreface and matching previews."""
import ast,json,zipfile
from pathlib import Path
OUT=Path(__file__).resolve().parent
report=json.loads((OUT/'continuous_front_face_validation.json').read_text(encoding='utf-8'))
assert report['independent_validation']=='passed'
for file,marker in [('continuous_front_face_build.log','CONTINUOUS_FRONT_FACE_REVISION_OK'),
    ('continuous_front_face_independent_validation.log','CONTINUOUS_FRONT_FACE_INDEPENDENT_VALIDATION_OK'),
    ('continuous_front_face_fbx_validation.log','FBX_REIMPORT_OK'),
    ('iterations/continuous_front_face_rebuild_check/build.log','NAIWA_BUILD_OK')]:
    assert marker in (OUT/file).read_text(encoding='utf-8-sig'),file
scripts=['build_naiwa.py','connection_sculpt.py','head_volume_sculpt.py','neck_extension_sculpt.py',
    'neck_blend_sculpt.py','neck_direction_sculpt.py','longitudinal_surface_sculpt.py',
    'short_round_face_sculpt.py','broad_face_arc_sculpt.py','lip_eye_sculpt.py','eye_projection_measure.py',
    'continuous_front_face_sculpt.py','revise_continuous_front_face.py','preview_continuous_front_face.py',
    'render_character_delivery.py','render_surface_diagnostics.py','render_details.py','validate_fbx.py',
    'validate_head_revision.py','validate_lip_eye_refinement.py','validate_continuous_front_face.py','finish_continuous_front_face.py']
for name in scripts: ast.parse((OUT/name).read_text(encoding='utf-8-sig'),filename=name)
files=['Naiwa_Character.blend','Naiwa_Character.fbx','README_使用说明.md','continuous_front_face_validation.json',
    'continuous_front_face_build_report.json','continuous_front_face_camera_settings.json',
    'continuous_front_face_independent_validation.log','continuous_front_face_fbx_validation.log',
    'iterations/continuous_front_face_rebuild_check/build.log']+scripts
images=sorted(p.name for p in OUT.glob('naiwa_*.png')); assert len(images)==23,len(images)
files+=images+['iterations/before_continuous_front_face/Naiwa_Character.blend',
    'iterations/before_continuous_front_face/README_使用说明.md',
    'iterations/before_continuous_front_face/lip_eye_camera_settings.json']
temporary=OUT/'Naiwa_Character_Preview.current.zip'; archive=OUT/'Naiwa_Character_Preview.zip'
with zipfile.ZipFile(temporary,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for name in files: z.write(OUT/name,arcname=name)
with zipfile.ZipFile(temporary) as z: assert z.testzip() is None
temporary.replace(archive)
print('CONTINUOUS_FRONT_FACE_PACKAGE_OK',len(files),'files',archive.stat().st_size,'bytes')
