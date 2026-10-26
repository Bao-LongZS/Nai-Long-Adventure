import os
import unreal


PROJECT_DIR = unreal.Paths.project_dir()
SOURCE_DIR = os.path.join(PROJECT_DIR, "SourceAssets", "MenuUI")
DESTINATION = "/Game/Naiwa/UI"
MAP_PACKAGE = "/Game/Naiwa/UI/L_NaiwaMainMenu"


def log(message):
    unreal.log("NAIWA_MENU_SETUP " + message)


unreal.EditorAssetLibrary.make_directory(DESTINATION)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

for filename, asset_name in (
    ("T_UI_StartNaiwa.png", "T_UI_StartNaiwa"),
    ("T_UI_LevelNaiwa_LeftFist.png", "T_UI_LevelNaiwa_LeftFist"),
    ("T_UI_Thumbnail_11.png", "T_UI_Thumbnail_11"),
    ("T_UI_Thumbnail_21.png", "T_UI_Thumbnail_21"),
    ("T_UI_Thumbnail_22.png", "T_UI_Thumbnail_22"),
    ("T_UI_Thumbnail_23.png", "T_UI_Thumbnail_23"),
):
    source_path = os.path.join(SOURCE_DIR, filename)
    if not os.path.isfile(source_path):
        raise RuntimeError("Missing source image: " + source_path)
    if unreal.EditorAssetLibrary.does_asset_exist(DESTINATION + "/" + asset_name):
        texture = unreal.EditorAssetLibrary.load_asset(DESTINATION + "/" + asset_name)
        log("texture_already_exists=" + asset_name)
    else:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source_path)
        task.set_editor_property("destination_path", DESTINATION)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("replace_existing", False)
        asset_tools.import_asset_tasks([task])
        texture = unreal.EditorAssetLibrary.load_asset(DESTINATION + "/" + asset_name)
        log("texture_imported=" + asset_name)
    if not texture:
        raise RuntimeError("Could not load imported texture: " + asset_name)
    for prop, enum_value in (
        ("lod_group", getattr(unreal.TextureGroup, "TEXTUREGROUP_UI", None)),
        ("mip_gen_settings", getattr(unreal.TextureMipGenSettings, "TMGS_NO_MIPMAPS", None)),
    ):
        if enum_value is not None:
            try:
                texture.set_editor_property(prop, enum_value)
            except Exception as exc:
                log("texture_setting_skipped=" + prop + " reason=" + str(exc))
    try:
        texture.set_editor_property("srgb", True)
    except Exception:
        pass
    unreal.EditorAssetLibrary.save_loaded_asset(texture)

if unreal.EditorAssetLibrary.does_asset_exist(MAP_PACKAGE):
    log("menu_map_already_exists=1")
    if not unreal.EditorLevelLibrary.load_level(MAP_PACKAGE):
        raise RuntimeError("Could not open existing menu map")
else:
    if not unreal.EditorLevelLibrary.new_level(MAP_PACKAGE):
        raise RuntimeError("Could not create menu map")
    log("menu_map_created=1")

world = unreal.EditorLevelLibrary.get_editor_world()
if not world:
    raise RuntimeError("Editor did not provide the new menu world")
menu_game_mode = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaMenuGameMode")
if not menu_game_mode:
    raise RuntimeError("Could not load ANaiwaMenuGameMode")
world.get_world_settings().set_editor_property("default_game_mode", menu_game_mode)
if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Could not save menu level")
log("menu_map_saved_with_game_mode=1")
