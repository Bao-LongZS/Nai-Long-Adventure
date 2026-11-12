"""Capture the isolated P6 A1 lighting map from its authored UE camera."""
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
REPORT = EVIDENCE / "P6_A1_LightingReviewCapture_v01.json"
CAPTURES = [
    (1280, 720, EVIDENCE / "P6_A1_LightingReview_1280x720_v01.png"),
    (1920, 1080, EVIDENCE / "P6_A1_LightingReview_1920x1080_v01.png"),
]


report = {
    "status": "running",
    "engine": unreal.SystemLibrary.get_engine_version(),
    "map_asset": MAP_PATH,
    "camera_label": CAMERA_LABEL,
}

try:
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_editor.load_level(MAP_PATH):
        raise RuntimeError("Could not load isolated A1 lighting review map")
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    camera_actor = next((actor for actor in actors if actor.get_actor_label() == CAMERA_LABEL), None)
    if not camera_actor or not isinstance(camera_actor, unreal.CameraActor):
        raise RuntimeError("Review camera actor is missing: " + CAMERA_LABEL)

    captured = []
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
        deadline = time.monotonic() + 45.0
        while not task.is_task_done() and time.monotonic() < deadline:
            time.sleep(0.25)
        if not task.is_task_done() or not image_path.is_file() or image_path.stat().st_size < 1024:
            raise RuntimeError("UE screenshot task did not finish with a usable image: " + str(image_path))
        captured.append({
            "path": str(image_path),
            "resolution_px": [width, height],
            "bytes": image_path.stat().st_size,
            "camera": camera_actor.get_path_name(),
        })

    report.update({"status": "captured", "captures": captured, "map_saved_before_capture": True})
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()

EVIDENCE.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
if report["status"] != "captured":
    unreal.log_error(report.get("error", "P6 A1 screenshot capture failed"))
    raise RuntimeError("P6 A1 screenshot capture failed; see " + str(REPORT))
unreal.log("NAIWA_P6_A1_LIGHTING_CAPTURED " + json.dumps(report, ensure_ascii=False))
