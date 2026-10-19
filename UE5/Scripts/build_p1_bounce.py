"""Create the isolated P1 bounce-pad prototype without touching P0 or prior P1 maps."""

import hashlib
import json
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
CONFIG = json.loads((ROOT / "UE5/Scripts/p1_bounce_prototype_config_v1.json").read_text(encoding="utf-8"))
OUTPUT = PROJECT / "Saved/NaiwaP1/bounce_build.json"
OUTPUT.parent.mkdir(parents=True, exist_ok=True)

MAP = CONFIG["map"]
ASSET_ROOT = CONFIG["asset_root"]
OWNER_TAG = CONFIG["owner_tag"]
GENERATED_TAG = CONFIG["camera_terrain_tag"]
MAP_TAG = CONFIG["map_tag"]

P0_PROTECTED_FILES = [
    PROJECT / "Content/Naiwa/Maps/L_NaiwaGraybox.umap",
    PROJECT / "Content/Naiwa/Blueprints/BP_NaiwaCharacter.uasset",
    PROJECT / "Content/Naiwa/Blueprints/BP_NaiwaGameMode.uasset",
    PROJECT / "Content/Naiwa/Input/IMC_Naiwa.uasset",
]
P1_PROTECTED_FILES = [
    PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Slice.umap",
    PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Full.umap",
]


def file_hash(path):
    assert path.is_file(), "Protected asset missing: " + str(path)
    return hashlib.sha256(path.read_bytes()).hexdigest()


def protected_hashes(paths):
    return {str(path.relative_to(PROJECT)): file_hash(path) for path in paths}


def resolve_kind(name):
    enum = getattr(unreal, "NaiwaP1TriggerKind", None) or getattr(unreal, "ENaiwaP1TriggerKind", None)
    assert enum, "P1 trigger enum unavailable; build the runtime module first"
    matches = [item for item in dir(enum) if item.replace("_", "").lower() == name.lower()]
    assert len(matches) == 1, "Cannot resolve trigger kind " + name
    return getattr(enum, matches[0])


def add_owned(actor, label, role_tag=None):
    assert actor
    actor.set_actor_label("P1_Bounce_" + label)
    tags = [unreal.Name(OWNER_TAG)]
    if role_tag:
        tags.append(unreal.Name(role_tag))
    actor.set_editor_property("tags", tags)
    return actor


def material(name, rgb):
    path = ASSET_ROOT + "/Materials/M_P1_Bounce_" + name
    existing = unreal.load_asset(path)
    if existing:
        assert isinstance(existing, unreal.Material), path + " is not a material"
        return existing
    result = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_P1_Bounce_" + name,
        ASSET_ROOT + "/Materials",
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    assert result, "Could not create " + path
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        result, unreal.MaterialExpressionConstant3Vector, -350, 0
    )
    expression.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
    assert unreal.MaterialEditingLibrary.connect_material_property(
        expression, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    assert not unreal.MaterialEditingLibrary.recompile_material(result)
    assert unreal.EditorAssetLibrary.save_loaded_asset(result)
    return result


def make_block(actors, cube, mat, label, left, right, top, thickness, depth, role_tag=None):
    center_x = (left + right) * 0.5
    actor = add_owned(
        actors.spawn_actor_from_class(
            unreal.StaticMeshActor,
            unreal.Vector(center_x, 0.0, top - thickness * 0.5),
        ),
        label,
        role_tag,
    )
    actor.set_editor_property("tags", [
        unreal.Name(OWNER_TAG), unreal.Name(GENERATED_TAG),
        *([unreal.Name(role_tag)] if role_tag else []),
    ])
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    assert component
    component.set_static_mesh(cube)
    component.set_material(0, mat)
    actor.set_actor_scale3d(unreal.Vector((right - left) / 100.0, depth / 100.0, thickness / 100.0))
    return actor


def make_map(game_mode_class, mats):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    exists = unreal.EditorAssetLibrary.does_asset_exist(MAP)
    required_labels = {
        "P1_Bounce_Approach", "P1_Bounce_Landing", "P1_Bounce_Pad",
        "P1_Bounce_Hazard", "P1_Bounce_Start", "P1_Bounce_KeyLight",
        "P1_Bounce_SkyLight",
    }
    if exists:
        assert levels.load_level(MAP)
    else:
        assert levels.new_level(MAP)
    world = unreal.EditorLevelLibrary.get_editor_world()
    settings = world.get_world_settings()
    map_tags = [str(tag) for tag in settings.get_editor_property("tags")]
    if exists:
        assert MAP_TAG in map_tags, "Existing map lacks P1 Bounce ownership tag; refusing to edit"
        current = settings.get_editor_property("default_game_mode")
        assert current and current.get_path_name() == game_mode_class.get_path_name(), (
            "Existing map uses an unexpected GameMode; refusing to overwrite"
        )
        existing_actors = actors.get_all_level_actors()
        owned_actors = [actor for actor in existing_actors
                        if OWNER_TAG in [str(tag) for tag in actor.get_editor_property("tags")]]
        owned = [actor.get_actor_label() for actor in owned_actors]
        assert required_labels.issubset(set(owned)), (
            "Existing P1 Bounce map is partial; inspect it before rebuilding. Missing: "
            + str(sorted(required_labels.difference(owned)))
        )
        by_label = {actor.get_actor_label(): actor for actor in owned_actors}
        landing_spec = CONFIG["landing"]
        landing_actor = by_label["P1_Bounce_Landing"]
        landing_actor.set_actor_location(unreal.Vector(
            (landing_spec["left"] + landing_spec["right"]) * 0.5,
            0.0,
            landing_spec["top"] - landing_spec["thickness"] * 0.5,
        ), False, False)
        landing_actor.set_actor_scale3d(unreal.Vector(
            (landing_spec["right"] - landing_spec["left"]) / 100.0,
            landing_spec["depth"] / 100.0,
            landing_spec["thickness"] / 100.0,
        ))
        hazard_spec = CONFIG["hazard"]
        by_label["P1_Bounce_Hazard"].set_actor_location(
            unreal.Vector(hazard_spec["x"], hazard_spec["y"], hazard_spec["z"]), False, False
        )
        pad_spec = CONFIG["pad"]
        by_label["P1_Bounce_Pad"].set_actor_location(
            unreal.Vector(pad_spec["x"], pad_spec["y"], pad_spec["z"]), False, False
        )
        by_label["P1_Bounce_Pad"].set_editor_property(
            "box_extent", unreal.Vector(*pad_spec["extent"])
        )
        key_light_component = by_label["P1_Bounce_KeyLight"].get_component_by_class(
            unreal.DirectionalLightComponent
        )
        key_light_component.set_editor_property("atmosphere_sun_light", True)
        if "P1_Bounce_Atmosphere" not in by_label:
            add_owned(
                actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0.0, 0.0, 0.0)),
                "Atmosphere",
            )
        assert levels.save_current_level(), "Could not save the updated bounce prototype map"
        owned = [actor.get_actor_label() for actor in actors.get_all_level_actors()
                 if OWNER_TAG in [str(tag) for tag in actor.get_editor_property("tags")]]
        return {"map": MAP, "status": "existing_map_updated_upper_landing_and_atmosphere", "owned_actors": owned}

    settings.set_editor_property("tags", [unreal.Name(MAP_TAG)])
    settings.set_editor_property("default_game_mode", game_mode_class)
    assert levels.save_current_level(), "Could not save the new map ownership marker"
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    assert cube, "Engine cube mesh unavailable"

    approach = CONFIG["approach"]
    make_block(actors, cube, mats["approach"], "Approach",
               approach["left"], approach["right"], approach["top"],
               approach["thickness"], approach["depth"])
    landing = CONFIG["landing"]
    make_block(actors, cube, mats["landing"], "Landing",
               landing["left"], landing["right"], landing["top"],
               landing["thickness"], landing["depth"], CONFIG["landing_tag"])

    native_pad = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1BouncePad")
    native_trigger = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1Trigger")
    assert native_pad and native_trigger, "Build native P1 bounce and trigger classes first"
    pad_spec = CONFIG["pad"]
    pad = add_owned(actors.spawn_actor_from_class(
                        native_pad, unreal.Vector(pad_spec["x"], pad_spec["y"], pad_spec["z"])),
                    "Pad", CONFIG["pad_tag"])
    pad.set_editor_properties({
        "launch_velocity_z": CONFIG["launch_velocity_z"],
        "box_extent": unreal.Vector(*pad_spec["extent"]),
    })
    display = pad.get_editor_property("display_mesh")
    assert display
    display.set_material(0, mats["spring"])

    hazard_spec = CONFIG["hazard"]
    hazard = add_owned(
        actors.spawn_actor_from_class(
            native_trigger,
            unreal.Vector(hazard_spec["x"], hazard_spec["y"], hazard_spec["z"]),
        ),
        "Hazard",
        CONFIG["hazard_tag"],
    )
    hazard.set_editor_properties({
        "trigger_kind": resolve_kind("Hazard"),
        "trigger_id": unreal.Name("BounceLandingHazard"),
        "box_extent": unreal.Vector(*hazard_spec["extent"]),
    })
    hazard.get_editor_property("display_mesh").set_material(0, mats["hazard"])

    start = add_owned(
        actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(*CONFIG["spawn"])),
        "Start",
    )
    light = add_owned(
        actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 800.0)),
        "KeyLight",
    )
    light.set_actor_rotation(unreal.Rotator(-45.0, -30.0, 0.0), False)
    light_component = light.get_component_by_class(unreal.DirectionalLightComponent)
    light_component.set_editor_property("intensity", 3.0)
    light_component.set_editor_property("atmosphere_sun_light", True)
    add_owned(actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0.0, 0.0, 600.0)), "SkyLight")
    add_owned(actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0.0, 0.0, 0.0)), "Atmosphere")
    assert start and light
    assert levels.save_current_level(), "Could not save the bounce prototype map"
    return {
        "map": MAP,
        "status": "new_isolated_map_saved",
        "owned_actors": sorted(actor.get_actor_label() for actor in actors.get_all_level_actors()
                                if OWNER_TAG in [str(tag) for tag in actor.get_editor_property("tags")]),
    }


report = {"schema": CONFIG["schema"], "engine": unreal.SystemLibrary.get_engine_version(),
          "status": "running", "target_map": MAP}
before_p0 = protected_hashes(P0_PROTECTED_FILES)
before_p1 = protected_hashes(P1_PROTECTED_FILES)
try:
    assert all(path.exists() for path in P0_PROTECTED_FILES + P1_PROTECTED_FILES)
    mode_bp = unreal.load_asset(ASSET_ROOT + "/BP_NaiwaP1GameMode")
    assert mode_bp, "Build the isolated P1 GameMode Blueprint first"
    mode_class = unreal.BlueprintEditorLibrary.generated_class(mode_bp)
    assert mode_class and unreal.BlueprintEditorLibrary.compile_blueprint(mode_bp)
    mode_defaults = unreal.get_default_object(mode_class)
    pawn_class = mode_defaults.get_editor_property("default_pawn_class")
    controller_class = mode_defaults.get_editor_property("player_controller_class")
    assert pawn_class and "BP_NaiwaP1Character" in pawn_class.get_path_name(), (
        "P1 GameMode must retain its P1-only pawn"
    )
    assert controller_class and controller_class.get_path_name() == "/Script/NaiwaRuntime.NaiwaP1PlayerController", (
        "P1 GameMode must retain its P1-only controller"
    )
    mats = {
        "approach": material("Approach", (0.15, 0.25, 0.33)),
        "landing": material("Landing", (0.08, 0.36, 0.56)),
        "spring": material("Spring", (0.98, 0.48, 0.08)),
        "hazard": material("Hazard", (0.92, 0.08, 0.10)),
    }
    report["assets"] = {key: value.get_path_name() for key, value in mats.items()}
    report["layout"] = make_map(mode_class, mats)
    after_p0 = protected_hashes(P0_PROTECTED_FILES)
    after_p1 = protected_hashes(P1_PROTECTED_FILES)
    assert after_p0 == before_p0, "A protected P0 asset changed during bounce map generation"
    assert after_p1 == before_p1, "An existing P1 Slice/Full map changed during bounce map generation"
    report["p0_protected_sha256"] = after_p0
    report["existing_p1_map_sha256"] = after_p1
    report["status"] = "saved_candidate_pending_walk_run_and_visual_review"
    unreal.log("NAIWA_P1_BOUNCE_BUILD_OK " + MAP)
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    unreal.log_error(report["error"])
    raise
finally:
    OUTPUT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
