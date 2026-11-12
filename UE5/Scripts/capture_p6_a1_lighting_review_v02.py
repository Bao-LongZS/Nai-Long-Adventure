"""Capture the isolated P6 A1 lighting review through UE's latent tick scheduler."""
from __future__ import annotations

import json
from pathlib import Path
import time
import traceback

import unreal


ROOT = Path(__file__).resolve().parents[2]
MAP_PATH = "/Game/Naiwa/P6/Art/A1/Maps/L_NaiwaP6_A1LightingReview_v01"
CAMERA_LABEL = "NAIWA_P6_A1_Review_OrthoCamera"
EVIDENCE = ROOT / "UE5/Records/p6_art/evidence"
REPORT = EVIDENCE / "P6_A1_LightingReviewCapture_v02.json"
CAPTURES = [
    (1280, 720, EVIDENCE / "P6_A1_LightingReview_1280x720_v02.png"),
    (1920, 1080, EVIDENCE / "P6_A1_LightingReview_1920x1080_v02.png"),
]

report = {
    "status": "running",
    "engine": unreal.SystemLibrary.get_engine_version(),
    "map_asset": MAP_PATH,
    "camera_label": CAMERA_LABEL,
    "tick_scheduler": "unreal.AutomationScheduler",
}
EVIDENCE.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")


def finish(status: str, **details) -> None:
    report.update({"status": status, **details})
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log("NAIWA_P6_A1_LIGHTING_CAPTURE_" + status.upper() + " " + json.dumps(report, ensure_ascii=False))
    unreal.SystemLibrary.quit_editor()


try:
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_editor.load_level(MAP_PATH):
        raise RuntimeError("Could not load isolated A1 lighting review map")
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    camera_actor = next((actor for actor in actors if actor.get_actor_label() == CAMERA_LABEL), None)
    if not camera_actor or not isinstance(camera_actor, unreal.CameraActor):
        raise RuntimeError("Review camera actor is missing: " + CAMERA_LABEL)
    if not hasattr(unreal, "AutomationScheduler"):
        raise RuntimeError("PythonAutomationTest plugin did not expose AutomationScheduler")

    @unreal.AutomationScheduler.add_latent_command
    def capture_with_ticks():
        captured = []
        try:
            for width, height, image_path in CAPTURES:
                if image_path.exists():
                    raise RuntimeError("Refusing to overwrite existing capture: " + str(image_path))
                task = unreal.AutomationLibrary.take_high_res_screenshot(
                    width,
                    height,
                    str(image_path),
                    camera=camera_actor,
                    capture_hdr=False,
                    comparison_notes="P6 A1 v04 isolated lighting review; not final M2 placement acceptance",
                    delay=1.0,
                    force_game_view=True,
                )
                if not task or not task.is_valid_task():
                    raise RuntimeError("UE did not queue screenshot task for " + str(width) + "x" + str(height))
                started = time.monotonic()
                while not task.is_task_done():
                    if time.monotonic() - started > 75.0:
                        raise RuntimeError("Screenshot task timed out at " + str(width) + "x" + str(height))
                    yield
                # The task can report completion one or two ticks before the PNG is flushed.
                for _ in range(90):
                    if image_path.is_file() and image_path.stat().st_size >= 1024:
                        break
                    yield
                if not image_path.is_file() or image_path.stat().st_size < 1024:
                    raise RuntimeError("Screenshot task completed without a usable image: " + str(image_path))
                captured.append({
                    "path": str(image_path),
                    "resolution_px": [width, height],
                    "bytes": image_path.stat().st_size,
                    "camera": camera_actor.get_path_name(),
                })
            finish("captured", captures=captured, map_saved_before_capture=True)
        except Exception:
            finish("failed", error=traceback.format_exc(), partial_captures=captured)
except Exception:
    finish("failed", error=traceback.format_exc())
