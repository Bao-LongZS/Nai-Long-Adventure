"""Create or validate the isolated native M1 P3 prototype map."""
import json
import pathlib
import traceback
import unreal


ROOT = pathlib.Path(__file__).resolve().parents[2]
MAP = "/Game/Naiwa/P3/Maps/L_NaiwaP3_M1Prototype"
OWNER_TAG = "NaiwaP3M1Prototype"
GAME_MODE_PATH = "/Script/NaiwaRuntime.NaiwaP3M1GameMode"
REPORT = ROOT / ".workbuddy/work/20261026_P3方案/M1/map_build_report.json"
report = {"map": MAP, "engine": unreal.SystemLibrary.get_engine_version(), "status": "running"}

try:
    game_mode = unreal.load_class(None, GAME_MODE_PATH)
    assert game_mode, f"Native game mode did not load: {GAME_MODE_PATH}"
    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P3/Maps")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        assert levels.load_level(MAP), f"Could not load map: {MAP}"
        world = unreal.EditorLevelLibrary.get_editor_world()
        tags = [str(tag) for tag in world.get_world_settings().get_editor_property("tags")]
        assert OWNER_TAG in tags, "Existing map is not owned by the P3 M1 prototype; refusing to overwrite it"
        current = world.get_world_settings().get_editor_property("default_game_mode")
        assert current and current.get_path_name() == game_mode.get_path_name(), (
            "Existing map has an unexpected GameMode"
        )
        status = "existing_owned_map_validated"
    else:
        assert levels.new_level(MAP), f"Could not create map: {MAP}"
        world = unreal.EditorLevelLibrary.get_editor_world()
        settings = world.get_world_settings()
        settings.set_editor_property("tags", [unreal.Name(OWNER_TAG)])
        settings.set_editor_property("default_game_mode", game_mode)
        assert levels.save_current_level(), f"Could not save map: {MAP}"
        status = "new_isolated_map_saved"

    report.update({
        "status": status,
        "default_game_mode": GAME_MODE_PATH,
        "map_tags": [str(tag) for tag in world.get_world_settings().get_editor_property("tags")],
    })
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log("NAIWA_P3_M1_MAP_OK " + json.dumps(report, ensure_ascii=False))
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log_error(report["error"])
    raise
