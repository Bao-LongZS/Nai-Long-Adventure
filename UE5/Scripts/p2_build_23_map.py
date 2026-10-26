"""Create or validate the standalone runtime-generated P2 2-3 map."""
import json
import pathlib
import traceback
import unreal

ROOT = pathlib.Path(__file__).resolve().parents[2]
MAP = "/Game/Naiwa/P2/Maps/L_NaiwaP2_23"
OWNER_TAG = "NaiwaP2Level23"
GAME_MODE_PATH = "/Script/NaiwaRuntime.NaiwaP2GameMode"
REPORT = ROOT / ".workbuddy/work/20261020_全物理改造/p2_23_map_result.json"

report = {
    "map": MAP,
    "engine": unreal.SystemLibrary.get_engine_version(),
    "status": "running",
}

try:
    game_mode = unreal.load_class(None, GAME_MODE_PATH)
    assert game_mode, f"Could not load P2 GameMode: {GAME_MODE_PATH}"
    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P2/Maps")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        assert levels.load_level(MAP), f"Could not load existing map: {MAP}"
        world = unreal.EditorLevelLibrary.get_editor_world()
        settings = world.get_world_settings()
        tags = [str(tag) for tag in settings.get_editor_property("tags")]
        assert OWNER_TAG in tags, "Existing map is not owned by the runtime-generated P2 2-3 flow"
        current = settings.get_editor_property("default_game_mode")
        assert current and current.get_path_name() == game_mode.get_path_name(), (
            "Existing map has an unexpected GameMode; inspect it before changing it"
        )
        status = "existing_runtime_map_validated"
    else:
        assert levels.new_level(MAP), f"Could not create map: {MAP}"
        world = unreal.EditorLevelLibrary.get_editor_world()
        settings = world.get_world_settings()
        settings.set_editor_property("tags", [unreal.Name(OWNER_TAG)])
        settings.set_editor_property("default_game_mode", game_mode)
        assert levels.save_current_level(), f"Could not save map: {MAP}"
        status = "new_runtime_generated_map_saved"

    report.update({
        "status": status,
        "default_game_mode": GAME_MODE_PATH,
        "map_tags": [str(tag) for tag in world.get_world_settings().get_editor_property("tags")],
        "level_selection": "inferred from L_NaiwaP2_23 map name; no command-line override required",
        "geometry": "generated at runtime from FSceneLibrary::Make(2-3) with Chaos bodies",
    })
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log("NAIWA_P2_23_MAP_OK " + json.dumps(report, ensure_ascii=False))
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log_error(report["error"])
    raise
