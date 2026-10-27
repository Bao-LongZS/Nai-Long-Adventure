"""Create the isolated, sceneized P3 M2 review map without touching default maps."""
import json
import pathlib
import traceback
import unreal


ROOT = pathlib.Path(__file__).resolve().parents[2]
MAP = "/Game/Naiwa/P3/Maps/L_NaiwaP3_M2ScenarioTest"
OWNER_TAG = "NaiwaP3M2ScenarioTest"
START_TAG = "NaiwaP3M2ScenarioPlayerStart"
GAME_MODE_PATH = "/Script/NaiwaRuntime.NaiwaP3M2GameMode"
PREVIEW_ACTOR_PATH = "/Script/NaiwaRuntime.NaiwaP3M2ScenarioPreviewActor"
PORTRAIT_SOURCE_DIR = ROOT / "UE5/SourceAssets/P3/Portraits/Processed"
PORTRAIT_DESTINATION = "/Game/Naiwa/P3/Portraits/Processed"
PORTRAIT_MATERIAL = "/Game/Naiwa/P3/Materials/M_P3Portrait"
TILE_SOURCE_DIR = ROOT / "UE5/SourceAssets/P3/Tiles"
TILE_DESTINATION = "/Game/Naiwa/P3/Tiles"
TILE_MATERIAL = "/Game/Naiwa/P3/Materials/M_P3TileArt"
PORTRAIT_PREVIEW_MATERIALS = {
    "SPR_Player_Neutral_A": "M_P3Portrait_Player",
    "SPR_Naicha_Chase_A": "M_P3Portrait_Naicha",
    "SPR_Naisha_Rage_A": "M_P3Portrait_Naisha",
    "SPR_Naihao_Neutral_A": "M_P3Portrait_Naihao",
    "SPR_Naiyu_Neutral_A": "M_P3Portrait_Naiyu",
}
REPORT = ROOT / "UE5/Records/p3_m2/scenario_test_map_report.json"
report = {"map": MAP, "engine": unreal.SystemLibrary.get_engine_version(), "status": "running"}


def prepare_portrait_assets():
    """Import the approved transparent PNGs and build the unlit alpha-mask material."""
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    unreal.EditorAssetLibrary.make_directory(PORTRAIT_DESTINATION)
    portraits = {}
    for source in sorted(PORTRAIT_SOURCE_DIR.glob("SPR_*.png")):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", PORTRAIT_DESTINATION)
        task.set_editor_property("destination_name", source.stem)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        asset_tools.import_asset_tasks([task])
        asset_path = f"{PORTRAIT_DESTINATION}/{source.stem}"
        texture = unreal.load_asset(asset_path)
        assert texture, f"Could not import approved portrait: {source}"
        texture.set_editor_property("srgb", True)
        texture.set_editor_property("never_stream", True)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SHARPEN0)
        assert unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False), (
            f"Could not save portrait texture: {asset_path}"
        )
        portraits[source.stem] = texture

    expected = {
        "SPR_Player_Neutral_A", "SPR_Player_Hit_A", "SPR_Naicha_Chase_A",
        "SPR_Naicha_Catch_A", "SPR_Naisha_Rage_A", "SPR_Naihao_Neutral_A",
        "SPR_Naiyu_Neutral_A", "SPR_Naiyu_Attack_A",
    }
    assert expected.issubset(portraits), f"P3 portrait source set is incomplete: {sorted(portraits)}"
    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P3/Materials")
    material = (unreal.load_asset(PORTRAIT_MATERIAL)
                if unreal.EditorAssetLibrary.does_asset_exist(PORTRAIT_MATERIAL) else None)
    if not material:
        material = asset_tools.create_asset("M_P3Portrait", "/Game/Naiwa/P3/Materials",
            unreal.Material, unreal.MaterialFactoryNew())
    assert material, "Could not create the P3 portrait material"
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("opacity_mask_clip_value", 0.1)
    for expression in unreal.MaterialEditingLibrary.get_material_expressions(material):
        unreal.MaterialEditingLibrary.delete_material_expression(material, expression)
    texture_sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -420, 0
    )
    texture_sample.set_editor_property("parameter_name", unreal.Name("Portrait"))
    texture_sample.set_editor_property("texture", portraits["SPR_Player_Neutral_A"])
    unreal.MaterialEditingLibrary.connect_material_property(
        texture_sample, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        texture_sample, "A", unreal.MaterialProperty.MP_OPACITY_MASK
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_asset(PORTRAIT_MATERIAL, only_if_is_dirty=False), (
        "Could not save the P3 portrait material"
    )

    # Editor preview components are serialized into the map. Give each portrait a
    # persistent material with its texture baked into the graph instead of assigning
    # a transient UMaterialInstanceDynamic that can disappear after reopening the map.
    for texture_name, material_name in PORTRAIT_PREVIEW_MATERIALS.items():
        texture = portraits[texture_name]
        material_path = f"/Game/Naiwa/P3/Materials/{material_name}"
        preview_material = (unreal.load_asset(material_path)
                            if unreal.EditorAssetLibrary.does_asset_exist(material_path) else None)
        if not preview_material:
            preview_material = asset_tools.create_asset(
                material_name, "/Game/Naiwa/P3/Materials", unreal.Material, unreal.MaterialFactoryNew()
            )
        assert preview_material, f"Could not create the preview material: {material_path}"
        preview_material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        preview_material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        preview_material.set_editor_property("two_sided", True)
        preview_material.set_editor_property("opacity_mask_clip_value", 0.1)
        for expression in unreal.MaterialEditingLibrary.get_material_expressions(preview_material):
            unreal.MaterialEditingLibrary.delete_material_expression(preview_material, expression)
        portrait_sample = unreal.MaterialEditingLibrary.create_material_expression(
            preview_material, unreal.MaterialExpressionTextureSample, -420, 0
        )
        portrait_sample.set_editor_property("texture", texture)
        unreal.MaterialEditingLibrary.connect_material_property(
            portrait_sample, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR
        )
        unreal.MaterialEditingLibrary.connect_material_property(
            portrait_sample, "A", unreal.MaterialProperty.MP_OPACITY_MASK
        )
        unreal.MaterialEditingLibrary.recompile_material(preview_material)
        assert unreal.EditorAssetLibrary.save_asset(material_path, only_if_is_dirty=False), (
            f"Could not save the portrait preview material: {material_path}"
        )
    return sorted(portraits)


def prepare_tile_art_assets():
    """Import deterministic top-down textures and create one tintable unlit material."""
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    unreal.EditorAssetLibrary.make_directory(TILE_DESTINATION)
    textures = {}
    for source in sorted(TILE_SOURCE_DIR.glob("T_P3*.png")):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", TILE_DESTINATION)
        task.set_editor_property("destination_name", source.stem)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        asset_tools.import_asset_tasks([task])
        asset_path = f"{TILE_DESTINATION}/{source.stem}"
        texture = unreal.load_asset(asset_path)
        assert texture, f"Could not import P3 board texture: {source}"
        texture.set_editor_property("srgb", True)
        texture.set_editor_property("never_stream", True)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SHARPEN0)
        assert unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False), (
            f"Could not save P3 board texture: {asset_path}"
        )
        textures[source.stem] = texture

    expected = {
        "T_P3Floor_A", "T_P3Floor_B", "T_P3HardStone", "T_P3WoodCrate", "T_P3ItemCrate"
    }
    assert expected.issubset(textures), f"P3 tile texture source set is incomplete: {sorted(textures)}"
    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P3/Materials")
    material = (unreal.load_asset(TILE_MATERIAL)
                if unreal.EditorAssetLibrary.does_asset_exist(TILE_MATERIAL) else None)
    if not material:
        material = asset_tools.create_asset(
            "M_P3TileArt", "/Game/Naiwa/P3/Materials", unreal.Material, unreal.MaterialFactoryNew()
        )
    assert material, "Could not create the P3 tile art material"
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    for expression in unreal.MaterialEditingLibrary.get_material_expressions(material):
        unreal.MaterialEditingLibrary.delete_material_expression(material, expression)
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -600, 0
    )
    sample.set_editor_property("parameter_name", unreal.Name("TileTexture"))
    sample.set_editor_property("texture", textures["T_P3Floor_A"])
    tint = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -600, 260
    )
    tint.set_editor_property("parameter_name", unreal.Name("Color"))
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    multiply = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -260, 0
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(sample, "RGB", multiply, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(tint, "", multiply, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_asset(TILE_MATERIAL, only_if_is_dirty=False), (
        "Could not save the P3 tile art material"
    )
    return sorted(textures), TILE_MATERIAL


try:
    portrait_assets = prepare_portrait_assets()
    tile_assets, tile_material = prepare_tile_art_assets()
    game_mode = unreal.load_class(None, GAME_MODE_PATH)
    assert game_mode, f"Native M2 game mode did not load: {GAME_MODE_PATH}"
    preview_class = unreal.load_class(None, PREVIEW_ACTOR_PATH)
    assert preview_class, f"Native M2 scenario preview did not load: {PREVIEW_ACTOR_PATH}"
    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P3/Maps")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        assert levels.load_level(MAP), f"Could not load map: {MAP}"
        status = "existing_owned_scenario_map_updated"
    else:
        assert levels.new_level(MAP), f"Could not create map: {MAP}"
        status = "new_isolated_scenario_map_saved"

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world, "Editor did not create a world for the M2 scenario map"

    settings = world.get_world_settings()
    if status == "new_isolated_scenario_map_saved":
        settings.set_editor_property("tags", [unreal.Name(OWNER_TAG)])
        settings.set_editor_property("default_game_mode", game_mode)
    else:
        tags = [str(tag) for tag in settings.get_editor_property("tags")]
        assert OWNER_TAG in tags, "Existing map is not owned by the M2 scenario test; refusing to change it"
        current = settings.get_editor_property("default_game_mode")
        assert current and current.get_path_name() == game_mode.get_path_name(), (
            "Existing M2 scenario map has an unexpected GameMode"
        )

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors()
    start_exists = any(START_TAG in [str(tag) for tag in actor.get_editor_property("tags")]
                       for actor in actors if actor.get_class().get_path_name() == "/Script/Engine.PlayerStart")
    if not start_exists:
        spawn = actor_subsystem.spawn_actor_from_class(
            unreal.PlayerStart,
            unreal.Vector(-900.0, 600.0, 0.0),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        assert spawn, "Could not place a PlayerStart at the M2 board spawn cell"
        spawn.set_editor_property("tags", [unreal.Name(START_TAG)])
        spawn.set_actor_label("M2 测试关玩家出生点")

    preview = next((actor for actor in actors
                    if actor.get_class().get_path_name() == PREVIEW_ACTOR_PATH), None)
    if not preview:
        preview = actor_subsystem.spawn_actor_from_class(
            preview_class, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)
        )
        assert preview, "Could not place the M2 editor scene preview actor"
        preview.set_actor_label("M2 同种子场景预览（游戏内自动隐藏）")
    preview.build_editor_preview()

    assert levels.save_current_level(), f"Could not save map: {MAP}"
    report.update({
        "status": status,
        "default_game_mode": GAME_MODE_PATH,
        "map_tags": [str(tag) for tag in settings.get_editor_property("tags")],
        "attempt_count": 0,
        "layout_seed": 320000,
        "board_size": "23x17",
        "player_start_world": [-900.0, 600.0, 0.0],
        "player_start_tag": START_TAG,
        "editor_scene_preview": PREVIEW_ACTOR_PATH,
        "portrait_assets": portrait_assets,
        "portrait_material": PORTRAIT_MATERIAL,
        "portrait_preview_materials": sorted(PORTRAIT_PREVIEW_MATERIALS.values()),
        "tile_texture_assets": tile_assets,
        "tile_material": tile_material,
        "actor_style": "approved 2D portrait planes with per-cell framing; NaiCha chase/catch and NaiYu neutral/attack art",
        "runtime_scene": "M2 GameMode creates the playable board, HUD, enemies, items, fog, and mechanics on BeginPlay; baked editor preview actors are hidden in game.",
    })
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()

REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
unreal.log("NAIWA_P3_M2_SCENARIO_MAP " + json.dumps(report, ensure_ascii=False))
if report["status"] not in {
    "new_isolated_scenario_map_saved",
    "existing_owned_scenario_map_updated",
}:
    raise RuntimeError(report.get("error", report["status"]))
