"""Build an isolated P5 A0 art sample map and import WIP art for Editor review."""
import json
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
MAP_PATH = "/Game/Naiwa/P5/Maps/L_NaiwaP5_A0ArtSample_v04"
OWNER_TAG = "NaiwaP5A0ArtSampleV04"
ACTOR_TAG = "NaiwaP5A0ArtSampleV04Generated"
GAME_MODE_PATH = "/Script/NaiwaRuntime.NaiwaP5M2PrototypeGameMode"
SOURCE_ROOT = ROOT / "UE5/SourceAssets/P5"
ART_ROOT = "/Game/Naiwa/P5/ArtSample_v04"
TEXTURE_DEST = "/Game/Naiwa/P5/ArtSample/Textures"
MATERIAL_DEST = ART_ROOT + "/Materials"
REPORT_PATH = ROOT / "UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A0_EditorSample_v04_build_report.json"

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
    if is_new:
        task = unreal.AssetImportTask()
        task.set_editor_properties({
            "filename": str(source),
            "destination_path": TEXTURE_DEST,
            "destination_name": asset_name,
            "replace_existing": False,
            "automated": True,
            "save": True,
        })
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(asset_path)
    assert texture, f"Texture import did not produce {asset_path}"
    if is_new:
        texture.set_editor_properties({
            "srgb": True,
            "never_stream": True,
            "compression_settings": unreal.TextureCompressionSettings.TC_DEFAULT,
            "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_SHARPEN0,
        })
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
        material, unreal.MaterialExpressionTextureSample, -320, 0
    )
    sample.set_editor_property("texture", texture)
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


def import_and_material(source_rel: str, asset_name: str, material_name: str, alpha_mode="masked"):
    texture, texture_path = import_texture(SOURCE_ROOT / source_rel, asset_name)
    material, material_path = make_texture_material(material_name, texture, alpha_mode)
    return {"texture": texture, "texture_path": texture_path,
            "material": material, "material_path": material_path}


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


def main():
    unreal.EditorAssetLibrary.make_directory(TEXTURE_DEST)
    unreal.EditorAssetLibrary.make_directory(MATERIAL_DEST)
    assets = {
        "far": import_and_material("Backgrounds/Exports/BG_Mountain_Far_v01_WIP.png",
                                   "T_P5_A0_MountainFar_v01", "M_P5_A0_MountainFar"),
        "mid": import_and_material("Backgrounds/Exports/BG_Forest_Mid_v01_WIP.png",
                                   "T_P5_A0_ForestMid_v01", "M_P5_A0_ForestMid"),
        "near": import_and_material("Backgrounds/Exports/BG_Ridge_Near_v03_WIP.png",
                                     "T_P5_A0_RidgeNear_v03", "M_P5_A0_RidgeNear"),
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
    }
    glide_path = ROOT / "UE5/Records/p5_p6_plan/evidence/P5_A0/C_Naiwa_Ski_Glide_Cell_v01_WIP.png"
    hero_texture, hero_texture_path = import_texture(glide_path, "T_P5_A0_NaiwaGlide_v01")
    hero_material, hero_material_path = make_texture_material(
        "M_P5_A0_NaiwaGlide", hero_texture, "masked"
    )
    assets["hero"] = {"texture": hero_texture, "texture_path": hero_texture_path,
                      "material": hero_material, "material_path": hero_material_path}
    game_mode = unreal.load_class(None, GAME_MODE_PATH)
    assert game_mode, f"Could not load P5 M2 GameMode: {GAME_MODE_PATH}"
    actors, levels, map_status = prepare_map(game_mode)
    plane = unreal.load_asset("/Engine/BasicShapes/Plane")
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    assert plane and cube, "Required Engine plane/cube meshes are missing"

    # M2 camera follows the pawn along X for 1800 m. The previous build placed
    # only one 30 m strip at X=0, so normal play quickly left all WIP scenery
    # behind the camera. Tile each accepted-size strip across the fixture route.
    route_length_cm = 180000.0
    tile_width_cm = 3000.0
    tile_count = int(route_length_cm / tile_width_cm) + 1
    layers = [
        # The runtime camera sits at pawn Y + 2500 and looks toward -Y. Keep
        # scenery inside its side-view depth range, ordered far-to-near, and
        # place gameplay art closer to camera than the graybox route geometry.
        ("P5A0_FarMountain", "far", 400.0, 1025.0, 750.0),
        ("P5A0_MidForest", "mid", 600.0, 700.0, 750.0),
        ("P5A0_NearRidge", "near", 800.0, 300.0, 1125.0),
    ]
    actor_labels = []
    for base_label, key, y, z, height_cm in layers:
        for tile_index in range(tile_count):
            x = tile_index * tile_width_cm
            label = f"{base_label}_{tile_index:02d}"
            spawn_plane(actors, plane, label, assets[key]["material"],
                        (x, y, z), (tile_width_cm, height_cm))
            actor_labels.append(label)

    # Sample a short strip of the first M2 slope using the same route tangent.
    spawn_cube(
        actors, cube, "P5A0_SnowSurfaceSample", assets["snow"]["material"],
        (1150.0, 1250.0, -202.0), (23.0, 1.0, 0.2), unreal.Rotator(pitch=-9.93)
    )
    actor_labels.append("P5A0_SnowSurfaceSample")

    # The player starts at x=0; this WIP sprite overlays only the proxy pose at start.
    hero_height = 220.0
    anchor_from_bottom = hero_height * (512.0 - 476.0) / 512.0
    hero_center_z = hero_height / 2.0 - anchor_from_bottom
    hero_width = hero_height * 683.0 / 512.0
    spawn_plane(actors, plane, "P5A0_NaiwaGlide", assets["hero"]["material"],
                (0.0, 1450.0, hero_center_z), (hero_width, hero_height))
    actor_labels.append("P5A0_NaiwaGlide")
    spawn_plane(actors, plane, "P5A0_PlayerShadow", assets["shadow"]["material"],
                (0.0, 1400.0, 2.0), (320.0, 90.0))
    actor_labels.append("P5A0_PlayerShadow")

    route_start_z = 0.0
    tree_height = 280.0
    spawn_plane(actors, plane, "P5A0_TreeA", assets["tree"]["material"],
                (620.0, 1450.0, route_start_z + tree_height / 2.0 - 108.5), (140.0, tree_height))
    actor_labels.append("P5A0_TreeA")
    rock_height = 220.0
    spawn_plane(actors, plane, "P5A0_RockA", assets["rock"]["material"],
                (1040.0, 1450.0, -180.0 + rock_height / 2.0 - 22.0), (220.0, rock_height))
    actor_labels.append("P5A0_RockA")

    spawn_plane(actors, plane, "P5A0_AvalancheCloud", assets["avalanche"]["material"],
                (-840.0, 350.0, 360.0), (800.0, 520.0))
    actor_labels.append("P5A0_AvalancheCloud")
    spawn_plane(actors, plane, "P5A0_AvalancheRim", assets["avalanche_rim"]["material"],
                (-680.0, 500.0, 130.0), (900.0, 230.0))
    actor_labels.append("P5A0_AvalancheRim")
    spawn_plane(actors, plane, "P5A0_SnowMist", assets["mist"]["material"],
                (250.0, 1000.0, 75.0), (1100.0, 280.0))
    actor_labels.append("P5A0_SnowMist")

    start_tag = "NaiwaP5A0ArtSamplePlayerStart"
    start = actors.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(0.0, 0.0, 80.0), unreal.Rotator()
    )
    assert start, "Could not spawn P5 A0 player start"
    start.set_editor_property("tags", [unreal.Name(ACTOR_TAG), unreal.Name(start_tag)])
    start.set_actor_label("P5A0_PlayerStart")
    actor_labels.append("P5A0_PlayerStart")

    key = actors.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(550.0, 92.5, 800.0),
        unreal.Rotator(pitch=-45.0, yaw=-30.0)
    )
    assert key, "Could not spawn P5 A0 directional light"
    tag_actor(key, "P5A0_KeyLight")
    key.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("intensity", 3.0)
    actor_labels.append("P5A0_KeyLight")

    assert levels.save_current_level(), f"Could not save A0 art sample: {MAP_PATH}"
    unreal.EditorAssetLibrary.save_directory(MATERIAL_DEST, only_if_is_dirty=False, recursive=True)
    report.update({
        "status": "saved_pending_visible_editor_review",
        "map_status": map_status,
        "game_mode": GAME_MODE_PATH,
        "sample_route": "M2 prototype 1800 m route; runtime geometry and HUD; WIP background strips tiled across the fixture",
        "texture_count": len(assets),
        "background_tiles_per_layer": tile_count,
        "background_layer_count": len(layers),
        "background_coverage_cm": route_length_cm + tile_width_cm,
        "texture_paths": {key: value["texture_path"] for key, value in assets.items()},
        "material_paths": {key: value["material_path"] for key, value in assets.items()},
        "generated_actor_labels": sorted(actor_labels),
        "sample_notes": [
            "All imported PNGs remain WIP and live under /Game/Naiwa/P5/ArtSample/Textures; v04 materials are isolated under ArtSample_v04.",
            "A4 Glide frame is a cell crop from the owner-approved six-pose atlas; anchor remains provisional.",
            "Background strips span the M2 route so scenery remains visible during ordinary play; dynamic camera-relative parallax is not implemented here.",
            "Near-ridge texture tiling and layer transitions remain WIP and need in-game review.",
            "Cutout art uses an explicit unlit masked material; soft mist/shadow remain translucent. Runtime visibility still needs owner review.",
            "Side-view planes use explicit roll=90 degrees (Python Rotator keyword arguments avoid the C++/Python positional-order mismatch).",
            "No 1080p/720p, texture-edge, frame-time, or gameplay acceptance is claimed by this build report.",
        ],
    })
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("NAIWA_P5_A0_ART_SAMPLE_OK " + json.dumps(report, ensure_ascii=False))


try:
    main()
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log_error(report["error"])
    raise
