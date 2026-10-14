"""Package the validated current scene, exports, images and reproducible source."""
import ast, json, zipfile
from pathlib import Path
OUT = Path(__file__).resolve().parent
report = json.loads((OUT/'broad_face_arc_validation.json').read_text(encoding='utf-8'))
assert report['independent_validation'] == 'passed'
for name, marker in [
    ('broad_face_arc_build.log', 'BROAD_FACE_ARC_REVISION_OK'),
    ('broad_face_arc_independent_validation.log', 'BROAD_FACE_ARC_INDEPENDENT_VALIDATION_OK'),
    ('broad_face_arc_fbx_validation.log', 'FBX_REIMPORT_OK'),
    ('iterations/broad_face_arc_rebuild_check/build.log', 'NAIWA_BUILD_OK')]:
    assert marker in (OUT/name).read_text(encoding='utf-8-sig'), name
scripts = ['build_naiwa.py', 'connection_sculpt.py', 'head_volume_sculpt.py',
    'neck_extension_sculpt.py', 'neck_blend_sculpt.py', 'neck_direction_sculpt.py',
    'longitudinal_surface_sculpt.py', 'short_round_face_sculpt.py', 'broad_face_arc_sculpt.py',
    'revise_broad_face_arc.py', 'preview_broad_face_arc.py', 'render_character_delivery.py',
    'render_surface_diagnostics.py', 'render_details.py', 'validate_fbx.py',
    'validate_head_revision.py', 'validate_broad_face_arc.py', 'finish_broad_face_arc.py']
for name in scripts:
    ast.parse((OUT/name).read_text(encoding='utf-8-sig'), filename=name)
files = ['Naiwa_Character.blend', 'Naiwa_Character.fbx', 'README_使用说明.md',
    'broad_face_arc_validation.json', 'broad_face_arc_build_report.json',
    'broad_face_arc_camera_settings.json', 'broad_face_arc_independent_validation.log',
    'broad_face_arc_fbx_validation.log', 'iterations/broad_face_arc_rebuild_check/build.log'] + scripts
images = sorted(p.name for p in OUT.glob('naiwa_*.png'))
assert len(images) == 23, len(images)
files += images + ['iterations/before_broad_face_arc_refinement/Naiwa_Character.blend',
                  'iterations/before_broad_face_arc_refinement/README_使用说明.md']
archive = OUT/'Naiwa_Character_Preview.zip'
temporary = OUT/'Naiwa_Character_Preview.current.zip'
with zipfile.ZipFile(temporary, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for name in files:
        z.write(OUT/name, arcname=name)
with zipfile.ZipFile(temporary) as z:
    assert z.testzip() is None
temporary.replace(archive)
print('BROAD_FACE_ARC_PACKAGE_OK', len(files), 'files', archive.stat().st_size, 'bytes')
