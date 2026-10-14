import bpy,sys,json
from pathlib import Path
import numpy as np
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT))
from longitudinal_surface_sculpt import sculpt_longitudinal_surface
from render_surface_diagnostics import render_neutral_views
BASE=OUT/'iterations/before_longitudinal_surface_revision/Naiwa_Character.blend'
DRAFT=OUT/'iterations/longitudinal_surface_draft'
DRAFT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(BASE))
render_neutral_views(DRAFT,'before')
bpy.ops.wm.open_mainfile(filepath=str(BASE))
report,field=sculpt_longitudinal_surface(bpy.data.objects['Naiwa unified golden body'],bpy.data.objects.get('Head and torso silhouette only'))
(DRAFT/'draft_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
np.savez(DRAFT/'fairing_fields.npz',z=field[0],u=field[1],source=field[2],target=field[3])
bpy.ops.wm.save_as_mainfile(filepath=str(DRAFT/'Naiwa_Character.blend'))
render_neutral_views(DRAFT,'after')
print('LONGITUDINAL_SURFACE_DRAFT_OK')
