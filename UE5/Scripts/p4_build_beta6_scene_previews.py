"""Import the approved Beta6 scene plates and build isolated P4 review maps."""
from __future__ import annotations

import hashlib
import json
import math
import struct
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
P4_SOURCE = ROOT / "UE5/SourceAssets/P4"
MANIFEST_PATH = P4_SOURCE / "Manifests/P4_Beta6_ArtDirection_manifest_v02.json"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_ScenePreview_build_report_v01.json"
TEXTURE_ROOT = "/Game/Naiwa/P4/Art/Beta6/Scenes"
MATERIAL_ROOT = "/Game/Naiwa/P4/Materials/Beta6/Scenes"
MAP_ROOT = "/Game/Naiwa/P4/Maps"
GAME_MODE_PATH = "/Script/NaiwaRuntime.NaiwaP4ArtIntegrationGameMode"
CAMERA_PATH = "/Script/NaiwaRuntime.NaiwaP4CameraRig"
OWNER_TAG = "NaiwaP4Beta6ScenePreview"
ACTOR_TAG = "NaiwaP4Beta6ScenePreviewGenerated"
CAMERA_PITCH_DEGREES = 50.0
ARENA_DEPTH = 420.0
CAMERA_DISTANCE = 1900.0
HORIZONTAL_FOV_DEGREES = 37.0
BACKDROP_EXTRA_DISTANCE = 650.0

report = {
    "schema": "naiwa.p4.beta6_scene_preview_build/1",
    "status": "running",
    "engine": unreal.SystemLibrary.get_engine_version(),
    "maps_created": [],
    "runtime_played": False,
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def inspect_png(path: Path) -> tuple[int, int]:
    if not path.is_file():
        raise FileNotFoundError(path)
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"Scene source is not a PNG: {path}")
    width, height = struct.unpack(">II", header[16:24])
    if width < 1600 or height < 900 or abs(width / height - 16.0 / 9.0) > 0.01:
        raise ValueError(f"Scene image must be at least 1600x900 at 16:9: {width}x{height}")
    return width, height


def import_texture(entry: dict) -> tuple[object, str, int, int]:
    source = (ROOT / entry["source_path"]).resolve()
    source_hash = sha256_file(source)
    if source_hash.lower() != entry.get("sha256", "").lower():
        raise ValueError(f"Approved scene hash mismatch: {source}")
    width, height = inspect_png(source)
    asset_name = f"T_P4_Beta6_{entry['chapter_id']}_Scene_v02"
    asset_path = f"{TEXTURE_ROOT}/{asset_name}"
    texture = unreal.load_asset(asset_path)
    imported_now = False
    if not texture:
        unreal.EditorAssetLibrary.make_directory(TEXTURE_ROOT)
        task = unreal.AssetImportTask()
        task.set_editor_properties({
            "filename": str(source),
            "destination_path": TEXTURE_ROOT,
            "destination_name": asset_name,
            "replace_existing": False,
            "automated": True,
            "save": False,
        })
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(asset_path)
        imported_now = True
    if not texture or not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Could not import Beta6 scene texture: {asset_path}")
    settings = {
        "srgb": True,
        "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        "compression_no_alpha": True,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_SHARPEN0,
        "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
        "never_stream": True,
    }
    texture.set_editor_properties(settings)
    for key, expected in settings.items():
        if texture.get_editor_property(key) != expected:
            raise RuntimeError(f"Texture settings readback failed for {asset_path}: {key}")
    if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save scene texture: {asset_path}")
    return texture, asset_path, width, height


def create_material(entry: dict, texture: object) -> tuple[object, str]:
    chapter = entry["chapter_id"]
    asset_name = f"M_P4_Beta6_{chapter}_Backdrop_v02"
    asset_path = f"{MATERIAL_ROOT}/{asset_name}"
    unreal.EditorAssetLibrary.make_directory(MATERIAL_ROOT)
    material = unreal.load_asset(asset_path)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name, MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew()
        )
    if not material:
        raise RuntimeError(f"Could not create scene material: {asset_path}")
    material.set_editor_properties({
        "shading_model": unreal.MaterialShadingModel.MSM_UNLIT,
        "blend_mode": unreal.BlendMode.BLEND_OPAQUE,
        "two_sided": True,
    })
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, 0, 0
    )
    sample.set_editor_properties({
        "texture": texture,
        "parameter_name": unreal.Name("P4Beta6SceneTexture"),
    })
    if not unreal.MaterialEditingLibrary.connect_material_property(
        sample, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    ):
        raise RuntimeError(f"Could not connect source texture to scene material: {asset_path}")
    errors = unreal.MaterialEditingLibrary.recompile_material(material)
    if errors:
        raise RuntimeError(f"Scene material compile errors for {asset_path}: {errors}")
    if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save scene material: {asset_path}")
    return material, asset_path


def scene_plate_transform() -> tuple[unreal.Vector, unreal.Vector]:
    pitch = math.radians(CAMERA_PITCH_DEGREES)
    arena_center = unreal.Vector(550.0, ARENA_DEPTH * 0.5, 0.0)
    camera_offset = unreal.Vector(
        0.0,
        -CAMERA_DISTANCE * math.cos(pitch),
        CAMERA_DISTANCE * math.sin(pitch),
    )
    camera_location = arena_center + camera_offset
    camera_forward = unreal.Vector(0.0, math.cos(pitch), -math.sin(pitch))
    distance = CAMERA_DISTANCE + BACKDROP_EXTRA_DISTANCE
    center = camera_location + camera_forward * distance
    width = 2.0 * distance * math.tan(math.radians(HORIZONTAL_FOV_DEGREES * 0.5)) * 1.06
    height = width * (9.0 / 16.0)
    # The plane faces the calibrated camera; mirroring its local X aligns its UVs
    # with the camera's right vector at yaw 90 degrees.
    scale = unreal.Vector(-width / 100.0, height / 100.0, 1.0)
    return center, scale


def set_owned(actor: object, label: str) -> object:
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(ACTOR_TAG)])
    return actor


def prepare_map(entry: dict, material: object, game_mode: object, camera_class: object, plane_mesh: object) -> str:
    chapter = entry["chapter_id"]
    chapter_number = str(int(chapter.rsplit("_", 1)[1])).zfill(2)
    map_path = f"{MAP_ROOT}/L_NaiwaP4_Beta6_{chapter_number}ScenePreview_v01"
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        if not levels.load_level(map_path):
            raise RuntimeError(f"Could not load existing scene preview map: {map_path}")
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        tags = [str(tag) for tag in world.get_world_settings().get_editor_property("tags")]
        if OWNER_TAG not in tags:
            raise RuntimeError(f"Refusing to modify an unowned map: {map_path}")
        for actor in actor_subsystem.get_all_level_actors():
            actor_tags = [str(tag) for tag in actor.get_editor_property("tags")]
            if ACTOR_TAG in actor_tags and not actor_subsystem.destroy_actor(actor):
                raise RuntimeError(f"Could not clear generated actor: {actor.get_name()}")
    else:
        if not levels.new_level(map_path):
            raise RuntimeError(f"Could not create scene preview map: {map_path}")
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world:
        raise RuntimeError(f"Scene preview map has no editor world: {map_path}")
    world_settings = world.get_world_settings()
    world_settings.set_editor_property("tags", [unreal.Name(OWNER_TAG), unreal.Name(f"NaiwaP4Beta6_{chapter}")])
    world_settings.set_editor_property("default_game_mode", game_mode)

    camera = actor_subsystem.spawn_actor_from_class(camera_class, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    start = actor_subsystem.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(550.0, 210.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    background_center, background_scale = scene_plate_transform()
    background = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, background_center, unreal.Rotator(0.0, 0.0, 40.0))
    if not camera or not start or not background:
        raise RuntimeError(f"Could not create camera, player start, and scene plate for {chapter}")
    background_component = background.get_editor_property("static_mesh_component")
    background_component.set_static_mesh(plane_mesh)
    background_component.set_material(0, material)
    background.set_actor_scale3d(background_scale)
    set_owned(camera, f"P4Beta6_{chapter}_FixedCamera")
    set_owned(start, f"P4Beta6_{chapter}_PlayerStart")
    set_owned(background, f"P4Beta6_{chapter}_ScenePlate")
    if not levels.save_current_level():
        raise RuntimeError(f"Could not save scene preview map: {map_path}")
    return map_path


def main() -> dict:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    if manifest.get("schema") != "naiwa.p4.beta6_art_direction_preview/2":
        raise ValueError("Unexpected Beta6 scene manifest schema")
    entries = [entry for entry in manifest.get("assets", []) if entry.get("asset_id", "").endswith("_SCENE")]
    if [entry.get("chapter_id") for entry in entries] != [f"chapter_{i}" for i in range(2, 7)]:
        raise ValueError("Expected exactly the five approved Beta6 scenes for chapters 2 through 6")
    game_mode = unreal.load_class(None, GAME_MODE_PATH)
    camera_class = unreal.load_class(None, CAMERA_PATH)
    plane_mesh = unreal.load_asset("/Engine/BasicShapes/Plane")
    if not game_mode or not camera_class or not plane_mesh:
        raise RuntimeError("P4 preview game mode, camera rig, or plane mesh is unavailable")

    built_entries = []
    for entry in entries:
        texture, texture_path, width, height = import_texture(entry)
        material, material_path = create_material(entry, texture)
        map_path = prepare_map(entry, material, game_mode, camera_class, plane_mesh)
        built_entries.append({
            "asset_id": entry["asset_id"],
            "chapter_id": entry["chapter_id"],
            "source_path": entry["source_path"],
            "source_sha256": entry["sha256"],
            "dimensions_px": [width, height],
            "texture_asset": texture_path,
            "material_asset": material_path,
            "review_map": map_path,
            "map_actor_count": 3,
            "camera": "P4 fixed three-quarter camera, 50-degree pitch, 37-degree horizontal FOV",
            "texture_settings": "sRGB, TC_EditorIcon (uncompressed), no alpha, Sharpen0 mips, UI group, non-streaming",
            "build_status": "map_saved_pending_visual_review",
        })
        entry["status"] = "owner_approved_concept_imported_pending_fixed_camera_visual_review"
        entry["runtime_reference"] = "Beta6 review map only; pending fixed-camera visual review and production cleanup"
        entry["ue_texture_asset"] = texture_path
        entry["ue_material_asset"] = material_path
        entry["review_map"] = map_path
        report["maps_created"].append(map_path)

    manifest["status"] = "owner_approved_concepts_imported_pending_fixed_camera_visual_review"
    manifest["ue_preview_build"] = {
        "date": "2026-11-11",
        "status": "maps_saved_pending_visual_review",
        "engine": report["engine"],
        "items": built_entries,
        "runtime_scope": "Review maps only; scene concept plates are not production-cleaned runtime delivery assets.",
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return {
        "status": "maps_saved_pending_visual_review",
        "camera_setup": "P4 art integration camera rig; arena depth 420; pitch 50 degrees; 37-degree horizontal FOV",
        "scene_plate": "Each chapter has a unique unlit opaque plane sized to the calibrated camera frustum.",
        "maps": built_entries,
        "runtime_played": False,
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
    unreal.log("NAIWA_P4_BETA6_SCENE_PREVIEW " + json.dumps(report, ensure_ascii=False))
