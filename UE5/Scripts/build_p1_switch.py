"""Build the isolated P1 touch-switch shuttle prototype map.

Only actors with this map's owner tag are reconciled on a repeat build. The
seven obsolete rescue actors are removed by exact label and tag. Existing P0
and other P1 assets are hashed before and after generation.
"""

import hashlib
import json
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
CONFIG_PATH = ROOT / "UE5/Scripts/p1_switch_prototype_config_v1.json"
CONFIG = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
OUTPUT = PROJECT / "Saved/NaiwaP1/switch_build.json"
OUTPUT.parent.mkdir(parents=True, exist_ok=True)

assert CONFIG["schema"] == "NaiwaP1SwitchPrototypeV1"
assert CONFIG["asset_root"] == "/Game/Naiwa/P1"
assert CONFIG["map"] == "/Game/Naiwa/P1/Maps/L_NaiwaP1_Switch"
ASSET_ROOT = CONFIG["asset_root"]
MAP = CONFIG["map"]
OWNER_TAG = CONFIG["owner_tag"]
MAP_TAG = CONFIG["map_tag"]
PLATFORM_TAG = CONFIG["platform_tag"]
TERRAIN_TAG = CONFIG["camera_terrain_tag"]
OBSOLETE_RESCUE_IDS = (
    "RescueFloor", "RescueLeftLow", "RescueLeftMid", "RescueLeftHigh",
    "RescueRightLow", "RescueRightMid", "RescueRightHigh",
)
OBSOLETE_RESCUE_LABELS = {"P1_Switch_Terrain_" + item for item in OBSOLETE_RESCUE_IDS}
CONFIG_HASH = hashlib.sha256(CONFIG_PATH.read_bytes()).hexdigest()
CONFIG_TAG = "NaiwaP1SwitchCfg_" + CONFIG_HASH[:16]

PROTECTED = [
    PROJECT / "Content/Naiwa/Maps/L_NaiwaGraybox.umap",
    PROJECT / "Content/Naiwa/Blueprints/BP_NaiwaCharacter.uasset",
    PROJECT / "Content/Naiwa/Blueprints/BP_NaiwaGameMode.uasset",
    PROJECT / "Content/Naiwa/Input/IMC_Naiwa.uasset",
    PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Slice.umap",
    PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Full.umap",
    PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Bounce.umap",
    PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Timed.umap",
    PROJECT / "Content/Naiwa/P1/BP_NaiwaP1Character.uasset",
    PROJECT / "Content/Naiwa/P1/BP_NaiwaP1GameMode.uasset",
]

COLORS = {
    "left_bank": (0.11, 0.31, 0.47),
    "right_bank": (0.10, 0.41, 0.41),
    "platform": (0.96, 0.66, 0.12),
    "left_switch": (0.90, 0.28, 0.24),
    "right_switch": (0.21, 0.70, 0.68),
}


def protected_hashes():
    result = {}
    for path in PROTECTED:
        assert path.is_file(), "Protected asset missing: " + str(path)
        result[str(path.relative_to(PROJECT))] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def validate_layout():
    platform = CONFIG["platform"]
    assert platform["at"] == [0.0, 0.0, 0.0]
    assert platform["dock_left_x"] < platform["dock_right_x"]
    assert all(float(value) > 0.0 for value in platform["extent"])
    assert platform["travel_speed_cm_per_second"] > 0.0
    assert platform["stable_boarding_seconds"] >= 0.25
    assert platform["departure_cue_seconds"] >= 0.65
    terrain = {item["id"]: item for item in CONFIG["terrain"]}
    assert len(terrain) == len(CONFIG["terrain"]), "Duplicate terrain id"
    assert set(terrain) == {"LeftBank", "RightBank"}, "The pit must be open to World KillZ"
    left, right = (terrain[key] for key in ("LeftBank", "RightBank"))
    assert left["right"] == -550.0 and right["left"] == 550.0
    assert left["top"] == right["top"] == platform["top_z"]
    assert platform["dock_left_x"] - platform["extent"][0] - left["right"] == 10.0
    assert right["left"] - (platform["dock_right_x"] + platform["extent"][0]) == 10.0
    assert left["left"] < platform["left_switch_x"] < left["right"]
    assert right["left"] < platform["right_switch_x"] < right["right"]
    assert left["left"] < CONFIG["spawn"][0] < left["right"]
    assert abs(CONFIG["spawn"][2] - left["top"] - 92.0) < 1.0
    for item in CONFIG["terrain"]:
        assert item["left"] < item["right"]
        assert item["thickness"] > 0.0 and item["depth"] >= 400.0
        assert item["material"] in COLORS


def material(name, rgb):
    asset_name = "M_P1_Switch_" + name
    path = ASSET_ROOT + "/Materials/" + asset_name
    existing = unreal.load_asset(path)
    if existing:
        assert isinstance(existing, unreal.Material), path + " is not a material"
        return existing
    created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, ASSET_ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew()
    )
    assert created and created.get_path_name().startswith(path + "."), (
        "Material was created outside the isolated P1 path: " + str(created)
    )
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        created, unreal.MaterialExpressionConstant3Vector, -350, 0
    )
    expression.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
    assert unreal.MaterialEditingLibrary.connect_material_property(
        expression, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    assert not unreal.MaterialEditingLibrary.recompile_material(created)
    assert unreal.EditorAssetLibrary.save_loaded_asset(created)
    return created


def set_owned(actor, label, tags=()):
    assert actor
    actor.set_actor_label("P1_Switch_" + label)
    actor.set_editor_property("tags", [unreal.Name(OWNER_TAG), *[unreal.Name(tag) for tag in tags]])
    return actor


def block_geometry(actor, cube, mats, spec):
    assert isinstance(actor, unreal.StaticMeshActor), "Owned terrain has unexpected class"
    actor.set_actor_location(unreal.Vector(
        (spec["left"] + spec["right"]) * 0.5, 0.0,
        spec["top"] - spec["thickness"] * 0.5,
    ), False, False)
    actor.set_actor_scale3d(unreal.Vector(
        (spec["right"] - spec["left"]) / 100.0,
        spec["depth"] / 100.0,
        spec["thickness"] / 100.0,
    ))
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    assert component
    component.set_static_mesh(cube)
    component.set_material(0, mats[spec["material"]])
    actor.set_editor_property("tags", [unreal.Name(OWNER_TAG), unreal.Name(TERRAIN_TAG)])


def platform_geometry(actor, mats):
    native = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1SwitchPlatform")
    assert native and actor.get_class().get_path_name() == native.get_path_name()
    spec = CONFIG["platform"]
    actor.set_actor_location(unreal.Vector(*spec["at"]), False, False)
    actor.set_editor_properties({
        "dock_left_x": spec["dock_left_x"],
        "dock_right_x": spec["dock_right_x"],
        "left_switch_x": spec["left_switch_x"],
        "right_switch_x": spec["right_switch_x"],
        "platform_top_z": spec["top_z"],
        "platform_extent": unreal.Vector(*spec["extent"]),
        "travel_speed_cm_per_second": spec["travel_speed_cm_per_second"],
        "stable_boarding_seconds": spec["stable_boarding_seconds"],
        "departure_cue_seconds": spec["departure_cue_seconds"],
    })
    for component_name, material_name in (
        ("platform_display", "platform"),
        ("left_switch_display", "left_switch"),
        ("right_switch_display", "right_switch"),
    ):
        component = actor.get_editor_property(component_name)
        assert component, "Missing native platform component: " + component_name
        component.set_material(0, mats[material_name])
    actor.set_editor_property("tags", [unreal.Name(OWNER_TAG), unreal.Name(PLATFORM_TAG)])


def validate_game_mode():
    mode_bp = unreal.load_asset(ASSET_ROOT + "/BP_NaiwaP1GameMode")
    assert mode_bp, "P1 GameMode Blueprint missing"
    mode_class = unreal.BlueprintEditorLibrary.generated_class(mode_bp)
    assert mode_class
    defaults = unreal.get_default_object(mode_class)
    pawn = defaults.get_editor_property("default_pawn_class")
    controller = defaults.get_editor_property("player_controller_class")
    assert pawn and "BP_NaiwaP1Character" in pawn.get_path_name()
    assert controller and controller.get_path_name() == "/Script/NaiwaRuntime.NaiwaP1PlayerController"
    return mode_class


def build_map(mode_class, mats):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    exists = unreal.EditorAssetLibrary.does_asset_exist(MAP)
    assert levels.load_level(MAP) if exists else levels.new_level(MAP)
    world = unreal.EditorLevelLibrary.get_editor_world()
    settings = world.get_world_settings()
    expected = {"P1_Switch_Terrain_" + item["id"] for item in CONFIG["terrain"]}
    expected.update({
        "P1_Switch_Platform", "P1_Switch_Start", "P1_Switch_KeyLight",
        "P1_Switch_SkyLight", "P1_Switch_Atmosphere",
    })
    if exists:
        tags = [str(tag) for tag in settings.get_editor_property("tags")]
        assert MAP_TAG in tags, "Existing map lacks switch prototype ownership tag"
        current_mode = settings.get_editor_property("default_game_mode")
        assert current_mode and current_mode.get_path_name() == mode_class.get_path_name(), (
            "Existing map uses unexpected GameMode; refusing to edit"
        )
        owned = [actor for actor in actors.get_all_level_actors()
                 if OWNER_TAG in [str(tag) for tag in actor.get_editor_property("tags")]]
        labels = [actor.get_actor_label() for actor in owned]
        assert len(labels) == len(set(labels)), "Duplicate owned actor labels"
        actual = set(labels)
        assert expected <= actual and actual <= expected | OBSOLETE_RESCUE_LABELS, (
            "Existing map ownership set differs; inspect before rebuilding: "
            + str({"missing": sorted(expected - actual),
                   "extra": sorted(actual - expected - OBSOLETE_RESCUE_LABELS)})
        )
        by_label = {actor.get_actor_label(): actor for actor in owned}
        assert PLATFORM_TAG in [str(tag) for tag in by_label["P1_Switch_Platform"].get_editor_property("tags")], (
            "Existing platform lacks its probe identity tag"
        )
        assert isinstance(by_label["P1_Switch_SkyLight"], unreal.SkyLight)
        assert isinstance(by_label["P1_Switch_Atmosphere"], unreal.SkyAtmosphere)
        obsolete = sorted(actual & OBSOLETE_RESCUE_LABELS)
        for label in obsolete:
            actor = by_label[label]
            actor_tags = {str(tag) for tag in actor.get_editor_property("tags")}
            assert isinstance(actor, unreal.StaticMeshActor)
            assert OWNER_TAG in actor_tags and TERRAIN_TAG in actor_tags, (
                "Refusing to remove an actor without both switch terrain tags: " + label
            )
        for label in obsolete:
            removed = actors.destroy_actor(by_label[label])
            assert removed, "Could not remove obsolete switch rescue actor: " + label
            del by_label[label]
    else:
        settings.set_editor_property("tags", [unreal.Name(MAP_TAG), unreal.Name(CONFIG_TAG)])
        settings.set_editor_property("default_game_mode", mode_class)
        assert levels.save_current_level(), "Could not save new map ownership marker"
        by_label = {}
        obsolete = []

    settings.set_editor_property("kill_z", -1000.0)

    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    assert cube, "Engine cube unavailable"
    for spec in CONFIG["terrain"]:
        label = "P1_Switch_Terrain_" + spec["id"]
        actor = by_label.get(label)
        if actor is None:
            actor = set_owned(actors.spawn_actor_from_class(
                unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, 0.0)
            ), "Terrain_" + spec["id"], (TERRAIN_TAG,))
        block_geometry(actor, cube, mats, spec)

    native = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1SwitchPlatform")
    assert native, "Build NaiwaP1SwitchPlatform native class first"
    platform = by_label.get("P1_Switch_Platform")
    if platform is None:
        platform = set_owned(actors.spawn_actor_from_class(
            native, unreal.Vector(*CONFIG["platform"]["at"])
        ), "Platform", (PLATFORM_TAG,))
    platform_geometry(platform, mats)

    start = by_label.get("P1_Switch_Start")
    if start is None:
        start = set_owned(actors.spawn_actor_from_class(
            unreal.PlayerStart, unreal.Vector(*CONFIG["spawn"])
        ), "Start")
    assert isinstance(start, unreal.PlayerStart), "Owned start has unexpected class"
    start.set_actor_location(unreal.Vector(*CONFIG["spawn"]), False, False)

    light = by_label.get("P1_Switch_KeyLight")
    if light is None:
        light = set_owned(actors.spawn_actor_from_class(
            unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 800.0)
        ), "KeyLight")
    assert isinstance(light, unreal.DirectionalLight)
    light.set_actor_rotation(unreal.Rotator(-45.0, -30.0, 0.0), False)
    light_component = light.get_component_by_class(unreal.DirectionalLightComponent)
    assert light_component
    light_component.set_editor_property("intensity", 3.0)
    light_component.set_editor_property("atmosphere_sun_light", True)
    if "P1_Switch_SkyLight" not in by_label:
        set_owned(actors.spawn_actor_from_class(
            unreal.SkyLight, unreal.Vector(0.0, 0.0, 600.0)
        ), "SkyLight")
    if "P1_Switch_Atmosphere" not in by_label:
        set_owned(actors.spawn_actor_from_class(
            unreal.SkyAtmosphere, unreal.Vector(0.0, 0.0, 0.0)
        ), "Atmosphere")
    tags = [str(tag) for tag in settings.get_editor_property("tags")]
    tags = [tag for tag in tags if not tag.startswith("NaiwaP1SwitchCfg_")]
    if MAP_TAG not in tags:
        tags.append(MAP_TAG)
    tags.append(CONFIG_TAG)
    settings.set_editor_property("tags", [unreal.Name(tag) for tag in tags])
    assert levels.save_current_level(), "Could not save P1 switch map"
    final_owned = [actor.get_actor_label() for actor in actors.get_all_level_actors()
                   if OWNER_TAG in [str(tag) for tag in actor.get_editor_property("tags")]]
    assert set(final_owned) == expected
    return {"map": MAP, "status": "existing_map_reconciled" if exists else "new_isolated_map_saved",
            "owned_actors": sorted(final_owned), "removed_rescue_actors": obsolete,
            "kill_z": settings.get_editor_property("kill_z"), "config_sha256": CONFIG_HASH}


report = {
    "schema": CONFIG["schema"],
    "target_map": MAP,
    "config_sha256": CONFIG_HASH,
    "engine": unreal.SystemLibrary.get_engine_version(),
    "status": "running",
}
try:
    validate_layout()
    before = protected_hashes()
    mode_class = validate_game_mode()
    mats = {name: material(name, rgb) for name, rgb in COLORS.items()}
    report["assets"] = {name: mat.get_path_name() for name, mat in mats.items()}
    report["layout"] = build_map(mode_class, mats)
    after = protected_hashes()
    assert after == before, "A protected P0 or existing P1 asset changed during switch map generation"
    report["protected_sha256"] = after
    report["status"] = "saved_candidate_pending_switch_probe_and_visual_review"
    unreal.log("NAIWA_P1_SWITCH_BUILD_OK " + MAP)
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    unreal.log_error(report["error"])
    raise
finally:
    OUTPUT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
