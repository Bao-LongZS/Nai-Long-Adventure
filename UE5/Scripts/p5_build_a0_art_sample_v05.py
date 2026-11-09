"""Build an isolated P5 A0 art sample map and import WIP art for Editor review."""
import json
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
MAP_PATH = "/Game/Naiwa/P5/Maps/L_NaiwaP5_A0ArtSample_v05"
OWNER_TAG = "NaiwaP5A0ArtSampleV05"
ACTOR_TAG = "NaiwaP5A0ArtSampleV05Generated"
GAME_MODE_PATH = "/Script/NaiwaRuntime.NaiwaP5M2PrototypeGameMode"
SOURCE_ROOT = ROOT / "UE5/SourceAssets/P5"
ART_ROOT = "/Game/Naiwa/P5/ArtSample_v05"
TEXTURE_DEST = "/Game/Naiwa/P5/ArtSample/Textures"
MATERIAL_DEST = ART_ROOT + "/Materials"
REPORT_PATH = ROOT / "UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_EditorSample_v05_build_report.json"
MATERIAL_REPAIR_REPORT_PATH = ROOT / "UE5/Records/p5_m2/P5_A0_BackgroundPlacement_materials_report_2026-10-29.json"

report = {
    "status": "running",
    "map": MAP_PATH,
    "engine": unreal.SystemLibrary.get_engine_version(),
    "owner_tag": OWNER_TAG,
    "art_scope": "WIP sample only; not runtime acceptance",
}


def import_texture(source: Path, asset_name: str):
    assert source.is_file(), f"Missing P5 A0 WIP texture: {source}"
    asset_path = f"{TEXTURE_DEST}/{asset_name}"
    is_new = not unreal.EditorAssetLibrary.does_asset_exist(asset_path)
    task = unreal.AssetImportTask()
    task.set_editor_properties({
        "filename": str(source),
        "destination_path": TEXTURE_DEST,
        "destination_name": asset_name,
        # These are WIP source-controlled sample textures. Reimport so a rebuilt
        # map cannot silently keep stale pixels when the PNG changed in place.
        "replace_existing": not is_new,
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
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_SHARPEN2,
    })
    assert texture.get_editor_property("srgb") is True, f"sRGB readback failed: {asset_path}"
    assert texture.get_editor_property("never_stream") is True, f"NeverStream readback failed: {asset_path}"
    assert unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False), (
        f"Could not save P5 texture: {asset_path}"
    )
    return texture, asset_path


def make_texture_material(name: str, texture, alpha_mode: str):
    path = f"{MATERIAL_DEST}/{name}"
    material = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, MATERIAL_DEST, unreal.Material, unreal.MaterialFactoryNew()
        )
    assert material, f"Could not create P5 A0 material: {path}"
    assert alpha_mode in ("masked", "translucent", "opaque"), f"Unknown alpha mode: {alpha_mode}"
    blend_mode = {
        "masked": unreal.BlendMode.BLEND_MASKED,
        "translucent": unreal.BlendMode.BLEND_TRANSLUCENT,
        "opaque": unreal.BlendMode.BLEND_OPAQUE,
    }[alpha_mode]
    material.set_editor_properties({
        "shading_model": unreal.MaterialShadingModel.MSM_UNLIT,
        "blend_mode": blend_mode,
        "two_sided": True,
    })
    if alpha_mode == "masked":
        material.set_editor_property("opacity_mask_clip_value", 0.01)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, 0, 0
    )
    sample.set_editor_property("parameter_name", "A0Texture")
    sample.set_editor_property("texture", texture)
    uv = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -1000, 240
    )
    uv_scale = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -1000, 440
    )
    uv_scale.set_editor_properties({
        "parameter_name": "A0UVScale",
        "default_value": unreal.LinearColor(1.0, 1.0, 0.0, 0.0),
    })
    uv_offset = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -1000, 640
    )
    uv_offset.set_editor_properties({
        "parameter_name": "A0UVOffset",
        "default_value": unreal.LinearColor(0.0, 0.0, 0.0, 0.0),
    })
    # VectorParameter emits float3, while UV inputs require float2. Explicitly
    # mask RG before using the parameters in UV arithmetic; otherwise SM6 falls
    # back to UE's default material for every A0 texture.
    uv_scale_rg = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionComponentMask, -760, 460
    )
    uv_scale_rg.set_editor_properties({"r": True, "g": True, "b": False, "a": False})
    uv_offset_rg = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionComponentMask, -460, 640
    )
    uv_offset_rg.set_editor_properties({"r": True, "g": True, "b": False, "a": False})
    uv_multiply = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -740, 300
    )
    uv_add = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, -460, 300
    )
    for source, output, target, input_name in (
        (uv, "", uv_multiply, "A"),
        (uv_scale, "", uv_scale_rg, ""),
        (uv_scale_rg, "", uv_multiply, "B"),
        (uv_multiply, "", uv_add, "A"),
        (uv_offset, "", uv_offset_rg, ""),
        (uv_offset_rg, "", uv_add, "B"),
        (uv_add, "", sample, "UVs"),
    ):
        if not unreal.MaterialEditingLibrary.connect_material_expressions(
            source, output, target, input_name
        ):
            available = unreal.MaterialEditingLibrary.get_material_expression_input_names(target)
            raise RuntimeError(
                f"Could not connect A0 UV input for {name}; "
                f"requested={input_name!r}, available={list(available)}"
            )
    assert unreal.MaterialEditingLibrary.connect_material_property(
        sample, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    ), f"Could not connect RGB for {name}"
    if alpha_mode == "masked":
        assert unreal.MaterialEditingLibrary.connect_material_property(
            sample, "A", unreal.MaterialProperty.MP_OPACITY_MASK
        ), f"Could not connect alpha mask for {name}"
    elif alpha_mode == "translucent":
        assert unreal.MaterialEditingLibrary.connect_material_property(
            sample, "A", unreal.MaterialProperty.MP_OPACITY
        ), f"Could not connect alpha for {name}"
    errors = unreal.MaterialEditingLibrary.recompile_material(material)
    if errors:
        raise RuntimeError(f"P5 A0 material compile errors for {name}: {errors}")
    assert unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False), (
        f"Could not save P5 A0 material: {path}"
    )
    assert material.get_editor_property("blend_mode") == blend_mode, (
        f"P5 A0 material blend mode did not persist for {name}"
    )
    return material, path


def make_solid_sky_material():
    """Create an unlit solid sky so empty alpha areas never fall through to black."""
    name = "M_P5_A0_Sky"
    path = f"{MATERIAL_DEST}/{name}"
    material = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, MATERIAL_DEST, unreal.Material, unreal.MaterialFactoryNew()
        )
    assert material, f"Could not create P5 A0 sky material: {path}"
    material.set_editor_properties({
        "shading_model": unreal.MaterialShadingModel.MSM_UNLIT,
        "blend_mode": unreal.BlendMode.BLEND_OPAQUE,
        "two_sided": True,
    })
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -360, 0
    )
    color.set_editor_properties({
        "parameter_name": "A0SkyColor",
        "default_value": unreal.LinearColor(0.22, 0.37, 0.52, 1.0),
    })
    assert unreal.MaterialEditingLibrary.connect_material_property(
        color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    ), "Could not connect the unlit A0 sky color"
    errors = unreal.MaterialEditingLibrary.recompile_material(material)
    if errors:
        raise RuntimeError(f"P5 A0 sky material compile errors: {errors}")
    assert unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False), (
        f"Could not save P5 A0 sky material: {path}"
    )
    return material, path


def import_and_material(source_rel: str, asset_name: str, material_name: str, alpha_mode="masked"):
    texture, texture_path = import_texture(SOURCE_ROOT / source_rel, asset_name)
    material, material_path = make_texture_material(material_name, texture, alpha_mode)
    return {"texture": texture, "texture_path": texture_path,
            "material": material, "material_path": material_path, "alpha_mode": alpha_mode}


def tag_actor(actor, label: str):
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(ACTOR_TAG)])
    actor.set_actor_enable_collision(False)
    return actor


def spawn_plane(actor_subsystem, plane, label, texture_material, location, size_cm):
    actor = actor_subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(*location), unreal.Rotator(roll=90.0)
    )
    assert actor, f"Could not spawn P5 A0 plane: {label}"
    tag_actor(actor, label)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_static_mesh(plane)
    component.set_material(0, texture_material)
    component.set_editor_property("cast_shadow", False)
    actor.set_actor_scale3d(unreal.Vector(size_cm[0] / 100.0, size_cm[1] / 100.0, 1.0))
    return actor


def spawn_cube(actor_subsystem, cube, label, material, location, scale, rotation=None):
    actor = actor_subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(*location),
        rotation or unreal.Rotator(),
    )
    assert actor, f"Could not spawn P5 A0 mesh: {label}"
    tag_actor(actor, label)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_static_mesh(cube)
    component.set_material(0, material)
    component.set_editor_property("cast_shadow", False)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


def prepare_map(game_mode):
    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P5/Maps")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        assert levels.load_level(MAP_PATH), f"Could not load A0 sample map: {MAP_PATH}"
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        settings = world.get_world_settings()
        tags = [str(tag) for tag in settings.get_editor_property("tags")]
        assert OWNER_TAG in tags, f"Refusing to replace an unowned map: {MAP_PATH}"
        current = settings.get_editor_property("default_game_mode")
        assert current and current.get_path_name() == game_mode.get_path_name(), (
            "A0 map has an unexpected GameMode; refusing to change it"
        )
        for actor in actors.get_all_level_actors():
            if ACTOR_TAG in [str(tag) for tag in actor.get_editor_property("tags")]:
                assert actors.destroy_actor(actor), f"Could not clear prior sample actor: {actor}"
        status = "owned_sample_map_rebuilt"
    else:
        assert levels.new_level(MAP_PATH), f"Could not create A0 sample map: {MAP_PATH}"
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        settings = world.get_world_settings()
        settings.set_editor_property("tags", [unreal.Name(OWNER_TAG)])
        settings.set_editor_property("default_game_mode", game_mode)
        status = "new_isolated_sample_map"
    return actors, levels, status


def build_art_assets():
    unreal.EditorAssetLibrary.make_directory(TEXTURE_DEST)
    unreal.EditorAssetLibrary.make_directory(MATERIAL_DEST)
    assets = {
        "far": import_and_material("Backgrounds/Exports/BG_Mountain_Far_v03_WIP.png",
                                   "T_P5_A0_MountainFar_v03", "M_P5_A0_MountainFar", "translucent"),
        "mid": import_and_material("Backgrounds/Exports/BG_Forest_Mid_v03_WIP.png",
                                   "T_P5_A0_ForestMid_v03", "M_P5_A0_ForestMid", "translucent"),
        "near": import_and_material("Backgrounds/Exports/BG_Ridge_Near_v04_WIP.png",
                                     "T_P5_A0_RidgeNear_v04", "M_P5_A0_RidgeNear", "translucent"),
        "snow": import_and_material("Materials/Exports/T_Snow_Ground_v01_WIP.png",
                                     "T_P5_A0_SnowGround_v01", "M_P5_A0_SnowGround", "opaque"),
        "tree": import_and_material("Environment/Exports/O_Tree_A_v01_WIP.png",
                                     "T_P5_A0_TreeA_v01", "M_P5_A0_TreeA"),
        "rock": import_and_material("Environment/Exports/O_Rock_A_v01_WIP.png",
                                     "T_P5_A0_RockA_v01", "M_P5_A0_RockA"),
        "avalanche": import_and_material("FX/Exports/FX_Avalanche_Cloud_v01_WIP.png",
                                          "T_P5_A0_AvalancheCloud_v01", "M_P5_A0_AvalancheCloud"),
        "avalanche_rim": import_and_material("FX/Exports/FX_Avalanche_Rim_v01_WIP.png",
                                              "T_P5_A0_AvalancheRim_v01", "M_P5_A0_AvalancheRim"),
        "mist": import_and_material("FX/Exports/FX_Snow_Mist_v01_WIP.png",
                                    "T_P5_A0_SnowMist_v01", "M_P5_A0_SnowMist", "translucent"),
        "shadow": import_and_material("FX/Exports/FX_Shadow_Soft_v01_WIP.png",
                                      "T_P5_A0_SoftShadow_v01", "M_P5_A0_SoftShadow", "translucent"),
        "mount": import_and_material("Environment/Exports/C_Kangaroo_Ride_v04_WIP.png",
                                      "T_P5_A0_KangarooRide_v04", "M_P5_A0_KangarooRideAtlas"),
    }
    hero_atlas_path = SOURCE_ROOT / "Characters/Exports/C_Naiwa_Ski_v01_WIP.png"
    hero_texture, hero_texture_path = import_texture(hero_atlas_path, "T_P5_A0_NaiwaSkiAtlas_v01")
    hero_material, hero_material_path = make_texture_material(
        "M_P5_A0_NaiwaSkiAtlas", hero_texture, "masked"
    )
    assets["hero"] = {"texture": hero_texture, "texture_path": hero_texture_path,
                      "material": hero_material, "material_path": hero_material_path,
                      "alpha_mode": "masked"}
    sky_material, sky_material_path = make_solid_sky_material()
    return assets, sky_material, sky_material_path


def rebuild_materials_only():
    """Recreate and compile v05 materials without loading or modifying any map."""
    assets, sky_material, sky_material_path = build_art_assets()
    for key in ("far", "mid", "near"):
        assert assets[key]["material"].get_editor_property("blend_mode") == unreal.BlendMode.BLEND_TRANSLUCENT, (
            f"Landscape alpha must remain translucent for {key}: {assets[key]['material_path']}"
        )
    assert unreal.EditorAssetLibrary.save_directory(MATERIAL_DEST, only_if_is_dirty=False, recursive=True), (
        f"Could not save A0 v05 materials: {MATERIAL_DEST}"
    )
    materials = {key: value["material_path"] for key, value in assets.items()}
    materials["sky"] = sky_material_path
    result = {
        "status": "materials_rebuilt_without_loading_map",
        "engine": unreal.SystemLibrary.get_engine_version(),
        "materials": materials,
        "texture_count": len(assets),
        "texture_mip_gen_settings": "TMGS_SHARPEN2",
        "uv_parameter_dimensions": "A0UVScale/A0UVOffset RG component masks produce float2 UV values",
        "background_blend_modes": {
            key: str(assets[key]["material"].get_editor_property("blend_mode"))
            for key in ("far", "mid", "near")
        },
    }
    MATERIAL_REPAIR_REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    MATERIAL_REPAIR_REPORT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("NAIWA_P5_A0_V05_MATERIALS_REBUILT " + json.dumps(result, ensure_ascii=False))


def main():
    assets, sky_material, sky_material_path = build_art_assets()
    game_mode = unreal.load_class(None, GAME_MODE_PATH)
    assert game_mode, f"Could not load P5 M2 GameMode: {GAME_MODE_PATH}"
    actors, levels, map_status = prepare_map(game_mode)
    plane = unreal.load_asset("/Engine/BasicShapes/Plane")
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    assert plane and cube, "Required Engine plane/cube meshes are missing"

    actor_labels = []

    start_tag = "NaiwaP5A0ArtSamplePlayerStart"
    start = actors.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(0.0, 0.0, 80.0), unreal.Rotator()
    )
    assert start, "Could not spawn P5 A0 player start"
    start.set_editor_property("tags", [unreal.Name(ACTOR_TAG), unreal.Name(start_tag)])
    start.set_actor_label("P5A0_PlayerStart")
    actor_labels.append("P5A0_PlayerStart")

    key_light = actors.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(550.0, 92.5, 800.0),
        unreal.Rotator(pitch=-45.0, yaw=-30.0)
    )
    assert key_light, "Could not spawn P5 A0 directional light"
    tag_actor(key_light, "P5A0_KeyLight")
    key_light.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("intensity", 3.0)
    actor_labels.append("P5A0_KeyLight")

    assert levels.save_current_level(), f"Could not save A0 art sample: {MAP_PATH}"
    unreal.EditorAssetLibrary.save_directory(MATERIAL_DEST, only_if_is_dirty=False, recursive=True)
    report.update({
        "status": "saved_pending_visible_editor_review",
        "map_status": map_status,
        "game_mode": GAME_MODE_PATH,
        "sample_route": "M2 prototype 1800 m route; runtime route-following visuals and HUD; isolated A0 art sample",
        "texture_count": len(assets),
        "background_components_runtime": 9,
        "background_layer_count": 3,
        "background_layer_source_dimensions": {
            "far": [2048, 512],
            "mid": [2048, 512],
            "near": [2048, 768],
        },
        "background_layer_canvas_dimensions": {
            "far": [2048, 1536],
            "mid": [2048, 1536],
            "near": [2048, 1536],
        },
        "background_layer_world_sizes_cm": {
            "far": [3600, 2700],
            "mid": [3600, 2700],
            "near": [3600, 2700],
        },
        "background_art_world_sizes_cm": {
            "far": [3600, 900],
            "mid": [3600, 900],
            "near": [3600, 1350],
        },
        "background_layer_z_anchors_cm": [635, 365, 440],
        "background_layer_vertical_padding_px": [512, 512, 384],
        "background_parallax": [0.12, 0.30, 0.65],
        "snow_backdrop": {
            "depth_y_cm": -1500,
            "top_offset_from_route_cm": -650,
            "depth_cm": 5000,
            "geometry": "procedural route-profile strip sampled at 1 m; continuous through local slope changes",
            "depth_order": "behind scenic art; in front of sky",
            "top_edge_rule": "follows local route height instead of using a camera-relative horizontal edge",
            "texture_mapping": "one vertical tile; 48 m horizontal repeat",
        },
        "snow_bank": {
            "depth_cm": 2600,
            "texture_mapping": "one vertical tile; 24 m horizontal repeat",
            "route_surface_repeat_cm": 450,
        },
        "cliff_recess": {
            "top_below_lower_lip_cm": 120,
            "depth_cm": 2200,
            "jump_corridor": "open above both route lips",
        },
        "texture_paths": {key: value["texture_path"] for key, value in assets.items()},
        "material_paths": {key: value["material_path"] for key, value in assets.items()},
        "texture_import_settings_readback": {
            key: {
                "srgb": bool(value["texture"].get_editor_property("srgb")),
                "never_stream": bool(value["texture"].get_editor_property("never_stream")),
                "compression_settings": str(value["texture"].get_editor_property("compression_settings")),
                "mip_gen_settings": str(value["texture"].get_editor_property("mip_gen_settings")),
                "alpha_source": "preserved from PNG",
            }
            for key, value in assets.items()
        },
        "material_blend_modes": {
            key: {
                "shading_model": str(value["material"].get_editor_property("shading_model")),
                "blend_mode": str(value["material"].get_editor_property("blend_mode")),
                "two_sided": bool(value["material"].get_editor_property("two_sided")),
            }
            for key, value in assets.items()
        },
        "sky_material_path": sky_material_path,
        "sky_material_blend_mode": str(sky_material.get_editor_property("blend_mode")),
        "wip_visual_connections": {
            "route_background": "three scrolling layers; original art pixel scale is preserved inside transparent vertical bleed canvases",
            "snow_surface": "route-following 0.8 m textured top at 4.5 m repeat; broad side bank uses one stretched vertical tile and 24 m horizontal repeat",
            "snow_backdrop": "single procedural profile following every route segment 6.5 m below the local snow line; no camera-height horizontal fill plane",
            "hero": "owner-approved A4 atlas; static frame rectangles, provisional feet anchor",
            "kangaroo": "A5 approved-base v04 WIP atlas; idle cell only, shown as a route prop",
            "coin": "flat coin silhouette for side-view readability",
            "hud": "compact two-corner overlay; detailed runtime diagnostics remain in logs",
        },
        "generated_actor_labels": sorted(actor_labels),
        "sample_notes": [
            "All imported PNGs remain WIP and live under /Game/Naiwa/P5/ArtSample/Textures; v05 materials are isolated under ArtSample_v05.",
            "A4 atlas uses the owner-approved six-pose source; frame bounds come from the approved manifest and anchors remain provisional.",
            "A5 mount uses the owner-approved base atlas WIP for its idle frame only; it is a visual sample and has not passed runtime acceptance.",
            "A0 background art preserves source pixel scale (far/mid 2048x512; near 2048x768); snow backdrop follows route height behind scenic art so steep slopes cannot expose a horizontal fill edge.",
            "The foreground snow bank maps a single texture vertically and uses a broad horizontal repeat to avoid repeated strata across the deep cutaway face.",
            "Cliff recess begins 1.2 m below the lower lip and extends 22 m; ledge shoulders stay narrow and the jump corridor above both route lips remains open.",
            "The sky fill uses a dedicated unlit material so transparent asset areas do not depend on scene lighting.",
            "Only three reusable tiles per background layer are created at runtime; parallax factors are 0.12 / 0.30 / 0.65.",
            "Sky fill, route-aligned snow, live pawn art, route event sprites, avalanche layers and shadow are runtime-owned by the A0-only presentation path.",
            "Landscape layers and soft mist/shadow use unlit translucent materials; character and obstacle cutouts use unlit masked materials. Runtime appearance still needs owner review.",
            "Side-view planes use explicit roll=90 degrees (Python Rotator keyword arguments avoid the C++/Python positional-order mismatch).",
            "No 1080p/720p, texture-edge, frame-time, or gameplay acceptance is claimed by this build report; the owner opens and reviews the map.",
        ],
    })
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("NAIWA_P5_A0_ART_SAMPLE_OK " + json.dumps(report, ensure_ascii=False))


try:
    command_line = unreal.SystemLibrary.get_command_line()
    if "-P5MaterialsOnly" in command_line:
        rebuild_materials_only()
    elif "-P5BuildMap" in command_line:
        main()
    else:
        raise RuntimeError(
            "Select an explicit mode: -P5MaterialsOnly or -P5BuildMap. "
            "Refusing to load or modify any map by default."
        )
except Exception:
    error_text = traceback.format_exc()
    materials_only = "-P5MaterialsOnly" in unreal.SystemLibrary.get_command_line()
    error_path = MATERIAL_REPAIR_REPORT_PATH if materials_only else REPORT_PATH
    failure_report = {
        "status": "failed",
        "mode": "materials_only" if materials_only else "build_map",
        "error": error_text,
    }
    error_path.parent.mkdir(parents=True, exist_ok=True)
    error_path.write_text(json.dumps(failure_report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log_error(error_text)
    raise
