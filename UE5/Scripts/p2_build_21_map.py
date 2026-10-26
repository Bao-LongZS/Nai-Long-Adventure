"""Create or validate the standalone native-game-mode map for P2 level 2-1."""
import json
import pathlib
import traceback
import unreal

ROOT = pathlib.Path(__file__).resolve().parents[2]
MAP = "/Game/Naiwa/P2/Maps/L_NaiwaP2_21"
OWNER_TAG = "NaiwaP2Level21"
PROMOTED_PROTOTYPE_TAG = "NaiwaP2M1bPrototype"
GAME_MODE_PATH = "/Script/NaiwaRuntime.NaiwaP2GameMode"
REPORT = ROOT / "UE5/Records/p2_21_map_result.json"

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
        if OWNER_TAG in tags:
            status = "existing_standalone_map_validated"
        elif PROMOTED_PROTOTYPE_TAG in tags:
            # The canonical level was created by promoting the already-tested
            # isolated prototype map in the editor. Retag it so the prototype
            # validator and this standalone-level validator remain distinct.
            settings.set_editor_property("tags", [unreal.Name(OWNER_TAG)])
            assert levels.save_current_level(), f"Could not save promoted map: {MAP}"
            status = "existing_prototype_promoted_to_standalone"
        else:
            raise AssertionError("Existing map is not marked as the standalone P2 2-1 map")
        current = settings.get_editor_property("default_game_mode")
        assert current and current.get_path_name() == game_mode.get_path_name(), (
            "Existing map has an unexpected GameMode; inspect it before changing it"
        )
    else:
        assert levels.new_level(MAP), f"Could not create map: {MAP}"
        world = unreal.EditorLevelLibrary.get_editor_world()
        settings = world.get_world_settings()
        settings.set_editor_property("tags", [unreal.Name(OWNER_TAG)])
        settings.set_editor_property("default_game_mode", game_mode)
        assert levels.save_current_level(), f"Could not save map: {MAP}"
        status = "new_standalone_map_saved"

    report.update({
        "status": status,
        "default_game_mode": GAME_MODE_PATH,
        "map_tags": [str(tag) for tag in world.get_world_settings().get_editor_property("tags")],
        "level_selection": "inferred from L_NaiwaP2_21 map name; no NaiwaP2Level override required",
        "geometry": "generated at runtime from FSceneLibrary::Make(2-1)",
    })
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log("NAIWA_P2_21_MAP_OK " + json.dumps(report, ensure_ascii=False))
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log_error(report["error"])
    raise
