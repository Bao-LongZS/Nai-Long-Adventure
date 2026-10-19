"""Build the isolated P1 timed-platform prototype map.

Only actors with this map's owner tag are reconciled on a repeat build. Existing
P0, Slice, Full, Bounce, and Switch assets are hashed before and after generation.
"""

import hashlib
import json
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
CONFIG_PATH = ROOT / "UE5/Scripts/p1_timed_prototype_config_v1.json"
CONFIG = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
OUTPUT = PROJECT / "Saved/NaiwaP1/timed_build.json"
OUTPUT.parent.mkdir(parents=True, exist_ok=True)

assert CONFIG["schema"] == "NaiwaP1TimedPrototypeV1"
assert CONFIG["asset_root"] == "/Game/Naiwa/P1"
assert CONFIG["map"] == "/Game/Naiwa/P1/Maps/L_NaiwaP1_Timed"
ASSET_ROOT = CONFIG["asset_root"]
MAP = CONFIG["map"]
OWNER_TAG = CONFIG["owner_tag"]
MAP_TAG = CONFIG["map_tag"]
PLATFORM_TAG = CONFIG["platform_tag"]
TERRAIN_TAG = CONFIG["camera_terrain_tag"]
CONFIG_HASH = hashlib.sha256(CONFIG_PATH.read_bytes()).hexdigest()
CONFIG_TAG = "NaiwaP1TimedCfg_" + CONFIG_HASH[:16]
TARGET_MAP_FILE = PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Timed.umap"

PROTECTED = [
    PROJECT / "Content/Naiwa/Maps/L_NaiwaGraybox.umap",
    PROJECT / "Content/Naiwa/Blueprints/BP_NaiwaCharacter.uasset",
    PROJECT / "Content/Naiwa/Blueprints/BP_NaiwaGameMode.uasset",
    PROJECT / "Content/Naiwa/Input/IMC_Naiwa.uasset",
    PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Slice.umap",
    PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Full.umap",
    PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Bounce.umap",
    PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Switch.umap",
    PROJECT / "Content/Naiwa/P1/BP_NaiwaP1Character.uasset",
    PROJECT / "Content/Naiwa/P1/BP_NaiwaP1GameMode.uasset",
]

COLORS = {
    "left_bank": (0.10, 0.35, 0.48),
    "right_bank": (0.11, 0.43, 0.38),
    "rescue_floor": (0.15, 0.22, 0.27),
    "rescue_step": (0.26, 0.39, 0.45),
    "platform": (0.95, 0.55, 0.13),
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
    assert all(float(value) > 0.0 for value in platform["extent"])
    assert platform["top_z"] == 120.0 and platform["extent"] == [210.0, 160.0, 20.0]
    assert platform["stable_landing_seconds"] >= 0.15
    assert platform["active_seconds"] >= (
        2.0 * platform["extent"][0] + 20.0
    ) / 150.0 + 0.5, "Walk150 has insufficient timed crossing margin"
    assert platform["disabled_seconds"] >= 1.0
    terrain = {item["id"]: item for item in CONFIG["terrain"]}
    assert len(terrain) == len(CONFIG["terrain"]), "Duplicate terrain id"
    assert len(terrain) == 2, "Expected two banks; expired floor falls to KillZ for a safe retry"
    left, right = (terrain[key] for key in ("LeftBank", "RightBank"))
    assert left["right"] == -220.0 and right["left"] == 220.0
    assert left["top"] == right["top"] == platform["top_z"]
    assert -platform["extent"][0] - left["right"] == 10.0
    assert right["left"] - platform["extent"][0] == 10.0
    assert left["left"] < CONFIG["spawn"][0] < left["right"]
    assert abs(CONFIG["spawn"][2] - left["top"] - 92.0) < 1.0
    for item in CONFIG["terrain"]:
        assert item["left"] < item["right"]
        assert item["thickness"] > 0.0 and item["depth"] >= 400.0
        assert item["material"] in COLORS
    assert all(floor["left"] <= item["left"] and item["right"] <= floor["right"]
               for item in CONFIG["terrain"] if item["id"].startswith("Rescue"))


def material(name, rgb):
    asset_name = "M_P1_Timed_" + name
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
    actor.set_actor_label("P1_Timed_" + label)
    actor.set_editor_property("tags", [unreal.Name(OWNER_TAG), *[unreal.Name(tag) for tag in tags]])
    return actor


def block_geometry(actor, cube, mats, spec):
    assert isinstance(actor, unreal.StaticMeshActor), "Owned terrain has unexpected class"
    actor.set_actor_location(unreal.Vector(
        (spec["left"] + spec["right"]) * 0.5, 0.0,
        spec["top"] - spec["thickness"] * 0.5,
    ), False, False)
    actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
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
    native = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1TimedPlatform")
    assert native and actor.get_class().get_path_name() == native.get_path_name()
    spec = CONFIG["platform"]
    actor.set_actor_location(unreal.Vector(*spec["at"]), False, False)
    actor.set_editor_properties({
        "platform_top_z": spec["top_z"],
        "platform_extent": unreal.Vector(*spec["extent"]),
        "stable_landing_seconds": spec["stable_landing_seconds"],
        "active_seconds": spec["active_seconds"],
        "disabled_seconds": spec["disabled_seconds"],
    })
    component = actor.get_editor_property("platform_display")
    assert component, "Missing native timed platform display component"
    component.set_material(0, mats["platform"])
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
    expected = {"P1_Timed_Terrain_" + item["id"] for item in CONFIG["terrain"]}
    expected.update({
        "P1_Timed_Platform", "P1_Timed_Start", "P1_Timed_KeyLight",
        "P1_Timed_SkyLight", "P1_Timed_Atmosphere",
    })
    if exists:
        tags = [str(tag) for tag in settings.get_editor_property("tags")]
        assert MAP_TAG in tags, "Existing map lacks timed prototype ownership tag"
        current_mode = settings.get_editor_property("default_game_mode")
        assert current_mode and current_mode.get_path_name() == mode_class.get_path_name(), (
            "Existing map uses unexpected GameMode; refusing to edit"
        )
        owned = [actor for actor in actors.get_all_level_actors()
                 if OWNER_TAG in [str(tag) for tag in actor.get_editor_property("tags")]]
        labels = [actor.get_actor_label() for actor in owned]
        assert len(labels) == len(set(labels)), "Duplicate owned actor labels"
        retired = {"P1_Timed_Terrain_Rescue" + side + stage
                   for side in ("Left", "Right") for stage in ("Low", "Mid", "High")}
        retired.update({"P1_Timed_Terrain_RescueFloor",
                        "P1_Timed_Terrain_RescueLeftRamp",
                        "P1_Timed_Terrain_RescueRightRamp"})
        extra = set(labels) - expected
        missing = expected - set(labels)
        assert extra <= retired and not missing, ("Existing map ownership set differs; inspect before rebuilding: "
            + str({"missing": sorted(missing), "extra": sorted(extra)}))
        by_label = {actor.get_actor_label(): actor for actor in owned}
        for label in sorted(extra):
            assert actors.destroy_actor(by_label.pop(label)), "Could not retire owned rescue step: " + label
        assert PLATFORM_TAG in [str(tag) for tag in by_label["P1_Timed_Platform"].get_editor_property("tags")], (
            "Existing platform lacks its probe identity tag"
        )
        assert isinstance(by_label["P1_Timed_SkyLight"], unreal.SkyLight)
        assert isinstance(by_label["P1_Timed_Atmosphere"], unreal.SkyAtmosphere)
    else:
        settings.set_editor_property("tags", [unreal.Name(MAP_TAG), unreal.Name(CONFIG_TAG)])
        settings.set_editor_property("default_game_mode", mode_class)
        assert levels.save_current_level(), "Could not save new map ownership marker"
        by_label = {}

    settings.set_editor_property("kill_z", -1000.0)

    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    assert cube, "Engine cube unavailable"
    for spec in CONFIG["terrain"]:
        label = "P1_Timed_Terrain_" + spec["id"]
        actor = by_label.get(label)
        if actor is None:
            actor = set_owned(actors.spawn_actor_from_class(
                unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, 0.0)
            ), "Terrain_" + spec["id"], (TERRAIN_TAG,))
        block_geometry(actor, cube, mats, spec)

    native = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1TimedPlatform")
    assert native, "Build NaiwaP1TimedPlatform native class first"
    platform = by_label.get("P1_Timed_Platform")
    if platform is None:
        platform = set_owned(actors.spawn_actor_from_class(
            native, unreal.Vector(*CONFIG["platform"]["at"])
        ), "Platform", (PLATFORM_TAG,))
    platform_geometry(platform, mats)

    start = by_label.get("P1_Timed_Start")
    if start is None:
        start = set_owned(actors.spawn_actor_from_class(
            unreal.PlayerStart, unreal.Vector(*CONFIG["spawn"])
        ), "Start")
    assert isinstance(start, unreal.PlayerStart), "Owned start has unexpected class"
    start.set_actor_location(unreal.Vector(*CONFIG["spawn"]), False, False)

    light = by_label.get("P1_Timed_KeyLight")
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
    if "P1_Timed_SkyLight" not in by_label:
        set_owned(actors.spawn_actor_from_class(
            unreal.SkyLight, unreal.Vector(0.0, 0.0, 600.0)
        ), "SkyLight")
    if "P1_Timed_Atmosphere" not in by_label:
        set_owned(actors.spawn_actor_from_class(
            unreal.SkyAtmosphere, unreal.Vector(0.0, 0.0, 0.0)
        ), "Atmosphere")
    tags = [str(tag) for tag in settings.get_editor_property("tags")]
    tags = [tag for tag in tags if not tag.startswith("NaiwaP1TimedCfg_")]
    if MAP_TAG not in tags:
        tags.append(MAP_TAG)
    tags.append(CONFIG_TAG)
    settings.set_editor_property("tags", [unreal.Name(tag) for tag in tags])
    assert levels.save_current_level(), "Could not save P1 timed map"
    final_owned = [actor.get_actor_label() for actor in actors.get_all_level_actors()
                   if OWNER_TAG in [str(tag) for tag in actor.get_editor_property("tags")]]
    assert set(final_owned) == expected
    return {"map": MAP, "status": "existing_map_reconciled" if exists else "new_isolated_map_saved",
            "owned_actors": sorted(final_owned), "config_sha256": CONFIG_HASH}


report = {
    "schema": CONFIG["schema"],
    "target_map": MAP,
    "hash_algorithm": "SHA-256",
    "config_sha256": CONFIG_HASH,
    "builder_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    "engine": unreal.SystemLibrary.get_engine_version(),
    "status": "running",
}
try:
    validate_layout()
    terrain_by_id = {item["id"]: item for item in CONFIG["terrain"]}
    left, right = terrain_by_id["LeftBank"], terrain_by_id["RightBank"]
    estimated_walk_crossing = (right["left"] - left["right"]) / 150.0
    report["design_metrics"] = {
        "walk150_crossing_seconds_at_constant_speed": round(estimated_walk_crossing, 3),
        "walk150_active_margin_seconds": round(CONFIG["platform"]["active_seconds"] - estimated_walk_crossing, 3),
        "fall_failure": "no_lower_collision; natural KillZ death and static-left-bank respawn",
        "kill_z": -1000.0,
        "note": "Natural fall/death/respawn and retry require separate Walk/Run runtime probes",
    }
    before = protected_hashes()
    report["protected_sha256_before"] = before
    mode_class = validate_game_mode()
    mats = {name: material(name, rgb) for name, rgb in COLORS.items()}
    report["assets"] = {name: mat.get_path_name() for name, mat in mats.items()}
    report["layout"] = build_map(mode_class, mats)
    after = protected_hashes()
    report["protected_sha256_after"] = after
    assert after == before, "A protected P0 or existing P1 asset changed during timed map generation"
    assert TARGET_MAP_FILE.is_file(), "Saved timed map package is missing"
    report["target_map_package"] = str(TARGET_MAP_FILE.relative_to(PROJECT)).replace("\\", "/")
    report["target_map_sha256"] = hashlib.sha256(TARGET_MAP_FILE.read_bytes()).hexdigest()
    report["target_map_bytes"] = TARGET_MAP_FILE.stat().st_size
    report["status"] = "saved_candidate_pending_timed_probe_and_visual_review"
    unreal.log("NAIWA_P1_TIMED_BUILD_OK " + MAP)
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    unreal.log_error(report["error"])
    raise
finally:
    OUTPUT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
