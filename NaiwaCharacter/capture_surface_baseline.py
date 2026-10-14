"""Capture the actual open Blender scene before the broad surface revision."""
import bpy, json
from pathlib import Path
OUT = Path(__file__).resolve().parent
backup = OUT / 'iterations/before_longitudinal_surface_revision'
backup.mkdir(parents=True, exist_ok=True)
report = {
    'open_file': bpy.data.filepath,
    'dirty': bpy.data.is_dirty,
    'objects': [(o.name, o.type) for o in bpy.data.objects],
    'body_modifiers': [(m.name, m.type) for m in bpy.data.objects['Naiwa unified golden body'].modifiers],
    'frame': bpy.context.scene.frame_current,
}
(backup / 'open_scene_capture.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
if bpy.context.object and bpy.context.object.mode != 'OBJECT':
    bpy.ops.object.mode_set(mode='OBJECT')
bpy.ops.wm.save_as_mainfile(filepath=str(backup/'Naiwa_Character.blend'), copy=True)
print('SURFACE_BASELINE_CAPTURED', str(backup))
