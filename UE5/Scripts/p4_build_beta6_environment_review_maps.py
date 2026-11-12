"""Build fixed-camera review maps for imported Beta6 production environment plates."""
from __future__ import annotations

import json
import math
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = ROOT / "UE5/SourceAssets/P4/Environment/Beta6"
MANIFEST_PATH = SOURCE_ROOT / "P4_Beta6_EnvironmentArt_manifest_v01.json"
IMPORT_REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_EnvironmentArt_import_report_v01.json"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_EnvironmentReviewMaps_build_report_v01.json"
MATERIAL_ROOT = "/Game/Naiwa/P4/Materials/Beta6/EnvironmentReview"
MAP_ROOT = "/Game/Naiwa/P4/Maps"
GAME_MODE_PATH = "/Script/NaiwaRuntime.NaiwaP4ArtIntegrationGameMode"
CAMERA_PATH = "/Script/NaiwaRuntime.NaiwaP4CameraRig"
OWNER_TAG = "NaiwaP4Beta6EnvironmentReview"
ACTOR_TAG = "NaiwaP4Beta6EnvironmentReviewGenerated"
ARENA_DEPTH = 420.0
CAMERA_PITCH_DEGREES = 50.0
CAMERA_DISTANCE = 1900.0
HORIZONTAL_FOV_DEGREES = 37.0
BACKDROP_EXTRA_DISTANCE = 650.0

report = {
    "schema": "naiwa.p4.beta6.environment_review_maps/1",
    "status": "running",
    "engine": unreal.SystemLibrary.get_engine_version(),
    "maps": [],
    "runtime_played": False,
}


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
    return center, unreal.Vector(-width / 100.0, height / 100.0, 1.0)


def create_material(chapter_number: str, texture: object) -> tuple[object, str]:
    unreal.EditorAssetLibrary.make_directory(MATERIAL_ROOT)
    name = f"M_P4_Beta6_chapter_{int(chapter_number)}_EnvironmentReview_v01"
    asset_path = f"{MATERIAL_ROOT}/{name}"
    material = unreal.load_asset(asset_path)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew()
        )
    if not material:
        raise RuntimeError(f"Could not create environment review material: {asset_path}")

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
        "parameter_name": unreal.Name("P4Beta6EnvironmentTexture"),
    })
    if not unreal.MaterialEditingLibrary.connect_material_property(
        sample, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    ):
        raise RuntimeError(f"Could not connect the environment texture to {asset_path}")
    errors = unreal.MaterialEditingLibrary.recompile_material(material)
    if errors:
        raise RuntimeError(f"Environment review material compile errors for {asset_path}: {errors}")
    if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save environment review material: {asset_path}")
    return material, asset_path


def set_owned(actor: object, label: str) -> None:
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(ACTOR_TAG)])


def prepare_map(chapter_number: str, material: object, game_mode: object,
                camera_class: object, plane_mesh: object) -> str:
    number = int(chapter_number)
    chapter_id = f"chapter_{number}"
    map_path = f"{MAP_ROOT}/L_NaiwaP4_Beta6_{number:02d}EnvironmentReview_v01"
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        if not levels.load_level(map_path):
            raise RuntimeError(f"Could not load existing environment review map: {map_path}")
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        tags = [str(tag) for tag in world.get_world_settings().get_editor_property("tags")]
        if OWNER_TAG not in tags:
            raise RuntimeError(f"Refusing to modify an unowned environment review map: {map_path}")
        for actor in actors.get_all_level_actors():
            actor_tags = [str(tag) for tag in actor.get_editor_property("tags")]
            if ACTOR_TAG in actor_tags and not actors.destroy_actor(actor):
                raise RuntimeError(f"Could not clear generated actor: {actor.get_name()}")
    else:
        if not levels.new_level(map_path):
            raise RuntimeError(f"Could not create environment review map: {map_path}")
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world:
        raise RuntimeError(f"Environment review map has no editor world: {map_path}")

    world_settings = world.get_world_settings()
    world_settings.set_editor_property("tags", [unreal.Name(OWNER_TAG), unreal.Name(f"NaiwaP4Beta6_{chapter_id}")])
    world_settings.set_editor_property("default_game_mode", game_mode)
    camera = actors.spawn_actor_from_class(
        camera_class, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)
    )
    start = actors.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(550.0, 210.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)
    )
    center, scale = scene_plate_transform()
    plate = actors.spawn_actor_from_class(
        unreal.StaticMeshActor, center, unreal.Rotator(0.0, 0.0, 40.0)
    )
    if not camera or not start or not plate:
        raise RuntimeError(f"Could not create camera, player start, and backdrop for {chapter_id}")
    component = plate.get_editor_property("static_mesh_component")
    component.set_static_mesh(plane_mesh)
    component.set_material(0, material)
    plate.set_actor_scale3d(scale)
    set_owned(camera, f"P4Beta6_{chapter_id}_EnvironmentReviewCamera")
    set_owned(start, f"P4Beta6_{chapter_id}_EnvironmentReviewPlayerStart")
    set_owned(plate, f"P4Beta6_{chapter_id}_EnvironmentReviewPlate")
    if not levels.save_current_level():
        raise RuntimeError(f"Could not save environment review map: {map_path}")
    return map_path


def main() -> dict:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    import_report = json.loads(IMPORT_REPORT_PATH.read_text(encoding="utf-8"))
    if manifest.get("schema") != "naiwa.p4.beta6.environment_art_manifest/1":
        raise ValueError("Unexpected Beta6 environment art manifest schema")
    if import_report.get("schema") != "naiwa.p4.beta6.environment_art_import/1" or \
            import_report.get("status") != "textures_imported_pending_engineering_theme_binding_and_readability_review" or \
            len(import_report.get("assets", [])) != 5:
        raise ValueError("Five production environment textures must be imported before map creation")
    entries = manifest.get("assets", [])
    if [entry.get("chapter_id") for entry in entries] != [f"chapter_{i}" for i in range(2, 7)]:
        raise ValueError("Expected one production environment plate for each Chapter 2 through 6")
    imported_by_chapter = {row["chapter_id"]: row for row in import_report["assets"]}
    game_mode = unreal.load_class(None, GAME_MODE_PATH)
    camera_class = unreal.load_class(None, CAMERA_PATH)
    plane_mesh = unreal.load_asset("/Engine/BasicShapes/Plane")
    if not game_mode or not camera_class or not plane_mesh:
        raise RuntimeError("P4 art integration GameMode, camera rig, or engine plane mesh is unavailable")

    built = []
    for entry in entries:
        chapter_id = entry["chapter_id"]
        imported = imported_by_chapter.get(chapter_id)
        if not imported or imported.get("texture_asset") != entry.get("ue_texture_asset"):
            raise RuntimeError(f"Production environment texture is missing from the import report: {chapter_id}")
        texture = unreal.load_asset(imported["texture_asset"])
        if not texture or not isinstance(texture, unreal.Texture2D):
            raise RuntimeError(f"Could not load imported production environment texture: {chapter_id}")
        number = chapter_id.rsplit("_", 1)[1]
        material, material_path = create_material(number, texture)
        map_path = prepare_map(number, material, game_mode, camera_class, plane_mesh)
        built.append({
            "asset_id": entry["asset_id"],
            "chapter_id": chapter_id,
            "source_path": entry["source_path"],
            "source_sha256": entry["sha256"],
            "texture_asset": imported["texture_asset"],
            "material_asset": material_path,
            "review_map": map_path,
            "actor_count": 3,
            "camera": "P4 fixed three-quarter camera, 50-degree pitch, 37-degree horizontal FOV",
            "runtime_bound": False,
            "build_status": "map_saved_pending_fixed_camera_readability_review",
        })
        report["maps"].append(map_path)

    return {
        "status": "review_maps_saved_pending_fixed_camera_readability_review",
        "map_count": len(built),
        "items": built,
        "runtime_played": False,
        "note": "Production environment review maps only; scene concept approval is recorded separately, and no Beta6 runtime theme bindings are changed.",
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
    unreal.log("NAIWA_P4_BETA6_ENVIRONMENT_REVIEW_MAPS " + json.dumps(report, ensure_ascii=False))
