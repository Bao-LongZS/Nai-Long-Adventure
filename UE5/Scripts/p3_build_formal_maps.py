"""Create isolated P3 formal 3-1 and 3-2 maps, preserving the review map."""
import json
import pathlib
import traceback
import unreal


ROOT = pathlib.Path(__file__).resolve().parents[2]
MAPS_DIR = "/Game/Naiwa/P3/Maps"
TEST_MAP = f"{MAPS_DIR}/L_NaiwaP3_M2ScenarioTest"
MAP_31 = f"{MAPS_DIR}/L_NaiwaP3_31"
MAP_32 = f"{MAPS_DIR}/L_NaiwaP3_32"
GAME_MODE_PATH = "/Script/NaiwaRuntime.NaiwaP3M2GameMode"
PREVIEW_ACTOR_PATH = "/Script/NaiwaRuntime.NaiwaP3M2ScenarioPreviewActor"
OWNER_TAGS = {MAP_31: "NaiwaP3FormalLevel31", MAP_32: "NaiwaP3FormalLevel32"}
SOURCE_TILES = ROOT / "UE5/SourceAssets/P3/Tiles"
TILE_DEST = "/Game/Naiwa/P3/Tiles"
MATERIAL_DEST = "/Game/Naiwa/P3/Materials"
REPORT = ROOT / "UE5/Records/p3_formal/formal_map_build_report.json"
report = {"status": "running", "maps": [MAP_31, MAP_32],
          "engine": unreal.SystemLibrary.get_engine_version()}


def import_tile_art():
    unreal.EditorAssetLibrary.make_directory(TILE_DEST)
    imported = []
    for source in sorted(SOURCE_TILES.glob("T_P3*.png")):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", TILE_DEST)
        task.set_editor_property("destination_name", source.stem)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        asset_path = f"{TILE_DEST}/{source.stem}"
        texture = unreal.load_asset(asset_path)
        assert texture, f"Could not import P3 material texture: {source}"
        texture.set_editor_property("srgb", True)
        texture.set_editor_property("never_stream", True)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        assert unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False), asset_path
        imported.append(asset_path)
    required = {"T_P3Floor_A_v2", "T_P3HardStone_v2", "T_P3WoodCrate_v2"}
    assert required.issubset({pathlib.Path(path).name for path in imported}), (
        f"Formal map textures are missing: {sorted(required - {pathlib.Path(path).name for path in imported})}"
    )
    return imported


def ensure_level(level_path, source_path=None):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(level_path):
        assert levels.load_level(level_path), f"Could not load formal map: {level_path}"
        return "existing_owned_formal_map"
    if source_path:
        assert unreal.EditorAssetLibrary.does_asset_exist(source_path), (
            f"Cannot preserve the tested layout because the source map is missing: {source_path}"
        )
        duplicated = unreal.EditorAssetLibrary.duplicate_asset(source_path, level_path)
        assert duplicated, f"Could not duplicate source map into formal path: {level_path}"
        assert levels.load_level(level_path), f"Could not load duplicated formal map: {level_path}"
        return "formal_map_duplicated_from_review_map"
    assert levels.new_level(level_path), f"Could not create formal map: {level_path}"
    return "new_formal_map_created"


def configure_current_map(level_path, level_id, spawn_cell, width, height, game_mode, preview_class,
                         allow_empty_tags=False):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world, f"Editor world did not load: {level_path}"
    settings = world.get_world_settings()
    owner_tag = OWNER_TAGS[level_path]
    tags = [str(tag) for tag in settings.get_editor_property("tags")]
    if owner_tag not in tags:
        assert allow_empty_tags or "NaiwaP3M2ScenarioTest" in tags or any(tag in OWNER_TAGS.values() for tag in tags), (
            f"Map is not an owned P3 map; refusing to change it: {level_path}, tags={tags}"
        )
    settings.set_editor_property("tags", [unreal.Name(owner_tag)])
    settings.set_editor_property("default_game_mode", game_mode)

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors()
    start = next((actor for actor in actors
                  if actor.get_class().get_path_name() == "/Script/Engine.PlayerStart" and
                  (owner_tag in [str(tag) for tag in actor.get_editor_property("tags")] or
                   "NaiwaP3M2ScenarioPlayerStart" in [str(tag) for tag in actor.get_editor_property("tags")])), None)
    spawn_world = unreal.Vector((spawn_cell[0] - (width - 1) * 0.5) * 100.0,
                                (spawn_cell[1] - (height - 1) * 0.5) * 100.0, 0.0)
    if not start:
        start = actor_subsystem.spawn_actor_from_class(
            unreal.PlayerStart, spawn_world, unreal.Rotator(0.0, 0.0, 0.0))
    assert start, f"Could not place PlayerStart in formal map: {level_path}"
    start.set_actor_location(spawn_world, False, False)
    start.set_editor_property("tags", [unreal.Name(owner_tag)])
    start.set_actor_label(f"3-{level_id - 30} 正式玩家出生点")

    preview = next((actor for actor in actors
                    if actor.get_class().get_path_name() == PREVIEW_ACTOR_PATH), None)
    if not preview:
        preview = actor_subsystem.spawn_actor_from_class(
            preview_class, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    assert preview, f"Could not place authored scene preview in formal map: {level_path}"
    preview.set_actor_label(f"3-{level_id - 30} 正式布局预览（游戏内自动隐藏）")
    preview.set_editor_property("preview_level_id", level_id)
    preview.build_editor_preview()
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level(), (
        f"Could not save formal map: {level_path}"
    )
    return {"map": level_path, "level_id": level_id, "owner_tag": owner_tag,
            "spawn_cell": list(spawn_cell), "spawn_world": [spawn_world.x, spawn_world.y, spawn_world.z],
            "board_size": f"{width}x{height}", "default_game_mode": GAME_MODE_PATH}


try:
    tile_assets = import_tile_art()
    game_mode = unreal.load_class(None, GAME_MODE_PATH)
    preview_class = unreal.load_class(None, PREVIEW_ACTOR_PATH)
    assert game_mode and preview_class, "P3 runtime classes are not available; build the editor target first"
    unreal.EditorAssetLibrary.make_directory(MAPS_DIR)

    map32_status = ensure_level(MAP_32, TEST_MAP)
    map32 = configure_current_map(MAP_32, 32, (2, 14), 23, 17, game_mode, preview_class)

    map31_status = ensure_level(MAP_31)
    map31 = configure_current_map(MAP_31, 31, (0, 8), 11, 9, game_mode, preview_class,
                                  allow_empty_tags=map31_status == "new_formal_map_created")

    report.update({"status": "formal_maps_created", "source_review_map_preserved": TEST_MAP,
                   "tile_assets": tile_assets, "level_31": map31, "level_31_status": map31_status,
                   "level_32": map32, "level_32_status": map32_status,
                   "level_31_fog": False, "level_32_fog": True})
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()

REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("NAIWA_P3_FORMAL_MAPS " + json.dumps(report, ensure_ascii=False))
if report["status"] != "formal_maps_created":
    raise RuntimeError(report.get("error", report["status"]))
