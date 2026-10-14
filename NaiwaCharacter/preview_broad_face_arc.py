import bpy, sys, json
from pathlib import Path
OUT = Path(__file__).resolve().parent
sys.path.insert(0, str(OUT))
from broad_face_arc_sculpt import sculpt_broad_face_arc
from render_surface_diagnostics import render_neutral_views
from connection_sculpt import section
DRAFT = OUT/'iterations/broad_face_arc_draft'
DRAFT.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(OUT/'iterations/before_broad_face_arc_refinement/Naiwa_Character.blend'))
report = sculpt_broad_face_arc(bpy.data.objects['Naiwa unified golden body'], bpy.data.objects.get('Head and torso silhouette only'))
bpy.ops.wm.save_as_mainfile(filepath=str(DRAFT/'Naiwa_Character.blend'))
report['sections'] = [[float(z), *section(bpy.data.objects['Naiwa unified golden body'], float(z))] for z in __import__('numpy').arange(1.98, 2.54, .02)]
(DRAFT/'draft_report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
render_neutral_views(DRAFT, 'after')
print('BROAD_FACE_ARC_DRAFT_OK')
