"""Render a few poses from the saved P2 archer action without editing its source."""
from pathlib import Path
import sys

import bpy

HERE = Path(__file__).resolve().parent
WORKSPACE = HERE.parents[2]
BLEND = HERE / "Naiwa_P2_SlingshotDraw.blend"
PREVIEW = HERE / "preview"

bpy.ops.wm.open_mainfile(filepath=str(BLEND), load_ui=False)
scene = bpy.context.scene
rig = bpy.data.objects["Naiwa_Rig"]
action = bpy.data.actions.get("Naiwa_P2_SlingshotDraw")
assert action is not None
rig.animation_data.action = action
sys.path.insert(0, str(WORKSPACE / "NaiwaCharacter"))
from render_head_reshape_comparison import prepare_scene
from render_head_neck_comparisons import camera

prepare_scene()
rig.animation_data.action = action
scene.render.resolution_x = scene.render.resolution_y = 512
scene.render.resolution_percentage = 100
if hasattr(scene, "eevee") and hasattr(scene.eevee, "taa_render_samples"):
    scene.eevee.taa_render_samples = 16
PREVIEW.mkdir(parents=True, exist_ok=True)
for view, yaw in (("side", 90), ("three_quarter", 45)):
    camera(scene, {"type": "ORTHO", "yaw": yaw, "pitch": 0,
                   "target": [0, -0.13, 1.22], "distance": 7,
                   "scale": 3.45, "lens": 50, "resolution": [512, 512]})
    for frame, label in ((1, "rest"), (16, "half_draw"), (31, "full_draw")):
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        print("POSE", view, label,
              tuple(round(v, 3) for v in rig.pose.bones["UpperArm.L"].rotation_euler),
              tuple(round(v, 3) for v in rig.pose.bones["UpperArm.R"].rotation_euler), flush=True)
        scene.render.filepath = str(PREVIEW / f"{view}_{label}.png")
        bpy.ops.render.render(write_still=True)
print("NAIWA_P2_ARCHER_REVIEW_RENDERED", flush=True)
