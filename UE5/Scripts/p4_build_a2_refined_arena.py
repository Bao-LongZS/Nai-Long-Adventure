"""Create the P4 refined A2 review map and import its fixed-camera scene plate."""
from pathlib import Path
import hashlib
import json
import struct
import traceback
import unreal

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "UE5/SourceAssets/P4/Environment/Alpha/P4_A2_ArenaScene_v02.png"
MANIFEST = ROOT / "UE5/SourceAssets/P4/Manifests/P4_A2_ArenaScene_manifest_v02.json"
REPORT = ROOT / "UE5/Records/p4_alpha/evidence/A2/P4_A2_RefinedArena_build_report.json"
MAP = "/Game/Naiwa/P4/Maps/L_NaiwaP4_A2_RefinedArena_v02"
TEXTURE = "/Game/Naiwa/P4/Art/Alpha/A2/T_P4_A2_ArenaScene_v02"
MATERIAL = "/Game/Naiwa/P4/Materials/Alpha/A2/M_P4_A2_RefinedBackdrop_v02"
OWNER_TAG = "NaiwaP4A2RefinedArena"
ACTOR_TAG = "NaiwaP4A2RefinedArenaGenerated"
GAME_MODE = "/Script/NaiwaRuntime.NaiwaP4ArtIntegrationGameMode"
CAMERA = "/Script/NaiwaRuntime.NaiwaP4CameraRig"
report = {"status": "running", "map": MAP, "source": str(SOURCE),
          "engine": unreal.SystemLibrary.get_engine_version(), "level_opened_or_changed": False,
          "runtime_played": False}

def sha256(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()

def import_texture():
    if not SOURCE.is_file():
        raise FileNotFoundError(SOURCE)
    with SOURCE.open("rb") as stream:
        header = stream.read(24)
    if header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError("Arena scene plate is not a PNG")
    width, height = struct.unpack(">II", header[16:24])
    if width < 1600 or height < 900 or abs(width / height - 16.0 / 9.0) > 0.01:
        raise ValueError(f"Unexpected scene image aspect/resolution: {width}x{height}")
    texture = unreal.load_asset(TEXTURE)
    if not texture:
        unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P4/Art/Alpha/A2")
        task = unreal.AssetImportTask()
        task.set_editor_properties({"filename": str(SOURCE), "destination_path": "/Game/Naiwa/P4/Art/Alpha/A2",
                                    "destination_name": "T_P4_A2_ArenaScene_v02", "replace_existing": False,
                                    "automated": True, "save": False})
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(TEXTURE)
    if not texture or not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Texture import failed: {TEXTURE}")
    settings = {"srgb": True, "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
                "compression_no_alpha": False, "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_SHARPEN0,
                "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI, "never_stream": True}
    texture.set_editor_properties(settings)
    for key, expected in settings.items():
        if texture.get_editor_property(key) != expected:
            raise RuntimeError(f"Texture readback failed: {key}")
    if not unreal.EditorAssetLibrary.save_asset(TEXTURE, only_if_is_dirty=False):
        raise RuntimeError("Could not save arena scene texture")
    return texture, width, height

def make_material(texture):
    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P4/Materials/Alpha/A2")
    material = unreal.load_asset(MATERIAL)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_P4_A2_RefinedBackdrop_v02", "/Game/Naiwa/P4/Materials/Alpha/A2", unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError("Could not create P4 refined backdrop material")
    material.set_editor_properties({"shading_model": unreal.MaterialShadingModel.MSM_UNLIT,
                                    "blend_mode": unreal.BlendMode.BLEND_OPAQUE, "two_sided": True})
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, 0, 0)
    sample.set_editor_properties({"texture": texture, "parameter_name": unreal.Name("P4ArenaSceneTexture")})
    if not unreal.MaterialEditingLibrary.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Could not connect arena texture to emissive color")
    errors = unreal.MaterialEditingLibrary.recompile_material(material)
    if errors:
        raise RuntimeError(f"Backdrop material compile errors: {errors}")
    if not unreal.EditorAssetLibrary.save_asset(MATERIAL, only_if_is_dirty=False):
        raise RuntimeError("Could not save refined backdrop material")
    return material

def create_map(game_mode, camera_class):
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        raise RuntimeError(f"Refusing to replace existing map: {MAP}")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.new_level(MAP):
        raise RuntimeError(f"Could not create map: {MAP}")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world:
        raise RuntimeError("New P4 map has no editor world")
    world_settings = world.get_world_settings()
    world_settings.set_editor_property("tags", [unreal.Name(OWNER_TAG)])
    world_settings.set_editor_property("default_game_mode", game_mode)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    camera = actors.spawn_actor_from_class(camera_class, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(550, 210, 0), unreal.Rotator(0, 0, 0))
    if not camera or not start:
        raise RuntimeError("Could not spawn P4 camera or PlayerStart")
    camera.set_actor_label("P4A2_RefinedFixedCamera")
    start.set_actor_label("P4A2_RefinedPlayerStart")
    camera.set_editor_property("tags", [unreal.Name(ACTOR_TAG)])
    start.set_editor_property("tags", [unreal.Name(ACTOR_TAG)])
    if not levels.save_current_level():
        raise RuntimeError("Could not save refined P4 A2 map")
    report["level_opened_or_changed"] = True

def main():
    metadata = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if metadata.get("status") != "owner_requested_ue_preview":
        raise ValueError("Arena scene manifest was not requested for UE preview")
    if not SOURCE.is_file():
        raise FileNotFoundError(SOURCE)
    source_hash = sha256(SOURCE)
    if metadata.get("source_sha256", "").lower() != source_hash:
        raise ValueError("Arena scene source hash does not match its approved manifest")
    with SOURCE.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError("Arena scene source is not a valid PNG")
    source_dimensions = list(struct.unpack(">II", header[16:24]))
    if source_dimensions != metadata.get("dimensions_px"):
        raise ValueError(f"Arena scene dimensions differ from manifest: {source_dimensions}")
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        raise RuntimeError(f"Refusing to modify assets because the target map already exists: {MAP}")
    texture, width, height = import_texture()
    material = make_material(texture)
    game_mode = unreal.load_class(None, GAME_MODE)
    camera_class = unreal.load_class(None, CAMERA)
    if not game_mode or not camera_class:
        raise RuntimeError("P4 runtime classes unavailable; build NaiwaRuntime first")
    create_map(game_mode, camera_class)
    metadata.update({"status": "imported_map_created_pending_owner_visual_review", "ue_texture_asset": TEXTURE,
                    "ue_material_asset": MATERIAL, "review_map": MAP})
    MANIFEST.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    report.update({"status": "map_created_pending_runtime_review", "source_sha256": source_hash,
                   "source_dimensions_px": [width, height], "texture_asset": TEXTURE, "material_asset": MATERIAL,
                   "texture_settings_readback": "TC_EditorIcon (uncompressed RGBA8), sRGB, UI group, Sharpen0 mips, non-streaming",
                   "map_actor_count": 2, "material_unlit_opaque": bool(material),
                   "limitations": ["No PIE/Standalone visual check was run.",
                                   "Camera framing and sprite-to-floor alignment remain owner runtime visual checks."]})

try:
    main()
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    raise
finally:
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("NAIWA_P4_A2_REFINED_ARENA " + json.dumps(report, ensure_ascii=False))
