"""Build only the first P1-5b teaching slice in a separate P1 map.

Deploy this file and its config to UE5/Scripts before running. The saved map
contains hazard cue -> switch shuttle -> bounce pad -> static checkpoint. It is
not the four-mechanism route and is not an acceptance result. Existing maps and
Blueprints are hash-protected; unknown actors in the target map are refused.
"""

import hashlib
import json
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
CONFIG_PATH = ROOT / "UE5/Scripts/p1_integrated_slice_config_v1.json"
CONFIG = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
OUTPUT = PROJECT / "Saved/NaiwaP1/integrated_slice_build.json"
OUTPUT.parent.mkdir(parents=True, exist_ok=True)

assert CONFIG["schema"] == "NaiwaP1IntegratedTeachingSliceV1"
assert CONFIG["phase"] == "P1-5b_partial_teaching_slice"
assert CONFIG["asset_root"] == "/Game/Naiwa/P1"
assert CONFIG["map"] == "/Game/Naiwa/P1/Maps/L_NaiwaP1_Integrated"
ASSET_ROOT = CONFIG["asset_root"]
MAP = CONFIG["map"]
MAP_TAG = CONFIG["map_tag"]
OWNER_TAG = CONFIG["owner_tag"]
TERRAIN_TAG = CONFIG["camera_terrain_tag"]
PARTIAL_TAG = "NaiwaP1PartialTeachingSliceV1"
CONFIG_HASH = hashlib.sha256(CONFIG_PATH.read_bytes()).hexdigest()
CONFIG_TAG = "NaiwaP1IntegratedCfg_" + CONFIG_HASH[:16]
PREFIX = "P1_Integrated_"

PROTECTED = [
    PROJECT / "Content/Naiwa/Maps/L_NaiwaGraybox.umap",
    PROJECT / "Content/Naiwa/Blueprints/BP_NaiwaCharacter.uasset",
    PROJECT / "Content/Naiwa/Blueprints/BP_NaiwaGameMode.uasset",
    PROJECT / "Content/Naiwa/Input/IMC_Naiwa.uasset",
    PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Slice.umap",
    PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Full.umap",
    PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Bounce.umap",
    PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Switch.umap",
    PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Timed.umap",
    PROJECT / "Content/Naiwa/P1/BP_NaiwaP1Character.uasset",
    PROJECT / "Content/Naiwa/P1/BP_NaiwaP1GameMode.uasset",
]

COLORS = {
    "left_bank": (0.11, 0.31, 0.47),
    "right_bank": (0.10, 0.41, 0.41),
    "landing": (0.08, 0.36, 0.56),
    "hazard": (0.94, 0.11, 0.16),
    "cue": (1.0, 0.56, 0.04),
    "switch_platform": (0.96, 0.66, 0.12),
    "left_switch": (0.90, 0.28, 0.24),
    "right_switch": (0.21, 0.70, 0.68),
    "bounce": (0.98, 0.48, 0.08),
    "checkpoint": (0.12, 0.85, 0.35),
}


def protected_hashes():
    result = {}
    for path in PROTECTED:
        assert path.is_file(), "Protected asset missing: " + str(path)
        result[str(path.relative_to(PROJECT))] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def validate_layout():
    terrain = {item["id"]: item for item in CONFIG["terrain"]}
    assert len(terrain) == len(CONFIG["terrain"]), "Duplicate terrain ID"
    assert set(terrain) == {"LeftBank", "RightBank", "BounceLanding"}
    left, right, landing = (terrain[key] for key in ("LeftBank", "RightBank", "BounceLanding"))
    switch = CONFIG["switch"]
    bounce = CONFIG["bounce"]
    hazard = CONFIG["hazard"]
    cue = CONFIG["hazard_cue"]
    checkpoint = CONFIG["checkpoint"]
    assert CONFIG["kill_z"] == -1000.0
    assert CONFIG["spawn"] == [-1450.0, 0.0, 212.0]
    assert left["left"] < CONFIG["spawn"][0] < cue["at"][0] < hazard["at"][0] < switch["left_switch_x"] < left["right"]
    assert left["top"] == right["top"] == switch["top_z"] == 120.0
    assert left["right"] == -550.0 and right["left"] == 550.0
    assert switch["at"] == [0.0, 0.0, 0.0]
    assert switch["dock_left_x"] == -420.0 and switch["dock_right_x"] == 420.0
    assert switch["extent"] == [120.0, 160.0, 20.0]
    assert switch["dock_left_x"] - switch["extent"][0] - left["right"] == 10.0
    assert right["left"] - switch["dock_right_x"] - switch["extent"][0] == 10.0
    assert right["left"] < switch["right_switch_x"] < right["right"]
    assert switch["stable_boarding_seconds"] >= 0.25
    assert switch["departure_cue_seconds"] >= 0.65
    assert switch["travel_speed_cm_per_second"] > 0.0
    assert hazard["at"][2] - hazard["extent"][2] == left["top"]
    assert cue["at"][2] - cue["scale"][2] * 50.0 == left["top"]
    assert all(value > 0.0 for value in hazard["extent"] + switch["extent"])
    assert bounce["at"] == [1660.0, 0.0, 0.0]
    assert bounce["extent"] == [160.0, 120.0, 12.0]
    assert bounce["launch_velocity_z"] == 1000.0
    assert right["right"] == bounce["at"][0] - 200.0
    assert landing["left"] == bounce["at"][0] + 80.0
    assert landing["top"] == 180.0
    assert checkpoint["at"] == [2700.0, 0.0, 272.0]
    assert checkpoint["at"][2] - 92.0 == landing["top"]
    assert checkpoint["respawn_offset"] == [0.0, 0.0, 0.0]
    assert landing["left"] + 150.0 < checkpoint["at"][0] < landing["right"] - 150.0
    assert len({hazard["id"], checkpoint["id"]}) == 2
    for block in terrain.values():
        assert block["left"] < block["right"] and block["thickness"] > 0.0
        assert block["depth"] >= 400.0 and block["material"] in COLORS


def label(role):
    return PREFIX + role


def expected_labels():
    result = {label("Terrain_" + block["id"]) for block in CONFIG["terrain"]}
    result.update({label(role) for role in (
        "Hazard", "HazardCue", "Switch", "Bounce", "Checkpoint",
        "Start", "KeyLight", "SkyLight", "Atmosphere",
    )})
    return result


def tags_of(actor):
    return {str(item) for item in actor.get_editor_property("tags")}


def validate_actor(role, actor):
    assert OWNER_TAG in tags_of(actor), "Owned actor lost ownership tag: " + label(role)
    if role.startswith("Terrain_"):
        assert actor.get_class().get_path_name() == "/Script/Engine.StaticMeshActor"
        assert TERRAIN_TAG in tags_of(actor)
    elif role == "HazardCue":
        assert actor.get_class().get_path_name() == "/Script/Engine.StaticMeshActor"
        assert CONFIG["cue_tag"] in tags_of(actor)
    elif role in ("Hazard", "Checkpoint"):
        assert actor.get_class().get_path_name() == "/Script/NaiwaRuntime.NaiwaP1Trigger"
        tag = CONFIG["hazard_tag"] if role == "Hazard" else CONFIG["checkpoint_tag"]
        assert tag in tags_of(actor)
    elif role in ("Switch", "Bounce"):
        kind = "NaiwaP1SwitchPlatform" if role == "Switch" else "NaiwaP1BouncePad"
        assert actor.get_class().get_path_name() == "/Script/NaiwaRuntime." + kind
        assert CONFIG["switch_tag" if role == "Switch" else "bounce_tag"] in tags_of(actor)
    else:
        native_name = {"Start": "PlayerStart", "KeyLight": "DirectionalLight",
                       "SkyLight": "SkyLight", "Atmosphere": "SkyAtmosphere"}[role]
        assert actor.get_class().get_path_name() == "/Script/Engine." + native_name, (
            "Unexpected class for " + label(role)
        )


def is_engine_system_actor(actor):
    # These are editor-generated world bookkeeping actors, not level content.
    return actor.get_class().get_path_name() in (
        "/Script/Engine.WorldSettings", "/Script/Engine.LevelBounds"
    )


def validate_game_mode():
    mode_bp = unreal.load_asset(ASSET_ROOT + "/BP_NaiwaP1GameMode")
    assert mode_bp and isinstance(mode_bp, unreal.Blueprint), "P1 GameMode BP missing"
    mode_class = unreal.BlueprintEditorLibrary.generated_class(mode_bp)
    assert mode_class, "P1 GameMode generated class missing"
    defaults = unreal.get_default_object(mode_class)
    pawn = defaults.get_editor_property("default_pawn_class")
    controller = defaults.get_editor_property("player_controller_class")
    assert pawn and "BP_NaiwaP1Character" in pawn.get_path_name()
    assert controller and controller.get_path_name() == "/Script/NaiwaRuntime.NaiwaP1PlayerController"
    for native_name in ("NaiwaP1Trigger", "NaiwaP1SwitchPlatform", "NaiwaP1BouncePad"):
        assert unreal.load_class(None, "/Script/NaiwaRuntime." + native_name), native_name
    return mode_class


def load_and_preflight(mode_class):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    exists = unreal.EditorAssetLibrary.does_asset_exist(MAP)
    assert levels.load_level(MAP) if exists else levels.new_level(MAP)
    world = unreal.EditorLevelLibrary.get_editor_world()
    settings = world.get_world_settings()
    if exists:
        tags = tags_of(settings)
        assert MAP_TAG in tags and PARTIAL_TAG in tags, "Existing map lacks exact teaching-slice marker"
        current_mode = settings.get_editor_property("default_game_mode")
        assert current_mode and current_mode.get_path_name() == mode_class.get_path_name(), (
            "Existing map uses another GameMode"
        )
    by_label = {}
    expected = expected_labels()
    for actor in actors.get_all_level_actors():
        if is_engine_system_actor(actor):
            continue
        actor_label = actor.get_actor_label()
        assert actor_label in expected, "Unknown actor in integrated map: " + actor_label
        assert actor_label not in by_label, "Duplicate integrated actor: " + actor_label
        role = actor_label[len(PREFIX):]
        validate_actor(role, actor)
        by_label[actor_label] = actor
    if not exists:
        settings.set_editor_property("tags", [
            unreal.Name(MAP_TAG), unreal.Name(PARTIAL_TAG), unreal.Name(CONFIG_TAG)
        ])
        settings.set_editor_property("default_game_mode", mode_class)
        assert levels.save_current_level(), "Could not persist new map ownership marker"
    return levels, actors, settings, by_label, exists


def material(name, rgb):
    asset_name = "M_P1_Integrated_" + name
    path = ASSET_ROOT + "/Materials/" + asset_name
    existing = unreal.load_asset(path)
    if existing:
        assert isinstance(existing, unreal.Material), path + " has unexpected class"
        return existing
    created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, ASSET_ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew()
    )
    assert created and created.get_path_name().startswith(path + ".")
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


def owned(actor, role, role_tags=()):
    assert actor
    actor.set_actor_label(label(role))
    actor.set_editor_property("tags", [
        unreal.Name(OWNER_TAG), *[unreal.Name(item) for item in role_tags]
    ])
    return actor


def ensure_actor(actors, by_label, role, klass, position, role_tags=()):
    actor = by_label.get(label(role))
    if actor is None:
        actor = owned(actors.spawn_actor_from_class(klass, unreal.Vector(*position)), role, role_tags)
        by_label[label(role)] = actor
    else:
        validate_actor(role, actor)
    # Actor transform is part of the mechanism contract. A previous editor
    # rotation/scale must not survive a repeat build (especially on the pad).
    actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
    actor.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
    actor.set_actor_location(unreal.Vector(*position), False, False)
    return actor


def configure_terrain(actors, by_label, cube, mats, block):
    role = "Terrain_" + block["id"]
    actor = ensure_actor(actors, by_label, role, unreal.StaticMeshActor,
                         [(block["left"] + block["right"]) / 2.0, 0.0,
                          block["top"] - block["thickness"] / 2.0], (TERRAIN_TAG,))
    mesh = actor.get_component_by_class(unreal.StaticMeshComponent)
    assert mesh
    mesh.set_static_mesh(cube)
    mesh.set_material(0, mats[block["material"]])
    actor.set_actor_scale3d(unreal.Vector(
        (block["right"] - block["left"]) / 100.0,
        block["depth"] / 100.0,
        block["thickness"] / 100.0,
    ))


def resolve_trigger_kind(name):
    enum = getattr(unreal, "NaiwaP1TriggerKind", None) or getattr(unreal, "ENaiwaP1TriggerKind", None)
    assert enum, "NaiwaP1TriggerKind enum unavailable"
    matches = [item for item in dir(enum) if item.replace("_", "").lower() == name.lower()]
    assert len(matches) == 1, "Cannot resolve trigger kind: " + name
    return getattr(enum, matches[0])


def configure_trigger(actors, by_label, mats, role, spec, kind, role_tag, material_name):
    klass = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1Trigger")
    actor = ensure_actor(actors, by_label, role, klass, spec["at"], (role_tag,))
    values = {
        "trigger_kind": resolve_trigger_kind(kind),
        "trigger_id": unreal.Name(spec["id"]),
        "box_extent": unreal.Vector(*spec["extent"]),
    }
    if kind == "Checkpoint":
        values["respawn_offset"] = unreal.Vector(*spec["respawn_offset"])
    actor.set_editor_properties(values)
    display = actor.get_editor_property("display_mesh")
    assert display
    display.set_material(0, mats[material_name])


def configure_cue(actors, by_label, cube, mats):
    spec = CONFIG["hazard_cue"]
    actor = ensure_actor(actors, by_label, "HazardCue", unreal.StaticMeshActor,
                         spec["at"], (CONFIG["cue_tag"],))
    mesh = actor.get_component_by_class(unreal.StaticMeshComponent)
    assert mesh
    mesh.set_static_mesh(cube)
    mesh.set_material(0, mats["cue"])
    mesh.set_collision_profile_name("NoCollision")
    mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    actor.set_actor_scale3d(unreal.Vector(*spec["scale"]))


def configure_switch(actors, by_label, mats):
    spec = CONFIG["switch"]
    klass = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1SwitchPlatform")
    actor = ensure_actor(actors, by_label, "Switch", klass, spec["at"], (CONFIG["switch_tag"],))
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
        ("platform_display", "switch_platform"),
        ("left_switch_display", "left_switch"),
        ("right_switch_display", "right_switch"),
    ):
        component = actor.get_editor_property(component_name)
        assert component, "Missing native component " + component_name
        component.set_material(0, mats[material_name])


def configure_bounce(actors, by_label, mats):
    spec = CONFIG["bounce"]
    klass = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1BouncePad")
    actor = ensure_actor(actors, by_label, "Bounce", klass, spec["at"], (CONFIG["bounce_tag"],))
    actor.set_editor_properties({
        "box_extent": unreal.Vector(*spec["extent"]),
        "launch_velocity_z": spec["launch_velocity_z"],
    })
    display = actor.get_editor_property("display_mesh")
    assert display
    display.set_material(0, mats["bounce"])


def vector_values(value):
    return [float(value.x), float(value.y), float(value.z)]


def rotation_values(value):
    return [float(value.pitch), float(value.yaw), float(value.roll)]


def assert_components(actual, expected, description):
    assert len(actual) == len(expected) and all(
        abs(float(a) - float(b)) <= 0.05 for a, b in zip(actual, expected)
    ), description + ": expected " + str(expected) + ", got " + str(actual)


def expected_transform(role):
    position = {
        "Hazard": CONFIG["hazard"]["at"],
        "HazardCue": CONFIG["hazard_cue"]["at"],
        "Switch": CONFIG["switch"]["at"],
        "Bounce": CONFIG["bounce"]["at"],
        "Checkpoint": CONFIG["checkpoint"]["at"],
        "Start": CONFIG["spawn"],
        "KeyLight": [0.0, 0.0, 800.0],
        "SkyLight": [0.0, 0.0, 600.0],
        "Atmosphere": [0.0, 0.0, 0.0],
    }.get(role)
    scale = [1.0, 1.0, 1.0]
    if role.startswith("Terrain_"):
        block = next(item for item in CONFIG["terrain"] if "Terrain_" + item["id"] == role)
        position = [(block["left"] + block["right"]) / 2.0, 0.0,
                    block["top"] - block["thickness"] / 2.0]
        scale = [(block["right"] - block["left"]) / 100.0,
                 block["depth"] / 100.0, block["thickness"] / 100.0]
    elif role == "HazardCue":
        scale = CONFIG["hazard_cue"]["scale"]
    assert position is not None, "No expected transform for " + role
    rotation = [-45.0, -30.0, 0.0] if role == "KeyLight" else [0.0, 0.0, 0.0]
    return position, rotation, scale


def verify_saved_semantics(final):
    transforms = {}
    for actor_label, actor in sorted(final.items()):
        role = actor_label[len(PREFIX):]
        position, rotation, scale = expected_transform(role)
        actual_position = vector_values(actor.get_actor_location())
        actual_rotation = rotation_values(actor.get_actor_rotation())
        actual_scale = vector_values(actor.get_actor_scale3d())
        assert_components(actual_position, position, actor_label + " location")
        assert_components(actual_rotation, rotation, actor_label + " rotation")
        assert_components(actual_scale, scale, actor_label + " scale")
        transforms[actor_label] = {
            "location": actual_position, "rotation": actual_rotation, "scale": actual_scale,
        }

    hazard = final[label("Hazard")]
    checkpoint = final[label("Checkpoint")]
    switch = final[label("Switch")]
    bounce = final[label("Bounce")]
    for role, actor, spec, kind in (
        ("Hazard", hazard, CONFIG["hazard"], "Hazard"),
        ("Checkpoint", checkpoint, CONFIG["checkpoint"], "Checkpoint"),
    ):
        assert actor.get_editor_property("trigger_kind") == resolve_trigger_kind(kind), role + " kind"
        assert str(actor.get_editor_property("trigger_id")) == spec["id"], role + " ID"
        assert_components(vector_values(actor.get_editor_property("box_extent")),
                          spec["extent"], role + " trigger extent")
    assert_components(vector_values(checkpoint.get_editor_property("respawn_offset")),
                      CONFIG["checkpoint"]["respawn_offset"], "checkpoint respawn offset")
    for property_name, config_key in (
        ("dock_left_x", "dock_left_x"), ("dock_right_x", "dock_right_x"),
        ("left_switch_x", "left_switch_x"), ("right_switch_x", "right_switch_x"),
        ("platform_top_z", "top_z"),
        ("travel_speed_cm_per_second", "travel_speed_cm_per_second"),
        ("stable_boarding_seconds", "stable_boarding_seconds"),
        ("departure_cue_seconds", "departure_cue_seconds"),
    ):
        assert abs(float(switch.get_editor_property(property_name))
                   - float(CONFIG["switch"][config_key])) <= 0.001, property_name
    assert_components(vector_values(switch.get_editor_property("platform_extent")),
                      CONFIG["switch"]["extent"], "switch platform extent")
    assert_components(vector_values(bounce.get_editor_property("box_extent")),
                      CONFIG["bounce"]["extent"], "bounce extent")
    assert abs(float(bounce.get_editor_property("launch_velocity_z"))
               - float(CONFIG["bounce"]["launch_velocity_z"])) <= 0.001
    return {
        "transform_reconciliation": "all_owned_actors_match_config_after_save",
        "verified_actor_count": len(transforms),
        "actor_transforms": transforms,
        "hazard_id": str(hazard.get_editor_property("trigger_id")),
        "checkpoint_id": str(checkpoint.get_editor_property("trigger_id")),
        "switch_docks_x": [float(switch.get_editor_property("dock_left_x")),
                           float(switch.get_editor_property("dock_right_x"))],
        "bounce_launch_velocity_z": float(bounce.get_editor_property("launch_velocity_z")),
    }


def build_map(levels, actors, settings, by_label, existed, mats):
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    assert cube, "Engine cube missing"
    for block in CONFIG["terrain"]:
        configure_terrain(actors, by_label, cube, mats, block)
    configure_trigger(actors, by_label, mats, "Hazard", CONFIG["hazard"],
                      "Hazard", CONFIG["hazard_tag"], "hazard")
    configure_cue(actors, by_label, cube, mats)
    configure_switch(actors, by_label, mats)
    configure_bounce(actors, by_label, mats)
    configure_trigger(actors, by_label, mats, "Checkpoint", CONFIG["checkpoint"],
                      "Checkpoint", CONFIG["checkpoint_tag"], "checkpoint")
    ensure_actor(actors, by_label, "Start", unreal.PlayerStart, CONFIG["spawn"])
    light = ensure_actor(actors, by_label, "KeyLight", unreal.DirectionalLight,
                         [0.0, 0.0, 800.0])
    # Unreal's Python Rotator positional arguments are roll, pitch, yaw.
    light.set_actor_rotation(unreal.Rotator(0.0, -45.0, -30.0), False)
    component = light.get_component_by_class(unreal.DirectionalLightComponent)
    assert component
    component.set_editor_property("intensity", 3.0)
    component.set_editor_property("atmosphere_sun_light", True)
    ensure_actor(actors, by_label, "SkyLight", unreal.SkyLight, [0.0, 0.0, 600.0])
    ensure_actor(actors, by_label, "Atmosphere", unreal.SkyAtmosphere, [0.0, 0.0, 0.0])

    settings.set_editor_property("kill_z", CONFIG["kill_z"])
    tags = [str(item) for item in settings.get_editor_property("tags")]
    tags = [item for item in tags if not item.startswith("NaiwaP1IntegratedCfg_")]
    assert MAP_TAG in tags and PARTIAL_TAG in tags
    settings.set_editor_property("tags", [unreal.Name(item) for item in tags + [CONFIG_TAG]])
    assert levels.save_current_level(), "Could not save integrated teaching slice"

    final = {}
    for actor in actors.get_all_level_actors():
        if is_engine_system_actor(actor):
            continue
        actor_label = actor.get_actor_label()
        assert actor_label in expected_labels(), "Unknown actor appeared: " + actor_label
        assert actor_label not in final, "Duplicate actor after save: " + actor_label
        validate_actor(actor_label[len(PREFIX):], actor)
        final[actor_label] = actor
    assert set(final) == expected_labels(), "Incomplete teaching slice: " + str(sorted(expected_labels() - set(final)))
    assert settings.get_editor_property("kill_z") == -1000.0
    semantics = verify_saved_semantics(final)
    return {
        "map": MAP,
        "status": "existing_partial_slice_reconciled" if existed else "new_partial_slice_saved",
        "phase": CONFIG["phase"],
        "scope": "hazard_cue_switch_bounce_checkpoint_only",
        "owned_actors": sorted(final),
        "kill_z": settings.get_editor_property("kill_z"),
        "game_mode": settings.get_editor_property("default_game_mode").get_path_name(),
        "saved_semantics": semantics,
    }


report = {
    "schema": CONFIG["schema"],
    "target_map": MAP,
    "phase": CONFIG["phase"],
    "config_sha256": CONFIG_HASH,
    "engine": unreal.SystemLibrary.get_engine_version(),
    "status": "running",
    "acceptance": "not_tested_partial_teaching_slice_only",
}
try:
    validate_layout()
    before = protected_hashes()
    mode_class = validate_game_mode()
    levels, actors, settings, by_label, existed = load_and_preflight(mode_class)
    mats = {name: material(name, rgb) for name, rgb in COLORS.items()}
    report["assets"] = {name: mat.get_path_name() for name, mat in mats.items()}
    report["layout"] = build_map(levels, actors, settings, by_label, existed, mats)
    after = protected_hashes()
    assert after == before, "A protected P0/P1 map or Blueprint changed"
    report["protected_sha256"] = after
    report["status"] = "saved_partial_teaching_slice_pending_runtime_and_visual_review"
    unreal.log("NAIWA_P1_INTEGRATED_TEACHING_SLICE_BUILD_OK " + MAP)
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    unreal.log_error(report["error"])
    raise
finally:
    OUTPUT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
