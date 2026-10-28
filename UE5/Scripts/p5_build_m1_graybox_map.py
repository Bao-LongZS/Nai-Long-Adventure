"""Create or validate the isolated P5 M1 prototype map."""
import json
import pathlib
import traceback
import unreal


ROOT = pathlib.Path(__file__).resolve().parents[2]
MAP = "/Game/Naiwa/P5/Maps/L_NaiwaP5_M1Prototype"
OWNER_TAG = "NaiwaP5M1Prototype"
GAME_MODE_PATH = "/Script/NaiwaRuntime.NaiwaP5PrototypeGameMode"
REPORT = ROOT / "UE5/Records/p5_m1/m1_map_build_report.json"
report = {"map": MAP, "engine": unreal.SystemLibrary.get_engine_version(), "status": "running"}

try:
    game_mode = unreal.load_class(None, GAME_MODE_PATH)
    assert game_mode, f"Native game mode did not load: {GAME_MODE_PATH}"
    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P5/Maps")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        assert levels.load_level(MAP), f"Could not load map: {MAP}"
        world = unreal.EditorLevelLibrary.get_editor_world()
        settings = world.get_world_settings()
        tags = [str(tag) for tag in settings.get_editor_property("tags")]
        assert OWNER_TAG in tags, "Existing map is not marked as this P5 prototype; refusing to overwrite it"
        current = settings.get_editor_property("default_game_mode")
        assert current and current.get_path_name() == game_mode.get_path_name(), (
            "Existing map has an unexpected GameMode; inspect it before changing it"
        )
        status = "existing_owned_map_validated"
    else:
        assert levels.new_level(MAP), f"Could not create map: {MAP}"
        world = unreal.EditorLevelLibrary.get_editor_world()
        settings = world.get_world_settings()
        settings.set_editor_property("tags", [unreal.Name(OWNER_TAG)])
        settings.set_editor_property("default_game_mode", game_mode)
        start = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.PlayerStart, unreal.Vector(0.0, 0.0, 80.0), unreal.Rotator(0.0, 0.0, 0.0)
        )
        assert start, "Could not create P5 player start"
        start.set_actor_label("NAIWA_P5_M1_PlayerStart")
        light = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.DirectionalLight, unreal.Vector(-1000.0, -2000.0, 3500.0), unreal.Rotator(-45.0, 0.0, 0.0)
        )
        assert light, "Could not create graybox directional light"
        light.set_actor_label("NAIWA_P5_M1_GrayboxLight")
        assert levels.save_current_level(), f"Could not save map: {MAP}"
        status = "new_isolated_map_saved"

    report.update({
        "status": status,
        "default_game_mode": GAME_MODE_PATH,
        "map_tags": [str(tag) for tag in world.get_world_settings().get_editor_property("tags")],
        "runtime_track_actor": "/Script/NaiwaRuntime.NaiwaP5PrototypeTrackActor",
        "runtime_pawn": "/Script/NaiwaRuntime.NaiwaP5PrototypePawn",
        "route_fixture": "temporary 100 m graybox; events are in FNaiwaP5TrackData",
    })
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log("NAIWA_P5_M1_MAP_OK " + json.dumps(report, ensure_ascii=False))
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log_error(report["error"])
    raise
