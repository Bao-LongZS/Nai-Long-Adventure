"""Build an isolated P4 W1 calibration map, sprite material, and ruler scene.

The script only owns /Game/Naiwa/P4/Maps/L_NaiwaP4_W1Preview and assets with
the P4 W1 names below. It refuses to repurpose an unrelated map. The generated
map is a measurement fixture, not an alpha arena or production art approval.
"""
from __future__ import annotations

import json
import math
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
MAP_PATH = "/Game/Naiwa/P4/Maps/L_NaiwaP4_W1Preview"
OWNER_TAG = "NaiwaP4W1Preview"
ACTOR_TAG = "NaiwaP4W1Generated"
ASSET_ROOT = "/Game/Naiwa/P4"
TEXTURE_DEST = f"{ASSET_ROOT}/Art/W1"
MATERIAL_DEST = f"{ASSET_ROOT}/Materials/W1"
ATLAS_SOURCE = ROOT / "UE5/SourceAssets/P4/Portraits/W1/P4_Player_W1_StaticPoseAtlas.png"
ATLAS_MANIFEST = ROOT / "UE5/SourceAssets/P4/Portraits/W1/P4_Player_W1_StaticPoseAtlas.json"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/w1_preview_build_report.json"
ATLAS_ASSET = f"{TEXTURE_DEST}/T_P4_Player_W1_StaticPoseAtlas"
SPRITE_MATERIAL_PREFIX = f"{MATERIAL_DEST}/M_P4_W1_Sprite_"
SHADOW_MATERIAL = f"{MATERIAL_DEST}/M_P4_W1_SoftShadow_RadialV6"
GAME_MODE_PATH = "/Script/NaiwaRuntime.NaiwaP4W1PreviewGameMode"
CAMERA_PATH = "/Script/NaiwaRuntime.NaiwaP4CameraRig"

report = {
    "status": "running",
    "map": MAP_PATH,
    "engine": unreal.SystemLibrary.get_engine_version(),
    "ownership_tag": OWNER_TAG,
}


def create_asset(name: str, folder: str, asset_class, factory):
    path = f"{folder}/{name}"
    existing = unreal.load_asset(path)
    return existing or unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, asset_class, factory
    )


def constant(material, asset_class, x: int, y: int):
    return unreal.MaterialEditingLibrary.create_material_expression(material, asset_class, x, y)


def connect(material, source, output_name, target, input_name, label):
    connected = unreal.MaterialEditingLibrary.connect_material_expressions(
        source, output_name, target, input_name
    )
    if not connected:
        input_names = unreal.MaterialEditingLibrary.get_material_expression_input_names(target)
        raise RuntimeError(
            f"Could not connect material pin for {label}; "
            f"requested={input_name!r}, available={list(input_names)}"
        )


def make_sprite_materials(frame_textures, manifest):
    """Build one masked material per pose from its isolated transparent cell texture."""
    result = {}
    for frame in manifest["frames"]:
        frame_id = frame["id"]
        rect = frame["atlas_rect"]
        material_path = SPRITE_MATERIAL_PREFIX + frame_id
        material = unreal.load_asset(material_path)
        if not material:
            material = create_asset(
                f"M_P4_W1_Sprite_{frame_id}", MATERIAL_DEST,
                unreal.Material, unreal.MaterialFactoryNew(),
            )
        assert material, f"Could not create sprite material: {material_path}"
        material.set_editor_properties({
            "shading_model": unreal.MaterialShadingModel.MSM_UNLIT,
            "blend_mode": unreal.BlendMode.BLEND_MASKED,
            "two_sided": True,
            "opacity_mask_clip_value": 0.08,
        })
        unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

        sample = constant(material, unreal.MaterialExpressionTextureSample, 0, 0)
        texture = frame_textures[frame_id]
        sample.set_editor_property("texture", texture)
        assert unreal.MaterialEditingLibrary.connect_material_property(
            sample, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR
        ), f"Could not connect {frame_id} emissive output"
        assert unreal.MaterialEditingLibrary.connect_material_property(
            sample, "A", unreal.MaterialProperty.MP_OPACITY_MASK
        ), f"Could not connect {frame_id} opacity mask"
        errors = unreal.MaterialEditingLibrary.recompile_material(material)
        if errors:
            raise RuntimeError(f"P4 sprite material compile errors for {frame_id}: {errors}")
        assert unreal.EditorAssetLibrary.save_asset(material_path, only_if_is_dirty=False)
        result[frame_id] = {
            "material": material_path,
            "atlas_rect": rect,
            "visible_rect": frame["visible_rect"],
            "foot_anchor": frame["foot_anchor"],
            "frame_texture": texture.get_path_name(),
            "sampling": "isolated transparent cell on the mesh's default UVs",
            "uv_scale": [1.0, 1.0],
            "uv_offset": [0.0, 0.0],
        }
    return result


def make_shadow_material():
    material_path = SHADOW_MATERIAL
    material = unreal.load_asset(material_path)
    if not material:
        material = create_asset(
            "M_P4_W1_SoftShadow_RadialV6", MATERIAL_DEST, unreal.Material, unreal.MaterialFactoryNew()
        )
    assert material, f"Could not create shadow material: {material_path}"
    material.set_editor_properties({
        "shading_model": unreal.MaterialShadingModel.MSM_UNLIT,
        "blend_mode": unreal.BlendMode.BLEND_TRANSLUCENT,
        "two_sided": True,
    })
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    color = constant(material, unreal.MaterialExpressionConstant3Vector, -400, 0)
    color.set_editor_property("constant", unreal.LinearColor(0.015, 0.022, 0.03, 1.0))
    uv = constant(material, unreal.MaterialExpressionTextureCoordinate, -1000, 280)
    center = constant(material, unreal.MaterialExpressionConstant2Vector, -1000, 500)
    center.set_editor_properties({"r": 0.5, "g": 0.5})
    delta = constant(material, unreal.MaterialExpressionSubtract, -760, 280)
    connect(material, uv, "", delta, "A", "shadow UV source")
    connect(material, center, "", delta, "B", "shadow UV center")
    diameter = constant(material, unreal.MaterialExpressionConstant2Vector, -1000, 720)
    diameter.set_editor_properties({"r": 2.0, "g": 2.0})
    normalized_delta = constant(material, unreal.MaterialExpressionMultiply, -520, 280)
    connect(material, delta, "", normalized_delta, "A", "shadow UV delta")
    connect(material, diameter, "", normalized_delta, "B", "shadow UV radius scale")
    radius = constant(material, unreal.MaterialExpressionLength, -280, 280)
    # Length exposes a single `Input` pin. Passing an empty name selects the
    # first input reliably across UE's Python reflection layer.
    assert unreal.MaterialEditingLibrary.connect_material_expressions(
        normalized_delta, "", radius, ""
    ), "Could not connect normalized UV delta into shadow radius"
    one = constant(material, unreal.MaterialExpressionConstant, -280, 520)
    one.set_editor_property("r", 1.0)
    raw_fade = constant(material, unreal.MaterialExpressionSubtract, -60, 280)
    connect(material, one, "", raw_fade, "A", "shadow fade baseline")
    connect(material, radius, "", raw_fade, "B", "shadow radius")
    zero = constant(material, unreal.MaterialExpressionConstant, -60, 720)
    zero.set_editor_property("r", 0.0)
    fade = constant(material, unreal.MaterialExpressionMax, 160, 280)
    connect(material, raw_fade, "", fade, "A", "shadow unbounded fade")
    connect(material, zero, "", fade, "B", "shadow minimum opacity")
    exponent = constant(material, unreal.MaterialExpressionConstant, -60, 520)
    exponent.set_editor_property("r", 2.0)
    soft_edge = constant(material, unreal.MaterialExpressionPower, 380, 280)
    connect(material, fade, "", soft_edge, "Base", "shadow clamped fade")
    connect(material, exponent, "", soft_edge, "Exp", "shadow falloff exponent")
    opacity_strength = constant(material, unreal.MaterialExpressionConstant, 160, 520)
    # The owner capture showed the contact shadow was difficult to read;
    # keep a modest, reversible opacity increase for the next visual review.
    opacity_strength.set_editor_property("r", 0.58)
    opacity = constant(material, unreal.MaterialExpressionMultiply, 600, 280)
    connect(material, soft_edge, "", opacity, "A", "shadow softness")
    connect(material, opacity_strength, "", opacity, "B", "shadow opacity strength")
    assert unreal.MaterialEditingLibrary.connect_material_property(
        color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    ), "Could not connect shadow emissive color"
    assert unreal.MaterialEditingLibrary.connect_material_property(
        opacity, "", unreal.MaterialProperty.MP_OPACITY
    ), "Could not connect shadow opacity"
    errors = unreal.MaterialEditingLibrary.recompile_material(material)
    if errors:
        raise RuntimeError(f"P4 soft shadow material compile errors: {errors}")
    assert unreal.EditorAssetLibrary.save_asset(material_path, only_if_is_dirty=False)
    return material


def make_color_material(name: str, rgb: tuple[float, float, float], roughness: float = 0.8):
    path = f"{MATERIAL_DEST}/{name}"
    material = unreal.load_asset(path)
    if material:
        return material
    material = create_asset(name, MATERIAL_DEST, unreal.Material, unreal.MaterialFactoryNew())
    assert material, f"Could not create color material: {path}"
    color = constant(material, unreal.MaterialExpressionConstant3Vector, -400, 0)
    color.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
    rough = constant(material, unreal.MaterialExpressionConstant, -400, 220)
    rough.set_editor_property("r", roughness)
    unreal.MaterialEditingLibrary.connect_material_property(
        color, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        rough, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    return material


def import_texture(source: Path, destination_name: str):
    asset_path = f"{TEXTURE_DEST}/{destination_name}"
    existing_texture = unreal.load_asset(asset_path)
    if existing_texture:
        return existing_texture
    assert source.is_file(), f"Missing P4 W1 texture source: {source}"
    unreal.EditorAssetLibrary.make_directory(TEXTURE_DEST)
    task = unreal.AssetImportTask()
    task.set_editor_properties({
        "filename": str(source),
        "destination_path": TEXTURE_DEST,
        "destination_name": destination_name,
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
    return texture


def import_atlas(manifest: dict):
    assert ATLAS_SOURCE.is_file(), f"Missing P4 W1 atlas: {ATLAS_SOURCE}"
    assert manifest.get("status") == "static_pose_render_probe", "Unexpected atlas manifest"
    atlas = import_texture(ATLAS_SOURCE, "T_P4_Player_W1_StaticPoseAtlas")
    frame_textures = {}
    for frame in manifest["frames"]:
        frame_source = ROOT / frame["frame_texture"]
        frame_textures[frame["id"]] = import_texture(
            frame_source, f"T_P4_Player_W1_{frame['id']}"
        )
    return atlas, frame_textures


def set_owned(actor, label: str):
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(ACTOR_TAG)])
    return actor


def spawn_mesh(actor_subsystem, cube, label, location, scale, material, rotation=None):
    actor = actor_subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor, location, rotation or unreal.Rotator(0.0, 0.0, 0.0)
    )
    assert actor, f"Could not spawn P4 W1 mesh actor: {label}"
    set_owned(actor, label)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_static_mesh(cube)
    component.set_material(0, material)
    # Character sprites are a visual stand-in; use the authored soft ellipse
    # below them instead of a hard directional-light silhouette.
    component.set_editor_property("cast_shadow", False)
    actor.set_actor_scale3d(scale)
    return actor


def create_preview_map(game_mode_class, camera_class, frame_materials, manifest):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    unreal.EditorAssetLibrary.make_directory(f"{ASSET_ROOT}/Maps")
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        assert levels.load_level(MAP_PATH), f"Could not load owned P4 map: {MAP_PATH}"
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        settings = world.get_world_settings()
        tags = [str(tag) for tag in settings.get_editor_property("tags")]
        actors = actor_subsystem.get_all_level_actors()
        if OWNER_TAG not in tags:
            # The first build can create the blank map package before the
            # ownership tag is serialized. Recover only from this builder's
            # known material argument failure recorded in its own report.
            prior_error = ""
            if REPORT_PATH.is_file():
                prior_error = json.loads(REPORT_PATH.read_text(encoding="utf-8")).get("error", "")
            assert (
                MAP_PATH in prior_error
                and (
                    "Cannot nativize 'dict' as 'Material'" in prior_error
                    or "Map exists but is not owned by this preview builder" in prior_error
                    or "Could not connect radial fade into the shadow clamp" in prior_error
                )
            ), f"Map exists but is not owned by this preview builder: {MAP_PATH}"
            settings.set_editor_property("tags", [unreal.Name(OWNER_TAG)])
        for actor in actors:
            tags = [str(tag) for tag in actor.get_editor_property("tags")]
            if ACTOR_TAG in tags:
                assert actor_subsystem.destroy_actor(actor), f"Could not replace owned actor: {actor}"
    else:
        assert levels.new_level(MAP_PATH), f"Could not create isolated P4 preview map: {MAP_PATH}"
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        settings = world.get_world_settings()
        settings.set_editor_property("tags", [unreal.Name(OWNER_TAG)])
    settings.set_editor_property("default_game_mode", game_mode_class)

    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    plane = unreal.load_asset("/Engine/BasicShapes/Plane")
    assert cube and plane, "Engine basic cube/plane meshes are required for W1 graybox"
    ground = make_color_material("M_P4_W1_Ground", (0.12, 0.17, 0.20))
    ruler_x = make_color_material("M_P4_W1_RulerX", (0.98, 0.68, 0.17))
    ruler_y = make_color_material("M_P4_W1_RulerY", (0.27, 0.83, 0.78))
    endpoint = make_color_material("M_P4_W1_Endpoint", (0.92, 0.92, 0.82))
    shadow = make_shadow_material()

    # 1100 x 185 uu logical arena, floor top at Z=0.
    spawn_mesh(actor_subsystem, cube, "P4W1_Ground", unreal.Vector(550, 92.5, -12.5),
               unreal.Vector(11, 1.85, 0.25), ground)
    # Keep the horizontal baseline at the arena's center depth. A front-edge
    # ruler is closer to the perspective camera and does not match the frozen
    # 1510 px / 1000 uu calibration target.
    ruler_x_y = 92.5
    sprite_depth_y = 125.0
    spawn_mesh(actor_subsystem, cube, "P4W1_Ruler_X_1000uu", unreal.Vector(550, ruler_x_y, 1),
               unreal.Vector(10, 0.035, 0.02), ruler_x)
    spawn_mesh(actor_subsystem, cube, "P4W1_Ruler_Y_185uu", unreal.Vector(1087, 92.5, 1),
               unreal.Vector(0.035, 1.85, 0.02), ruler_y)
    for label, point in (
        ("P4W1_X_Start", unreal.Vector(50, ruler_x_y, 1.2)),
        ("P4W1_X_End", unreal.Vector(1050, ruler_x_y, 1.2)),
        ("P4W1_Y_Start", unreal.Vector(1087, 0, 1.2)),
        ("P4W1_Y_End", unreal.Vector(1087, 185, 1.2)),
    ):
        spawn_mesh(actor_subsystem, cube, label, point, unreal.Vector(0.12, 0.12, 0.04), endpoint)

    camera = actor_subsystem.spawn_actor_from_class(
        camera_class, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0)
    )
    assert camera, "Could not place P4 fixed camera rig"
    set_owned(camera, "P4W1_FixedCamera")
    player_start = actor_subsystem.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(550, 92.5, 0), unreal.Rotator(0, 0, 0)
    )
    assert player_start, "Could not place P4 W1 PlayerStart"
    set_owned(player_start, "P4W1_PlayerStart")

    for x, frame_id in ((280.0, "idle_neutral"), (820.0, "hit_reaction")):
        width, height = manifest["cell_size"]
        world_height = 180.0
        world_width = world_height * width / height
        anchor_y = manifest["frames"][[frame["id"] for frame in manifest["frames"]].index(frame_id)]["foot_anchor"][1]
        anchor_from_bottom = world_height * (height - anchor_y) / height
        frame_material = unreal.load_asset(frame_materials[frame_id]["material"])
        assert frame_material, f"Could not load P4 W1 cell material for {frame_id}"
        sprite = spawn_mesh(
            actor_subsystem, plane, f"P4W1_Player_{frame_id}",
            unreal.Vector(x, sprite_depth_y, world_height / 2.0 - anchor_from_bottom),
            unreal.Vector(world_width / 100.0, world_height / 100.0, 1.0),
            frame_material, unreal.Rotator(pitch=0.0, yaw=0.0, roll=90.0),
        )
        report.setdefault("sprite_plane_rotations", {})[frame_id] = {
            "pitch": sprite.get_actor_rotation().pitch,
            "yaw": sprite.get_actor_rotation().yaw,
            "roll": sprite.get_actor_rotation().roll,
        }
        sprite.set_actor_enable_collision(False)
        spawn_mesh(
            actor_subsystem, plane, f"P4W1_Shadow_{frame_id}",
            unreal.Vector(x, sprite_depth_y, 1.0), unreal.Vector(1.4, 0.48, 1.0),
            shadow, unreal.Rotator(0.0, 0.0, 0.0),
        ).set_actor_enable_collision(False)

    light = actor_subsystem.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(550, 92.5, 800), unreal.Rotator(-45, -30, 0)
    )
    set_owned(light, "P4W1_KeyLight")
    light.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("intensity", 3.0)
    sky = actor_subsystem.spawn_actor_from_class(
        unreal.SkyLight, unreal.Vector(550, 92.5, 600), unreal.Rotator(0, 0, 0)
    )
    set_owned(sky, "P4W1_SkyLight")

    resolution = [1920, 1080]
    fov = 37.0
    pitch = 57.0
    distance = 1900.0
    focal_px = resolution[0] / (2.0 * math.tan(math.radians(fov) / 2.0))
    center_y = 92.5
    camera_y = center_y - distance * math.cos(math.radians(pitch))
    camera_z = distance * math.sin(math.radians(pitch))

    def project_ground_y(y: float) -> float:
        rel_y = y - camera_y
        rel_z = -camera_z
        view_depth = rel_y * math.cos(math.radians(pitch)) - rel_z * math.sin(math.radians(pitch))
        view_up = rel_y * math.sin(math.radians(pitch)) + rel_z * math.cos(math.radians(pitch))
        return focal_px * view_up / view_depth

    projected_y_span = abs(project_ground_y(185.0) - project_ground_y(0.0))
    ruler_x_z = 1.0
    ruler_x_rel_y = ruler_x_y - camera_y
    ruler_x_rel_z = ruler_x_z - camera_z
    ruler_x_view_depth = (
        ruler_x_rel_y * math.cos(math.radians(pitch))
        - ruler_x_rel_z * math.sin(math.radians(pitch))
    )
    x_scale_px_per_uu = focal_px / ruler_x_view_depth
    arena_x_screen_bounds = sorted((
        resolution[0] / 2.0 + (550.0 - 0.0) * x_scale_px_per_uu,
        resolution[0] / 2.0 + (550.0 - 1100.0) * x_scale_px_per_uu,
    ))
    report.update({
        "status": "saved_pending_w1_acceptance",
        "map": MAP_PATH,
        "camera": {"horizontal_fov_degrees": fov, "pitch_degrees": pitch, "distance_uu": distance},
        "resolution": resolution,
        "rulers": {
            "x_1000uu_expected_px": 1000.0 * x_scale_px_per_uu,
            "x_ruler_depth_y_uu": ruler_x_y,
            "x_ruler_view_depth_uu": ruler_x_view_depth,
            "depth_y_185uu_expected_px": projected_y_span,
            "same_depth_left_right_foot_delta_px_expected": 0.0,
            "arena_x_screen_bounds_expected_px": arena_x_screen_bounds,
            "arena_horizontal_screen_margin_expected_px": min(
                arena_x_screen_bounds[0], resolution[0] - arena_x_screen_bounds[1]
            ),
            "logical_arena_uu": [1100.0, 185.0],
        },
        "sprite_samples": {
            key: {"world_height_uu": 180.0, "world_x": x, "depth_y": sprite_depth_y, "cell": frame_materials[key]}
            for x, key in ((280.0, "idle_neutral"), (820.0, "hit_reaction"))
        },
        "limitations": [
            "Two static poses only; this is not animation approval.",
            "A 1920x1080 Standalone capture exists; exact ruler and foot-anchor pixel checks plus frame-time sampling remain pending.",
            "The shadow is a soft translucent ellipse approximation; final depth sorting and art readability need visual review.",
        ],
    })
    assert levels.save_current_level(), f"Could not save P4 W1 preview map: {MAP_PATH}"
    unreal.EditorAssetLibrary.save_directory(ASSET_ROOT, only_if_is_dirty=False, recursive=True)
    for frame_id, metadata in frame_materials.items():
        material = unreal.load_asset(metadata["material"])
        assert material, f"Could not reload saved sprite material: {metadata['material']}"
        assert material.get_editor_property("blend_mode") == unreal.BlendMode.BLEND_MASKED
        metadata["saved_material_readback"] = "passed"
    generated_labels = sorted(
        actor.get_actor_label()
        for actor in actor_subsystem.get_all_level_actors()
        if ACTOR_TAG in [str(tag) for tag in actor.get_editor_property("tags")]
    )
    expected_labels = sorted([
        "P4W1_Ground", "P4W1_Ruler_X_1000uu", "P4W1_Ruler_Y_185uu",
        "P4W1_X_Start", "P4W1_X_End", "P4W1_Y_Start", "P4W1_Y_End",
        "P4W1_FixedCamera", "P4W1_PlayerStart", "P4W1_Player_idle_neutral",
        "P4W1_Shadow_idle_neutral", "P4W1_Player_hit_reaction",
        "P4W1_Shadow_hit_reaction", "P4W1_KeyLight", "P4W1_SkyLight",
    ])
    assert generated_labels == expected_labels, (
        f"Unexpected saved P4 W1 actor set: {generated_labels}"
    )
    report["saved_owned_actor_labels"] = generated_labels
    report["sprite_material_readback"] = "passed"
    report["shadow_material_compile"] = "passed"
    return report


try:
    manifest = json.loads(ATLAS_MANIFEST.read_text(encoding="utf-8"))
    _, frame_textures = import_atlas(manifest)
    unreal.EditorAssetLibrary.make_directory(MATERIAL_DEST)
    cell_materials = make_sprite_materials(frame_textures, manifest)
    game_mode = unreal.load_class(None, GAME_MODE_PATH)
    camera_class = unreal.load_class(None, CAMERA_PATH)
    assert game_mode, f"P4 W1 preview GameMode is unavailable: {GAME_MODE_PATH}"
    assert camera_class, f"P4 camera rig is unavailable: {CAMERA_PATH}"
    create_preview_map(game_mode, camera_class, cell_materials, manifest)
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    raise
finally:
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("NAIWA_P4_W1_PREVIEW " + json.dumps(report, ensure_ascii=False))
