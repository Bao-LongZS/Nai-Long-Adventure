"""Reproduce this revision from the captured open scene without cumulative changes."""
import bpy,sys,json
from pathlib import Path
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT))
from longitudinal_surface_sculpt import sculpt_longitudinal_surface
from render_character_delivery import deliver_character
from render_surface_diagnostics import render_neutral_views
bpy.ops.wm.open_mainfile(filepath=str(OUT/'iterations/before_longitudinal_surface_revision/Naiwa_Character.blend'))
report,_=sculpt_longitudinal_surface(bpy.data.objects['Naiwa unified golden body'],bpy.data.objects.get('Head and torso silhouette only'))
(OUT/'longitudinal_surface_build_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
deliver_character(OUT)
render_neutral_views(OUT,'naiwa_surface_gray')
print('LONGITUDINAL_SURFACE_REVISION_OK')
