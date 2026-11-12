"""Create or validate the isolated P6 M1 graybox level and bind its native GameMode."""
from __future__ import annotations

import json
import os
from pathlib import Path
import traceback

import unreal


ROOT = Path(__file__).resolve().parents[2]
MAP_ASSET = "/Game/Naiwa/P6/Maps/L_NaiwaP6_M1Prototype"
MAP_FILE = ROOT / "UE5/NaiwaP0/Content/Naiwa/P6/Maps/L_NaiwaP6_M1Prototype.umap"
GAME_MODE_PATH = "/Script/NaiwaRuntime.NaiwaP6InvestigationGameMode"
OWNER_TAG = "NaiwaP6M1Prototype"
REPORT = Path(os.environ.get(
    "NAIWA_P6_MAP_REPORT", str(ROOT / "UE5/Records/p6_m1/m1_map_build_report.json")
))

report = {"status": "failed", "map": MAP_ASSET, "game_mode": GAME_MODE_PATH}
try:
    game_mode = unreal.load_class(None, GAME_MODE_PATH)
    if not game_mode:
        raise RuntimeError("P6 M1 native GameMode could not be loaded: " + GAME_MODE_PATH)

    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P6/Maps")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_ASSET):
        if not levels.load_level(MAP_ASSET):
            raise RuntimeError("Could not load existing P6 level: " + MAP_ASSET)
        world = editor.get_editor_world()
        settings = world.get_world_settings()
        tags = [str(tag) for tag in settings.get_editor_property("tags")]
        current = settings.get_editor_property("default_game_mode")
        if OWNER_TAG not in tags or not current or current.get_path_name() != GAME_MODE_PATH:
            raise RuntimeError("Existing level is not the owned P6 M1 prototype; refusing to overwrite it")
        status = "existing_owned_map_validated"
    else:
        if not levels.new_level(MAP_ASSET):
            raise RuntimeError("Could not create new P6 M1 level")
        world = editor.get_editor_world()
        settings = world.get_world_settings()
        settings.set_editor_property("tags", [unreal.Name(OWNER_TAG)])
        settings.set_editor_property("default_game_mode", game_mode)

        start = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.PlayerStart, unreal.Vector(-2450.0, -1250.0, 27.0), unreal.Rotator(0.0, 0.0, 0.0)
        )
        if not start:
            raise RuntimeError("Could not create P6 M1 PlayerStart")
        start.set_actor_label("NAIWA_P6_M1_PlayerStart")

        light = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.DirectionalLight, unreal.Vector(-1500.0, -2200.0, 3200.0),
            unreal.Rotator(-48.0, -32.0, 0.0)
        )
        if not light:
            raise RuntimeError("Could not create P6 graybox directional light")
        light.set_actor_label("NAIWA_P6_M1_GrayboxLight")

        if not levels.save_current_level():
            raise RuntimeError("Could not save P6 M1 level")
        status = "new_isolated_map_saved"

    if not MAP_FILE.is_file() or MAP_FILE.stat().st_size <= 0:
        raise RuntimeError("Saved P6 map package is missing: " + str(MAP_FILE))
    current = editor.get_editor_world().get_world_settings().get_editor_property(
        "default_game_mode"
    )
    if not current or current.get_path_name() != GAME_MODE_PATH:
        raise RuntimeError("P6 M1 GameMode did not round-trip through the map package")

    report = {
        "status": status,
        "runtime_fixture_stage": "M2",
        "map": MAP_ASSET,
        "map_file": str(MAP_FILE),
        "map_package_bytes": MAP_FILE.stat().st_size,
        "engine": unreal.SystemLibrary.get_engine_version(),
        "owner_tag": OWNER_TAG,
        "default_game_mode": GAME_MODE_PATH,
        "runtime_pawn": "/Script/NaiwaRuntime.NaiwaP6PlayerPawn",
        "m1_baseline_points": 3,
        "interaction_points": 14,
        "required_interaction_points": 9,
        "optional_joke_points": 5,
        "clue_cards": ["G1", "G2", "G3", "G4", "G5", "G6"],
    }
except Exception:
    report["error"] = traceback.format_exc()

REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
if report["status"] not in ("new_isolated_map_saved", "existing_owned_map_validated"):
    unreal.log_error(report.get("error", "P6 M1 map creation failed"))
    raise RuntimeError("P6 M1 map creation failed; see " + str(REPORT))
unreal.log("NAIWA_P6_M1_MAP_OK " + json.dumps(report, ensure_ascii=False))
