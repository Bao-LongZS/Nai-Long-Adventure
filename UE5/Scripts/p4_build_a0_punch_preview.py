"""Build a separate P4 A0 punch-cycle preview map from the selected WIP frames.

This builder owns only /Game/Naiwa/P4/Maps/L_NaiwaP4_A0PunchPreview and
assets whose path starts with the P4 A0 folder below. It does not modify the
P4 W1 calibration map or any P1/P2/P3 content.
"""
from __future__ import annotations

import json
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = ROOT / "UE5/SourceAssets/P4/Characters/Alpha/A0_PunchSample"
MANIFEST_PATH = SOURCE_DIR / "P4_A0_PunchSample_manifest.json"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/W1/P4_A0_PunchSample_build_report.json"
MAP_PATH = "/Game/Naiwa/P4/Maps/L_NaiwaP4_A0PunchPreview"
ART_DEST = "/Game/Naiwa/P4/Art/Alpha/A0_PunchSample"
MATERIAL_DEST = "/Game/Naiwa/P4/Materials/Alpha/A0_PunchSample"
OWNER_TAG = "NaiwaP4A0PunchPreview"
ACTOR_TAG = "NaiwaP4A0PunchGenerated"
GAME_MODE_PATH = "/Script/NaiwaRuntime.NaiwaP4W1PreviewGameMode"
CAMERA_PATH = "/Script/NaiwaRuntime.NaiwaP4CameraRig"
GROUND_MATERIAL_PATH = "/Game/Naiwa/P4/Materials/W1/M_P4_W1_Ground"
SHADOW_MATERIAL_PATH = "/Game/Naiwa/P4/Materials/W1/M_P4_W1_SoftShadow_RadialV6"

ARENA_WIDTH = 1100.0
ARENA_DEPTH = 185.0
SPRITE_HEIGHT = 180.0
SPRITE_DEPTH_Y = 92.5
SPRITE_XS = (260.0, 550.0, 840.0)

report = {
    "status": "running",
    "map": MAP_PATH,
    "engine": unreal.SystemLibrary.get_engine_version(),
    "owner_tag": OWNER_TAG,
}


def import_texture(frame: dict):
    source = SOURCE_DIR / frame["file"]
    assert source.is_file(), f"Missing A0 sample frame: {source}"
    unreal.EditorAssetLibrary.make_directory(ART_DEST)
    asset_name = Path(frame["file"]).stem.replace("P4_A0_Punch_", "T_P4_A0_Punch_")
    asset_path = f"{ART_DEST}/{asset_name}"
    texture = unreal.load_asset(asset_path)
    if not texture:
        task = unreal.AssetImportTask()
        task.set_editor_properties({
            "filename": str(source),
            "destination_path": ART_DEST,
            "destination_name": asset_name,
            "replace_existing": False,
            "automated": True,
            "save": True,
        })
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(asset_path)
    assert texture, f"Texture import did not produce {asset_path}"
    texture.set_editor_properties({
        "srgb": True,
        "never_stream": True,
        "compression_settings": unreal.TextureCompressionSettings.TC_DEFAULT,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_SHARPEN0,
    })
    assert unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False)
    return texture, asset_path


def make_sprite_material(frame_id: str, texture):
    unreal.EditorAssetLibrary.make_directory(MATERIAL_DEST)
    name = f"M_P4_A0_{frame_id}"
    path = f"{MATERIAL_DEST}/{name}"
    material = unreal.load_asset(path)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, MATERIAL_DEST, unreal.Material, unreal.MaterialFactoryNew()
        )
    assert material, f"Could not create {path}"
    material.set_editor_properties({
        "shading_model": unreal.MaterialShadingModel.MSM_UNLIT,
        "blend_mode": unreal.BlendMode.BLEND_MASKED,
        "two_sided": True,
        "opacity_mask_clip_value": 0.08,
    })
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -300, 0
    )
    sample.set_editor_property("texture", texture)
    assert unreal.MaterialEditingLibrary.connect_material_property(
        sample, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    ), f"Could not connect {frame_id} RGB"
    assert unreal.MaterialEditingLibrary.connect_material_property(
        sample, "A", unreal.MaterialProperty.MP_OPACITY_MASK
    ), f"Could not connect {frame_id} alpha"
    errors = unreal.MaterialEditingLibrary.recompile_material(material)
    if errors:
        raise RuntimeError(f"A0 material compile error for {frame_id}: {errors}")
    assert unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    assert material.get_editor_property("blend_mode") == unreal.BlendMode.BLEND_MASKED
    return material, path


def tag_actor(actor, label: str):
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(ACTOR_TAG)])
    return actor


def spawn_mesh(actor_subsystem, mesh, label, location, scale, material, rotation=None):
    actor = actor_subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor,
        location,
        rotation or unreal.Rotator(0.0, 0.0, 0.0),
    )
    assert actor, f"Could not spawn {label}"
    tag_actor(actor, label)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_static_mesh(mesh)
    component.set_material(0, material)
    component.set_editor_property("cast_shadow", False)
    actor.set_actor_scale3d(scale)
    actor.set_actor_enable_collision(False)
    return actor


def prepare_map():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P4/Maps")
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        assert levels.load_level(MAP_PATH), f"Could not load A0 map: {MAP_PATH}"
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        world_settings = world.get_world_settings()
        tags = [str(tag) for tag in world_settings.get_editor_property("tags")]
        assert OWNER_TAG in tags, f"Refusing to replace an unowned map: {MAP_PATH}"
        for actor in actor_subsystem.get_all_level_actors():
            tags = [str(tag) for tag in actor.get_editor_property("tags")]
            if ACTOR_TAG in tags:
                assert actor_subsystem.destroy_actor(actor), "Could not clear prior A0-owned actor"
    else:
        assert levels.new_level(MAP_PATH), f"Could not create A0 preview map: {MAP_PATH}"
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        world_settings = world.get_world_settings()
        world_settings.set_editor_property("tags", [unreal.Name(OWNER_TAG)])

    game_mode = unreal.load_class(None, GAME_MODE_PATH)
    camera_class = unreal.load_class(None, CAMERA_PATH)
    assert game_mode, f"Missing P4 preview GameMode: {GAME_MODE_PATH}"
    assert camera_class, f"Missing P4 camera rig: {CAMERA_PATH}"
    world_settings.set_editor_property("default_game_mode", game_mode)

    ground_material = unreal.load_asset(GROUND_MATERIAL_PATH)
    shadow_material = unreal.load_asset(SHADOW_MATERIAL_PATH)
    plane = unreal.load_asset("/Engine/BasicShapes/Plane")
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    assert ground_material and shadow_material and plane and cube, "Required W1 or Engine materials are unavailable"

    spawn_mesh(
        actor_subsystem, cube, "P4A0_Ground",
        unreal.Vector(ARENA_WIDTH / 2.0, ARENA_DEPTH / 2.0, -12.5),
        unreal.Vector(ARENA_WIDTH / 100.0, ARENA_DEPTH / 100.0, 0.25),
        ground_material,
    )

    camera = actor_subsystem.spawn_actor_from_class(
        camera_class, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0)
    )
    assert camera, "Could not spawn P4 fixed camera"
    tag_actor(camera, "P4A0_FixedCamera")
    start = actor_subsystem.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(550, SPRITE_DEPTH_Y, 0), unreal.Rotator(0, 0, 0)
    )
    assert start, "Could not spawn A0 PlayerStart"
    tag_actor(start, "P4A0_PlayerStart")

    frame_details = []
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    assert manifest["status"] == "owner_selected_direction_wip"
    for frame, x in zip(manifest["frames"], SPRITE_XS):
        texture, texture_path = import_texture(frame)
        material, material_path = make_sprite_material(frame["id"], texture)
        width, height = manifest["image_spec"]["resolution"]
        foot_x = frame["foot_anchor_px"][0]
        foot_y = frame["foot_anchor_px"][1]
        anchor_from_bottom = SPRITE_HEIGHT * (height - foot_y) / height
        sprite_z = SPRITE_HEIGHT / 2.0 - anchor_from_bottom
        world_width = SPRITE_HEIGHT * width / height
        sprite_x = x - (foot_x - width / 2.0) * world_width / width
        sprite = spawn_mesh(
            actor_subsystem, plane, f"P4A0_{frame['id']}",
            unreal.Vector(sprite_x, SPRITE_DEPTH_Y, sprite_z),
            unreal.Vector(world_width / 100.0, SPRITE_HEIGHT / 100.0, 1.0),
            material, unreal.Rotator(pitch=0.0, yaw=0.0, roll=90.0),
        )
        shadow = spawn_mesh(
            actor_subsystem, plane, f"P4A0_Shadow_{frame['id']}",
            unreal.Vector(x, SPRITE_DEPTH_Y, 1.0),
            unreal.Vector(1.4, 0.48, 1.0), shadow_material,
        )
        frame_details.append({
            "id": frame["id"],
            "texture": texture_path,
            "material": material_path,
            "target_foot_world_x_uu": x,
            "world_location_uu": [sprite_x, SPRITE_DEPTH_Y, sprite_z],
            "world_height_uu": SPRITE_HEIGHT,
            "foot_anchor_px": frame["foot_anchor_px"],
            "projected_anchor_offset_x_uu": (foot_x - width / 2.0) * world_width / width,
            "projected_anchor_from_bottom_uu": anchor_from_bottom,
        })

    key = actor_subsystem.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(550, 92.5, 800), unreal.Rotator(-45, -30, 0)
    )
    tag_actor(key, "P4A0_KeyLight")
    key.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("intensity", 3.0)
    sky = actor_subsystem.spawn_actor_from_class(
        unreal.SkyLight, unreal.Vector(550, 92.5, 600), unreal.Rotator(0, 0, 0)
    )
    tag_actor(sky, "P4A0_SkyLight")

    assert levels.save_current_level(), f"Could not save A0 map: {MAP_PATH}"
    unreal.EditorAssetLibrary.save_directory("/Game/Naiwa/P4/Maps", only_if_is_dirty=False, recursive=True)
    unreal.EditorAssetLibrary.save_directory(ART_DEST, only_if_is_dirty=False, recursive=True)
    unreal.EditorAssetLibrary.save_directory(MATERIAL_DEST, only_if_is_dirty=False, recursive=True)
    owned_labels = sorted(
        actor.get_actor_label()
        for actor in actor_subsystem.get_all_level_actors()
        if ACTOR_TAG in [str(tag) for tag in actor.get_editor_property("tags")]
    )
    report.update({
        "status": "saved_pending_visible_acceptance",
        "resolution_target": [1920, 1080],
        "logical_arena_uu": [ARENA_WIDTH, ARENA_DEPTH],
        "sprite_depth_y_uu": SPRITE_DEPTH_Y,
        "frames": frame_details,
        "saved_owned_actor_labels": owned_labels,
        "material_compile": "passed",
        "limitations": [
            "Map saved through Editor Python; NullRHI generation does not verify visible rendering.",
            "A visible Standalone/PIE capture, 1080p and 720p silhouette review, and p95 frame-time sample remain open.",
            "The sample covers only a front three-quarter punch; back/side direction samples and full Alpha production approval remain open.",
        ],
    })


try:
    prepare_map()
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    raise
finally:
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("NAIWA_P4_A0_PUNCH_PREVIEW " + json.dumps(report, ensure_ascii=False))
