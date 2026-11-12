"""Create an isolated P4 Beta6 gameplay test map after the native class is compiled."""
from __future__ import annotations

import json
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_InternalTestMap_build_report_v01.json"
MAP_PATH = "/Game/Naiwa/P4/Maps/L_NaiwaP4_Beta6_InternalTest_v01"
GAME_MODE_PATH = "/Script/NaiwaRuntime.NaiwaP4Beta6TestGameMode"
CAMERA_PATH = "/Script/NaiwaRuntime.NaiwaP4CameraRig"
OWNER_TAG = "NaiwaP4Beta6InternalTestMap"
ACTOR_TAG = "NaiwaP4Beta6InternalTestGenerated"
REQUIRED_TABLES = (
    "/Game/Naiwa/P4/Data/DT_P4_Beta6_Chapters",
    "/Game/Naiwa/P4/Data/DT_P4_Beta6_Themes",
    "/Game/Naiwa/P4/Data/DT_P4_Beta6_WaveSpawns",
    "/Game/Naiwa/P4/Data/DT_P4_Beta6_Enemies",
    "/Game/Naiwa/P4/Data/DT_P4_Beta6_Bosses",
    "/Game/Naiwa/P4/Data/DT_P4_Beta6_BossAttacks",
    "/Game/Naiwa/P4/Data/DT_P4_Upgrade_Alpha",
)

report = {
    "schema": "naiwa.p4.beta6_internal_test_map_build/1",
    "status": "running",
    "engine": unreal.SystemLibrary.get_engine_version(),
    "map": MAP_PATH,
    "runtime_played": False,
}


def set_owned(actor: object, label: str) -> None:
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(ACTOR_TAG)])


def main() -> dict:
    game_mode = unreal.load_class(None, GAME_MODE_PATH)
    camera_class = unreal.load_class(None, CAMERA_PATH)
    plane_mesh = unreal.load_asset("/Engine/BasicShapes/Plane")
    if not game_mode or not camera_class or not plane_mesh:
        raise RuntimeError("Compiled Beta6 GameMode, P4 camera rig, or engine plane mesh is unavailable.")

    missing_tables = [path for path in REQUIRED_TABLES if not unreal.EditorAssetLibrary.does_asset_exist(path)]
    if missing_tables:
        raise RuntimeError(f"Beta6 internal map requires imported progression DataTables: {missing_tables}")

    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not levels.load_level(MAP_PATH):
            raise RuntimeError(f"Could not load existing Beta6 internal map: {MAP_PATH}")
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        tags = [str(tag) for tag in world.get_world_settings().get_editor_property("tags")]
        if OWNER_TAG not in tags:
            raise RuntimeError(f"Refusing to modify an unowned map: {MAP_PATH}")
        for actor in actor_subsystem.get_all_level_actors():
            actor_tags = [str(tag) for tag in actor.get_editor_property("tags")]
            if ACTOR_TAG in actor_tags and not actor_subsystem.destroy_actor(actor):
                raise RuntimeError(f"Could not clear generated actor: {actor.get_name()}")
    else:
        if not levels.new_level(MAP_PATH):
            raise RuntimeError(f"Could not create Beta6 internal map: {MAP_PATH}")
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world:
        raise RuntimeError(f"Beta6 internal map has no editor world: {MAP_PATH}")

    world_settings = world.get_world_settings()
    world_settings.set_editor_property("tags", [unreal.Name(OWNER_TAG), unreal.Name("NaiwaP4Beta6")])
    world_settings.set_editor_property("default_game_mode", game_mode)

    camera = actor_subsystem.spawn_actor_from_class(
        camera_class, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)
    )
    start = actor_subsystem.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(550.0, 210.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)
    )
    floor = actor_subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(550.0, 210.0, -8.0), unreal.Rotator(0.0, 0.0, 0.0)
    )
    if not camera or not start or not floor:
        raise RuntimeError("Could not create the camera rig, player start, and neutral test floor.")

    floor_component = floor.get_editor_property("static_mesh_component")
    floor_component.set_static_mesh(plane_mesh)
    floor.set_actor_scale3d(unreal.Vector(11.0, 4.2, 1.0))
    if unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial"):
        floor_component.set_material(0, unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial"))
    set_owned(camera, "P4Beta6_InternalTestCamera")
    set_owned(start, "P4Beta6_InternalTestPlayerStart")
    set_owned(floor, "P4Beta6_InternalTestNeutralFloor")

    if not levels.save_current_level():
        raise RuntimeError(f"Could not save Beta6 internal test map: {MAP_PATH}")
    return {
        "status": "map_saved_pending_runtime_acceptance",
        "default_game_mode": GAME_MODE_PATH,
        "required_tables": list(REQUIRED_TABLES),
        "actors": ["fixed P4 camera rig", "PlayerStart", "neutral test floor"],
        "runtime_played": False,
        "note": "Gameplay test map only; the five chapter scene preview maps and project default GameMode are unchanged.",
    }


try:
    report.update(main())
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    raise
finally:
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("NAIWA_P4_BETA6_INTERNAL_TEST_MAP " + json.dumps(report, ensure_ascii=False))
