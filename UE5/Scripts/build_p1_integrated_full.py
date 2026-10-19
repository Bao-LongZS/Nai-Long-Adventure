"""Extend the exact saved 12-actor teaching slice; a saved map is not acceptance.

Deploy at UE5/Scripts together with the full JSON; the wrapper belongs at UE5/.
No P0/old-map/prototype asset is edited. Unknown or drifted content is refused
before material creation or actor writes. The original 12 actors remain untouched.
"""

import hashlib
import json
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
CONFIG_PATH = Path(__file__).with_name("p1_integrated_full_config_v1.json")
CONFIG = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
BASE = CONFIG["base_teaching_slice"]
MAP = CONFIG["map"]
ASSET_ROOT = CONFIG["asset_root"]
OWNER_TAG = BASE["owner_tag"]
MAP_TAG = BASE["map_tag"]
PARTIAL_TAG = "NaiwaP1PartialTeachingSliceV1"
FULL_TAG = "NaiwaP1IntegratedFullCandidateV1"
PREFIX = "P1_Integrated_"
CONFIG_HASH = hashlib.sha256(CONFIG_PATH.read_bytes()).hexdigest()
CONFIG_TAG = "NaiwaP1IntegratedFullCfg_" + CONFIG_HASH[:16]
BASE_CONFIG_TAG = "NaiwaP1IntegratedCfg_" + CONFIG["base_config_sha256"][:16]
EXPECTED_BASE_CONFIG_SHA256 = "391cec36cc1504177bd911a11bbada4e2edb8f4383a361e10fd59be12370e7b8"
EXPECTED_BASE_CANONICAL_SHA256 = "73bb6c807e6560ccf0a1da16993ec1aacf96ee12021efae149831e6efdf5a916"
OUTPUT = PROJECT / "Saved/NaiwaP1/integrated_full_build.json"
MAP_FILE = PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Integrated.umap"
PROTECTED = [PROJECT / item for item in CONFIG["protected_packages"]]
COLORS = {
    "right_bank": (0.10, 0.41, 0.41), "landing": (0.08, 0.36, 0.56),
    "hazard": (0.94, 0.11, 0.16), "cue": (1.0, 0.56, 0.04),
    "switch_platform": (0.96, 0.66, 0.12), "left_switch": (0.90, 0.28, 0.24),
    "right_switch": (0.21, 0.70, 0.68), "bounce": (0.98, 0.48, 0.08),
    "checkpoint": (0.12, 0.85, 0.35), "timed": (0.66, 0.38, 0.90),
    "pickup": (1.0, 0.86, 0.14), "goal": (0.18, 0.84, 0.90),
}
CLASS_PATHS = {
    "Terrain": "/Script/Engine.StaticMeshActor", "Cue": "/Script/Engine.StaticMeshActor",
    "Hazard": "/Script/NaiwaRuntime.NaiwaP1Trigger",
    "Checkpoint": "/Script/NaiwaRuntime.NaiwaP1Trigger",
    "Pickup": "/Script/NaiwaRuntime.NaiwaP1Trigger", "Goal": "/Script/NaiwaRuntime.NaiwaP1Trigger",
    "Switch": "/Script/NaiwaRuntime.NaiwaP1SwitchPlatform",
    "Bounce": "/Script/NaiwaRuntime.NaiwaP1BouncePad",
    "Timed": "/Script/NaiwaRuntime.NaiwaP1TimedPlatform",
    "Start": "/Script/Engine.PlayerStart", "KeyLight": "/Script/Engine.DirectionalLight",
    "SkyLight": "/Script/Engine.SkyLight", "Atmosphere": "/Script/Engine.SkyAtmosphere",
}


def base_specs():
    result = {"Terrain_" + item["id"]: dict(item, kind="Terrain", tag=BASE["camera_terrain_tag"])
              for item in BASE["terrain"]}
    for role, key, kind, tag_key in (
        ("Hazard", "hazard", "Hazard", "hazard_tag"),
        ("HazardCue", "hazard_cue", "Cue", "cue_tag"),
        ("Switch", "switch", "Switch", "switch_tag"),
        ("Bounce", "bounce", "Bounce", "bounce_tag"),
        ("Checkpoint", "checkpoint", "Checkpoint", "checkpoint_tag"),
    ):
        result[role] = dict(BASE[key], kind=kind, tag=BASE[tag_key])
    for role, position in (("Start", BASE["spawn"]), ("KeyLight", [0.0, 0.0, 800.0]),
                           ("SkyLight", [0.0, 0.0, 600.0]), ("Atmosphere", [0.0, 0.0, 0.0])):
        result[role] = {"kind": role, "at": position}
    return result


def all_specs():
    return dict(base_specs(), **{item["role"]: item for item in CONFIG["extension_actors"]})


def validate_layout():
    assert CONFIG["schema"] == "NaiwaP1IntegratedFullV1"
    assert CONFIG["phase"] == "P1-5b_full_candidate"
    assert MAP == BASE["map"] == "/Game/Naiwa/P1/Maps/L_NaiwaP1_Integrated"
    assert ASSET_ROOT == "/Game/Naiwa/P1"
    assert BASE["kill_z"] == CONFIG["kill_z"] == -1000.0
    assert BASE["spawn"] == [-1450.0, 0.0, 212.0]
    assert CONFIG["required_pickup_count"] == 1
    assert CONFIG["base_config_sha256"] == EXPECTED_BASE_CONFIG_SHA256
    assert hashlib.sha256(json.dumps(BASE, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
                          .encode("utf-8")).hexdigest() == EXPECTED_BASE_CANONICAL_SHA256
    base = base_specs()
    extension = CONFIG["extension_actors"]
    assert len(base) == 12 and len(extension) == 13
    assert len({item["role"] for item in extension}) == len(extension)
    assert not set(base).intersection(item["role"] for item in extension)
    assert len({item["id"] for item in extension}) == len(extension)
    assert len({item["tag"] for item in extension}) == len(extension)
    assert not {item["tag"] for item in extension}.intersection(
        item.get("tag") for item in base.values())
    ids = [item["id"] for item in extension if item["kind"] in ("Hazard", "Checkpoint", "Pickup", "Goal")]
    ids += [BASE["hazard"]["id"], BASE["checkpoint"]["id"]]
    assert len(set(ids)) == len(ids)
    for item in extension:
        assert item["kind"] in CLASS_PATHS and item["material"] in COLORS
        assert item["tag"].startswith("NaiwaP1Integrated")
        assert all(value > 0.0 for value in item.get("extent", []))
        if item["kind"] == "Terrain":
            assert item["left"] < item["right"] and item["depth"] >= 400.0
            assert item["thickness"] > 0.0
        else:
            assert len(item["at"]) == 3 and item["at"][1] == 0.0
    specs = all_specs()
    teaching = specs["TimedTeaching"]
    assert teaching["at"] == [3340.0, 0.0, 0.0] and teaching["top_z"] == 180.0
    assert teaching["extent"] == [210.0, 160.0, 20.0]
    assert teaching["active_seconds"] >= 4.3 and teaching["stable_landing_seconds"] == 0.18
    assert specs["Terrain_TimedExit"]["left"] == 3560.0
    assert BASE["terrain"][2]["right"] == 3120.0
    switch = specs["SwitchCombination"]
    assert switch["at"] == [5050.0, 0.0, 60.0] and switch["top_z"] == 120.0
    assert [switch["dock_left_x"], switch["dock_right_x"]] == [-420.0, 420.0]
    assert switch["extent"] == [120.0, 160.0, 20.0]
    assert 5050.0 - 420.0 - 120.0 - specs["Terrain_TimedExit"]["right"] == 10.0
    assert specs["Terrain_CombinationBank"]["left"] - (5050.0 + 420.0 + 120.0) == 10.0
    assert specs["HazardCombination"]["at"][2] - specs["HazardCombination"]["extent"][2] == 180.0
    assert specs["HazardCueCombination"]["at"][0] < specs["HazardCombination"]["at"][0]
    pad, timed, bank = (specs[key] for key in ("BounceCombination", "TimedCombination", "Terrain_FinalBank"))
    assert pad["at"] == [6700.0, 0.0, 60.0] and pad["extent"] == [160.0, 120.0, 12.0]
    assert pad["launch_velocity_z"] == 1000.0
    assert specs["Terrain_CombinationBank"]["right"] == 6500.0
    assert timed["at"] == [6990.0, 0.0, 0.0] and timed["top_z"] == 360.0
    assert timed["extent"] == [210.0, 160.0, 20.0] and timed["active_seconds"] >= 4.3
    assert bank["left"] == 7210.0 and bank["top"] == 360.0
    pickup = specs["PickupRequired"]
    assert pickup["at"][0] < specs["Goal"]["at"][0] < bank["right"] - 100.0
    for checkpoint in [base["Checkpoint"], specs["CheckpointCombination"], specs["CheckpointFinal"]]:
        safe_banks = [item for item in specs.values() if item["kind"] == "Terrain"
                      and item["left"] + 150.0 < checkpoint["at"][0] < item["right"] - 150.0
                      and abs(checkpoint["at"][2] - 92.0 - item["top"]) < 0.01]
        assert len(safe_banks) == 1, "Checkpoint needs one static safe bank"
        assert checkpoint["respawn_offset"] == [0.0, 0.0, 0.0]
        assert checkpoint["at"][0] < pickup["at"][0], "Pickup must remain ahead of every checkpoint"
    assert len(PROTECTED) == 11 and MAP_FILE not in PROTECTED


def design_metrics():
    return {
        "timed_teaching_walk150_gap_cm": 440.0,
        "timed_teaching_walk150_constant_speed_seconds": 440.0 / 150.0,
        "prototype_walk150_trigger_to_right_bank_observed_seconds": 3.367,
        "timed_teaching_active_seconds": 4.3,
        "estimated_margin_against_prototype_seconds": round(4.3 - 3.367, 3),
        "timed_combination_full_width_to_safe_bank_cm": 430.0,
        "timed_combination_walk150_constant_speed_seconds": 430.0 / 150.0,
        "timed_combination_active_seconds": 4.3,
        "timed_combination_actual_trigger_x_and_exit_seconds": "pending_new_map_Walk_Run_runtime_measurement",
        "combination_heights_cm": {"front_bank": 180.0, "pad_top": 72.0, "timed_and_exit": 360.0,
                                    "prior_normal_jump_feet_peak_estimate": 323.0,
                                    "prior_bounce_feet_peak_estimate": 436.0},
        "risks": CONFIG["design_risks"],
        "acceptance": "design_estimates_only_no_route_or_visual_result",
    }


def hashes():
    result = {}
    for path in PROTECTED:
        assert path.is_file(), "Protected asset missing: " + str(path)
        result[str(path.relative_to(PROJECT)).replace("\\", "/")] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def tags(actor):
    return {str(item) for item in actor.get_editor_property("tags")}


def values(vector):
    return [float(vector.x), float(vector.y), float(vector.z)]


def equal(actual, expected, description):
    assert len(actual) == len(expected) and all(abs(float(a) - float(b)) <= 0.05
                                               for a, b in zip(actual, expected)), description + ": " + str(actual)


def transform(spec):
    if spec["kind"] == "Terrain":
        position = [(spec["left"] + spec["right"]) / 2.0, 0.0, spec["top"] - spec["thickness"] / 2.0]
        scale = [(spec["right"] - spec["left"]) / 100.0, spec["depth"] / 100.0, spec["thickness"] / 100.0]
    else:
        position, scale = spec["at"], spec.get("scale", [1.0, 1.0, 1.0])
    rotation = [-45.0, -30.0, 0.0] if spec["kind"] == "KeyLight" else [0.0, 0.0, 0.0]
    return position, rotation, scale


def trigger_kind(kind):
    enum = getattr(unreal, "NaiwaP1TriggerKind", None) or getattr(unreal, "ENaiwaP1TriggerKind", None)
    assert enum
    names = [name for name in dir(enum) if name.replace("_", "").lower() == kind.lower()]
    assert len(names) == 1
    return getattr(enum, names[0])


def parameters(spec):
    kind = spec["kind"]
    if kind in ("Hazard", "Checkpoint", "Pickup", "Goal"):
        result = {"trigger_kind": trigger_kind(kind), "trigger_id": unreal.Name(spec["id"]),
                  "box_extent": unreal.Vector(*spec["extent"])}
        if kind == "Checkpoint":
            result["respawn_offset"] = unreal.Vector(*spec["respawn_offset"])
        return result
    if kind == "Bounce":
        return {"box_extent": unreal.Vector(*spec["extent"]), "launch_velocity_z": spec["launch_velocity_z"]}
    if kind == "Switch":
        return dict({key: spec[key] for key in ("dock_left_x", "dock_right_x", "left_switch_x", "right_switch_x",
                                               "travel_speed_cm_per_second", "stable_boarding_seconds", "departure_cue_seconds")},
                    platform_top_z=spec["top_z"], platform_extent=unreal.Vector(*spec["extent"]))
    if kind == "Timed":
        return {"platform_top_z": spec["top_z"], "platform_extent": unreal.Vector(*spec["extent"]),
                **{key: spec[key] for key in ("stable_landing_seconds", "active_seconds", "disabled_seconds")}}
    return {}


def verify(actor, role, spec, require_extension_id=False):
    actor_label = PREFIX + role
    assert actor.get_actor_label() == actor_label
    assert actor.get_class().get_path_name() == CLASS_PATHS[spec["kind"]], "Class drift: " + actor_label
    assert OWNER_TAG in tags(actor) and (not spec.get("tag") or spec["tag"] in tags(actor)), "Ownership drift: " + actor_label
    if require_extension_id:
        assert "NaiwaP1IntegratedStableId_" + spec["id"] in tags(actor), "Stable ID tag drift: " + actor_label
    position, rotation, scale = transform(spec)
    equal(values(actor.get_actor_location()), position, actor_label + " location")
    actual_rotation = actor.get_actor_rotation()
    equal([actual_rotation.pitch, actual_rotation.yaw, actual_rotation.roll], rotation, actor_label + " rotation")
    equal(values(actor.get_actor_scale3d()), scale, actor_label + " scale")
    for key, expected in parameters(spec).items():
        actual = actor.get_editor_property(key)
        if key in ("box_extent", "platform_extent", "respawn_offset"):
            equal(values(actual), values(expected), actor_label + " " + key)
        elif isinstance(expected, (float, int)):
            assert abs(float(actual) - float(expected)) <= 0.001, actor_label + " " + key
        else:
            assert actual == expected, actor_label + " " + key
    if spec["kind"] in ("Terrain", "Cue"):
        mesh = actor.get_component_by_class(unreal.StaticMeshComponent)
        assert mesh and mesh.get_editor_property("static_mesh").get_path_name() == "/Engine/BasicShapes/Cube.Cube"
        collision = mesh.get_collision_enabled()
        if spec["kind"] == "Cue":
            assert collision == unreal.CollisionEnabled.NO_COLLISION, actor_label + " cue must not be a step"
        else:
            assert BASE["camera_terrain_tag"] in tags(actor), actor_label + " camera terrain tag"
            assert collision in (unreal.CollisionEnabled.QUERY_AND_PHYSICS, unreal.CollisionEnabled.QUERY_ONLY), actor_label + " bank collision"
    return {"location": values(actor.get_actor_location()), "rotation": rotation,
            "scale": values(actor.get_actor_scale3d()), "kind": spec["kind"], "id": spec.get("id", role)}


def game_mode():
    bp = unreal.load_asset(ASSET_ROOT + "/BP_NaiwaP1GameMode")
    assert bp and isinstance(bp, unreal.Blueprint)
    klass = unreal.BlueprintEditorLibrary.generated_class(bp)
    defaults = unreal.get_default_object(klass)
    assert "BP_NaiwaP1Character" in defaults.get_editor_property("default_pawn_class").get_path_name()
    assert defaults.get_editor_property("player_controller_class").get_path_name() == "/Script/NaiwaRuntime.NaiwaP1PlayerController"
    assert int(defaults.get_editor_property("required_pickup_count")) == CONFIG["required_pickup_count"]
    for kind in ("Hazard", "Switch", "Bounce", "Timed"):
        assert unreal.load_class(None, CLASS_PATHS[kind]), "Native mechanism unavailable: " + kind
    return klass


def collect(actors):
    result = {}
    for actor in actors.get_all_level_actors():
        if actor.get_class().get_path_name() in ("/Script/Engine.WorldSettings", "/Script/Engine.LevelBounds"):
            continue
        name = actor.get_actor_label()
        assert name.startswith(PREFIX), "Unknown actor: " + name
        role = name[len(PREFIX):]
        assert role not in result, "Duplicate actor label: " + name
        result[role] = actor
    return result


def preflight(mode_class):
    assert unreal.EditorAssetLibrary.does_asset_exist(MAP), "Only an existing saved teaching slice may be extended"
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assert levels.load_level(MAP)
    settings = unreal.EditorLevelLibrary.get_editor_world().get_world_settings()
    markers = tags(settings)
    assert MAP_TAG in markers and BASE_CONFIG_TAG in markers, "Baseline slice marker/config drift"
    assert settings.get_editor_property("kill_z") == -1000.0
    assert settings.get_editor_property("default_game_mode").get_path_name() == mode_class.get_path_name()
    previous_full = FULL_TAG in markers
    if previous_full:
        assert markers == {MAP_TAG, BASE_CONFIG_TAG, FULL_TAG, CONFIG_TAG}, "Full candidate marker/config drift"
        expected = all_specs()
    else:
        assert markers == {MAP_TAG, BASE_CONFIG_TAG, PARTIAL_TAG}, "Partial candidate marker/config drift"
        expected = base_specs()
    found = collect(actors)
    assert set(found) == set(expected), "Unknown/missing actor(s): " + str(sorted(set(found).symmetric_difference(expected)))
    for role, spec in expected.items():
        verify(found[role], role, spec, role not in base_specs())
    return levels, actors, settings, found, previous_full


def material(name):
    # New materials are namespaced to this full candidate; existing base materials remain untouched.
    asset_name = "M_P1_IntegratedFull_" + name
    path = ASSET_ROOT + "/Materials/" + asset_name
    result = unreal.load_asset(path)
    if result:
        assert isinstance(result, unreal.Material)
        return result
    result = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, ASSET_ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    assert result
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        result, unreal.MaterialExpressionConstant3Vector, -350, 0)
    expression.set_editor_property("constant", unreal.LinearColor(*COLORS[name], 1.0))
    assert unreal.MaterialEditingLibrary.connect_material_property(expression, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert not unreal.MaterialEditingLibrary.recompile_material(result)
    assert unreal.EditorAssetLibrary.save_loaded_asset(result)
    return result


def extend(actors, found, mats):
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    assert cube
    for spec in CONFIG["extension_actors"]:
        role = spec["role"]
        actor = found.get(role)
        if actor is None:
            klass = unreal.load_class(None, CLASS_PATHS[spec["kind"]])
            position, rotation, scale = transform(spec)
            actor = actors.spawn_actor_from_class(klass, unreal.Vector(*position))
            assert actor
            actor.set_actor_label(PREFIX + role)
            role_tags = [OWNER_TAG, spec["tag"], "NaiwaP1IntegratedStableId_" + spec["id"]]
            if spec["kind"] == "Terrain":
                role_tags.append(BASE["camera_terrain_tag"])
            actor.set_editor_property("tags", [unreal.Name(item) for item in role_tags])
            # Positional Rotator arguments are roll,pitch,yaw. All extensions are level.
            actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
            actor.set_actor_scale3d(unreal.Vector(*scale))
            actor.set_editor_properties(parameters(spec))
            if spec["kind"] in ("Terrain", "Cue"):
                mesh = actor.get_component_by_class(unreal.StaticMeshComponent)
                mesh.set_static_mesh(cube)
                mesh.set_material(0, mats[spec["material"]])
                if spec["kind"] == "Cue":
                    # Persist the no-step contract across save/reload, including profile restoration.
                    mesh.set_collision_profile_name("NoCollision")
                mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION if spec["kind"] == "Cue"
                                           else unreal.CollisionEnabled.QUERY_AND_PHYSICS)
            elif spec["kind"] == "Switch":
                for component, color in (("platform_display", "switch_platform"),
                                         ("left_switch_display", "left_switch"), ("right_switch_display", "right_switch")):
                    actor.get_editor_property(component).set_material(0, mats[color])
            else:
                component = "platform_display" if spec["kind"] == "Timed" else "display_mesh"
                actor.get_editor_property(component).set_material(0, mats[spec["material"]])
            found[role] = actor
        verify(actor, role, spec, True)


report = {"schema": CONFIG["schema"], "target_map": MAP, "phase": CONFIG["phase"],
          "config_sha256": CONFIG_HASH, "builder_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
          "engine": unreal.SystemLibrary.get_engine_version(), "status": "running",
          "acceptance": "pending_new_map_route_checkpoint_and_visual_review"}
before = None
try:
    validate_layout()
    report["design_metrics"] = design_metrics()
    before = hashes()
    report["protected_sha256_before"] = before
    assert MAP_FILE.is_file()
    report["target_map_sha256_before"] = hashlib.sha256(MAP_FILE.read_bytes()).hexdigest()
    levels, actors, settings, found, previous_full = preflight(game_mode())
    # Every refusal above precedes material/actor writes. Never reconfigure the original 12 actors.
    original = {role: verify(found[role], role, spec) for role, spec in base_specs().items()}
    mats = {name: material(name) for name in COLORS}
    extend(actors, found, mats)
    found = collect(actors)
    assert set(found) == set(all_specs())
    for role, spec in base_specs().items():
        assert verify(found[role], role, spec) == original[role], "Original teaching actor changed"
    markers = tags(settings)
    markers.discard(PARTIAL_TAG)
    markers.add(FULL_TAG)
    markers.add(CONFIG_TAG)
    settings.set_editor_property("tags", [unreal.Name(item) for item in sorted(markers)])
    assert levels.save_current_level(), "Could not save full candidate map"
    # Read the saved map back rather than treating in-memory assignments as saved proof.
    assert levels.load_level(MAP)
    settings = unreal.EditorLevelLibrary.get_editor_world().get_world_settings()
    found = collect(actors)
    assert set(found) == set(all_specs())
    semantics = {PREFIX + role: verify(found[role], role, spec, role not in base_specs())
                 for role, spec in all_specs().items()}
    assert tags(settings).issuperset({MAP_TAG, BASE_CONFIG_TAG, FULL_TAG, CONFIG_TAG})
    assert PARTIAL_TAG not in tags(settings) and settings.get_editor_property("kill_z") == -1000.0
    assert settings.get_editor_property("default_game_mode").get_path_name() == game_mode().get_path_name()
    assert hashes() == before, "Protected P0/old-map/prototype/Blueprint package changed"
    report["layout"] = {"status": "full_candidate_idempotent_verified" if previous_full else "partial_slice_extended_to_full_candidate",
                        "owned_actors": sorted(semantics), "verified_actor_count": len(semantics),
                        "original_actor_count_preserved": 12, "kill_z": -1000.0,
                        "saved_semantics": {"transform_reconciliation": "all_owned_actors_match_config_after_save_reload",
                                            "actor_transforms": semantics},
                        "required_pickup_count": 1}
    report["target_map_sha256_after"] = hashlib.sha256(MAP_FILE.read_bytes()).hexdigest()
    report["target_map_bytes"] = MAP_FILE.stat().st_size
    report["status"] = "saved_full_candidate_pending_route_and_visual_review"
    unreal.log("NAIWA_P1_INTEGRATED_FULL_BUILD_OK " + MAP)
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    unreal.log_error(report["error"])
    raise
finally:
    if before is not None:
        report["protected_sha256_after"] = hashes()
        report["protected_packages_unchanged"] = report["protected_sha256_after"] == before
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
