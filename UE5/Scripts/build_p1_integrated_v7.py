"""Build the isolated P1 v7 candidate from the hash-locked integrated map.

The source map and its 14 teaching actors are never edited. A saved map only
proves serialized geometry; A/B/C, route, checkpoint and visual gates follow.
"""

import hashlib
import json
import traceback
from copy import deepcopy
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
CONFIG_PATH = Path(__file__).with_name("p1_integrated_v7_config.json")
SOURCE_CONFIG_PATH = Path(__file__).with_name("p1_integrated_full_config_v1.json")
CONFIG = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
SOURCE = json.loads(SOURCE_CONFIG_PATH.read_text(encoding="utf-8"))
BASE = SOURCE["base_teaching_slice"]
MAP = CONFIG["map"]
SOURCE_MAP = CONFIG["source_map"]
ASSET_ROOT = CONFIG["asset_root"]
CONFIG_HASH = hashlib.sha256(CONFIG_PATH.read_bytes()).hexdigest()
SOURCE_CONFIG_HASH = hashlib.sha256(SOURCE_CONFIG_PATH.read_bytes()).hexdigest()
CONFIG_TAG = "NaiwaP1V7Cfg_" + CONFIG_HASH[:16]
SOURCE_BASE_CONFIG_TAG = "NaiwaP1IntegratedCfg_" + SOURCE["base_config_sha256"][:16]
SOURCE_FULL_CONFIG_TAG = "NaiwaP1IntegratedFullCfg_" + SOURCE_CONFIG_HASH[:16]
SOURCE_FULL_MARKERS = {BASE["map_tag"], SOURCE_BASE_CONFIG_TAG,
                       "NaiwaP1IntegratedFullCandidateV1", SOURCE_FULL_CONFIG_TAG}
V7_MARKERS = {CONFIG["map_tag"], CONFIG_TAG}
SOURCE_PREFIX = "P1_Integrated_"
V7_PREFIX = "P1_V7_"
SOURCE_OWNER_TAG = BASE["owner_tag"]
SOURCE_FILE = PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Integrated.umap"
TARGET_FILE = PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_v7.umap"
OUTPUT = PROJECT / "Saved/NaiwaP1/integrated_v7_build.json"
PROTECTED = [PROJECT / relative for relative in CONFIG["protected_packages"]]
COLORS = {
    "landing": (0.08, 0.36, 0.56), "hazard": (0.94, 0.11, 0.16),
    "switch_platform": (0.96, 0.66, 0.12), "left_switch": (0.90, 0.28, 0.24),
    "right_switch": (0.21, 0.70, 0.68), "bounce": (0.98, 0.48, 0.08),
    "checkpoint": (0.12, 0.85, 0.35), "timed": (0.66, 0.38, 0.90),
    "pickup": (1.0, 0.86, 0.14), "goal": (0.18, 0.84, 0.90),
}
CLASS_PATHS = {
    "Terrain": "/Script/Engine.StaticMeshActor", "Cue": "/Script/Engine.StaticMeshActor",
    "Hazard": "/Script/NaiwaRuntime.NaiwaP1Trigger",
    "Checkpoint": "/Script/NaiwaRuntime.NaiwaP1Trigger",
    "Pickup": "/Script/NaiwaRuntime.NaiwaP1Trigger",
    "Goal": "/Script/NaiwaRuntime.NaiwaP1Trigger",
    "Switch": "/Script/NaiwaRuntime.NaiwaP1SwitchPlatform",
    "Bounce": "/Script/NaiwaRuntime.NaiwaP1BouncePad",
    "Timed": "/Script/NaiwaRuntime.NaiwaP1TimedPlatform",
    "Start": "/Script/Engine.PlayerStart", "KeyLight": "/Script/Engine.DirectionalLight",
    "SkyLight": "/Script/Engine.SkyLight", "Atmosphere": "/Script/Engine.SkyAtmosphere",
}


def source_specs():
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
    result.update({item["role"]: item for item in SOURCE["extension_actors"]})
    return result


SOURCE_SPECS = source_specs()
KEPT_SOURCE = {role: SOURCE_SPECS[role] for role in CONFIG["keep_source_roles"]}
KEPT = deepcopy(KEPT_SOURCE)
for role, override in CONFIG.get("retained_source_overrides", {}).items():
    assert role in KEPT, "Override must target a retained source actor: " + role
    KEPT[role].update(override)
V7 = {item["role"]: item for item in CONFIG["extension_actors"]}


def validate_layout():
    assert CONFIG["schema"] == "NaiwaP1IntegratedV7V1"
    assert CONFIG["phase"] == "P1-5b_v7_candidate"
    assert MAP == "/Game/Naiwa/P1/Maps/L_NaiwaP1_v7"
    assert SOURCE_MAP == SOURCE["map"] == "/Game/Naiwa/P1/Maps/L_NaiwaP1_Integrated"
    assert ASSET_ROOT == "/Game/Naiwa/P1"
    assert CONFIG["source_map_sha256"] == "ee5c82788fa2290089f27f4021f1f8539f9bfa979cf357910a4eabcdde73a350"
    assert CONFIG["source_full_config_sha256"] == "3aad9c28e6a88397aeb6e1652559fb8d60496185fe255633ab259d7e72f2aa5d"
    assert SOURCE_CONFIG_HASH == CONFIG["source_full_config_sha256"]
    assert CONFIG["kill_z"] == SOURCE["kill_z"] == -1000
    assert CONFIG["required_pickup_count"] == 1 and SOURCE["required_pickup_count"] == 1
    assert len(SOURCE_SPECS) == 25 and len(KEPT) == 14 and len(V7) == 59
    assert len(CONFIG["keep_source_roles"]) == len(KEPT)
    assert set(CONFIG["retained_source_overrides"]) == {"Terrain_BounceLanding", "Bounce", "Checkpoint"}
    assert KEPT["Terrain_BounceLanding"]["top"] == 420
    assert KEPT["Bounce"]["launch_velocity_z"] == 1250
    assert KEPT["Checkpoint"]["at"] == [2700, 0, 512]
    assert KEPT["Terrain_BounceLanding"]["top"] - KEPT["Terrain_RightBank"]["top"] > 264.7
    assert set(KEPT) == set(SOURCE_SPECS).difference(
        {"SwitchCombination", "Terrain_CombinationBank", "HazardCueCombination",
         "HazardCombination", "CheckpointCombination", "BounceCombination",
         "TimedCombination", "Terrain_FinalBank", "CheckpointFinal",
         "PickupRequired", "Goal"})
    assert len(CONFIG["extension_actors"]) == len(V7)
    assert len({item["id"] for item in V7.values()}) == len(V7)
    assert len({item["tag"] for item in V7.values()}) == len(V7)
    assert {item["kind"] for item in V7.values()} == {
        "Terrain", "Switch", "Bounce", "Timed", "Checkpoint", "Hazard", "Pickup", "Goal"}
    assert not {item["kind"] for item in V7.values()}.intersection({"Cue", "Start"})
    assert len([item for item in V7.values() if item["kind"] == "Terrain"]) == 23
    assert len([item for item in V7.values() if item["kind"] == "Switch"]) == 3
    assert len([item for item in V7.values() if item["kind"] == "Bounce"]) == 3
    assert len([item for item in V7.values() if item["kind"] == "Timed"]) == 3
    assert len([item for item in V7.values() if item["kind"] == "Checkpoint"]) == 7
    pickup_specs = [item for item in V7.values() if item["kind"] == "Pickup"]
    assert len(pickup_specs) == 4 and sum(bool(item.get("b_required_for_goal")) for item in pickup_specs) == 1
    assert next(item for item in pickup_specs if item.get("b_required_for_goal"))["id"] == "IntegratedRequiredPickup01"
    assert len([item for item in V7.values() if item["kind"] == "Hazard"]) == 15
    assert sum(item["kind"] == "Hazard" and item["b_reveal_on_spawn"] for item in V7.values()) == 11
    assert sum(item["kind"] == "Hazard" and not item["b_reveal_on_spawn"] for item in V7.values()) == 4
    pickups = sorted((item for item in V7.values() if item["kind"] == "Pickup"),
                     key=lambda item: item["pickup_order"])
    assert len(pickups) == 4
    assert [(item["role"], item["id"], item["pickup_order"], item["at"])
            for item in pickups] == [
        ("Pickup01", "IntegratedPickup01", 1, [1000, 0, 182]),
        ("Pickup02", "IntegratedPickup02", 2, [8740, 0, 242]),
        ("Pickup03", "IntegratedPickup03", 3, [14960, 0, 542]),
        ("PickupRequired", "IntegratedRequiredPickup01", 4, [26800, 0, 902]),
    ]
    assert len([item for item in V7.values() if item["kind"] == "Goal"]) == 1
    assert len(PROTECTED) == 12 and SOURCE_FILE in PROTECTED and TARGET_FILE not in PROTECTED
    assert len(set(PROTECTED)) == len(PROTECTED)
    assert CONFIG["show_checkpoint_labels"] is True
    for item in V7.values():
        assert item["kind"] in CLASS_PATHS and item["material"] in COLORS
        assert item["tag"].startswith("NaiwaP1V7")
        if item["kind"] == "Terrain":
            assert item["left"] < item["right"] and item["thickness"] > 0
            assert item["depth"] >= 400
        else:
            assert len(item["at"]) == 3 and item["at"][1] == 0
            assert len(item["extent"]) == 3 and all(n > 0 for n in item["extent"])
    assert [item["id"] for item in CONFIG["gap_chains"]] == ["A-4", "B-5", "C-5"]
    assert [len(item["gaps"]) for item in CONFIG["gap_chains"]] == [3, 3, 5]
    assert [item["top"] for item in CONFIG["gap_chains"]] == [180, 480, 240]
    assert [item["id"] for item in CONFIG["checkpoints"]] == [
        "IntegratedCheckpoint%02d" % number for number in range(1, 9)]
    assert [item["x"] for item in CONFIG["checkpoints"]] == [
        2700, 4700, 7700, 10370, 13770, 16640, 19760, 24080]
    assert [item["top"] for item in CONFIG["checkpoints"]] == [420, 180, 180, 180, 480, 240, 240, 240]
    assert CONFIG["checkpoints"][0]["role"] == "Checkpoint"
    assert CONFIG["checkpoints"][0]["source"] == "retained_teaching"
    assert [item["role"] for item in CONFIG["checkpoints"][1:]] == [
        "Checkpoint%02d" % number for number in range(2, 9)]
    assert len({item["id"] for item in CONFIG["checkpoints"]}) == 8
    for checkpoint in CONFIG["checkpoints"]:
        spec = KEPT["Checkpoint"] if checkpoint["source"] == "retained_teaching" else V7[checkpoint["role"]]
        assert spec["kind"] == "Checkpoint" and spec["id"] == checkpoint["id"]
        assert spec["at"] == [checkpoint["x"], 0, checkpoint["top"] + 92]
        assert spec["respawn_offset"] == [0, 0, 0]
        banks = [item for item in list(KEPT.values()) + list(V7.values())
                 if item["kind"] == "Terrain" and item["top"] == checkpoint["top"]
                 and item["left"] + 100 <= checkpoint["x"] <= item["right"] - 100]
        assert len(banks) == 1, "Checkpoint lacks exactly one safe static bank: " + checkpoint["id"]
        assert checkpoint["x"] < V7["PickupRequired"]["at"][0]
    expected_gaps = {
        "A-4": [[8300, 8540], [8940, 9180], [9580, 9820]],
        "B-5": [[14520, 14760], [15160, 15400], [15800, 16040]],
        "C-5": [[20660, 20900], [21300, 21540], [21940, 22180],
                [22580, 22820], [23220, 23460]],
    }
    for chain in CONFIG["gap_chains"]:
        assert chain["gaps"] == expected_gaps[chain["id"]]
        hazards = [item for item in V7.values()
                   if item["kind"] == "Hazard" and item.get("gap_chain") == chain["id"]]
        assert len(hazards) == len(chain["gaps"])
        for index, (left, right) in enumerate(chain["gaps"], 1):
            assert right - left == 240
            assert (index == 1 or left - chain["gaps"][index - 2][1] == 400)
            matches = [item for item in hazards if item["gap_index"] == index]
            assert len(matches) == 1
            spike = matches[0]
            assert spike["b_reveal_on_spawn"] is True
            assert spike["b_use_spike_cluster"] is True
            assert spike["at"] == [(left + right) / 2, 0, chain["top"] - 90]
            assert spike["extent"] == [120, 160, 50]
    # The only gap widths counted as Walk-fail gates are intentional route gaps.
    assert V7["SwitchA"]["dock_right_x"] == 940
    assert V7["SwitchA"]["at"][0] + 940 + 120 == 7500 - 240
    assert V7["SwitchB"]["at"][0] + 1180 + 120 == 13120
    assert V7["BounceB"]["at"][0] - V7["BounceB"]["extent"][0] == 13180
    assert V7["BounceB"]["at"][2] + V7["BounceB"]["extent"][2] == 180
    assert V7["BounceB"]["launch_velocity_z"] == 1150
    assert V7["SwitchC"]["at"][0] + 870 + 120 == 18830
    assert V7["SwitchC"]["dock_left_top_z"] == 240
    assert V7["SwitchC"]["dock_right_top_z"] == 340
    for role in ("SwitchA", "SwitchB", "SwitchC"):
        assert V7[role]["transit_trap_min_count"] == 1
        assert V7[role]["transit_trap_max_count"] == 2
        assert V7[role]["transit_trap_seed"] > 0
        assert V7[role]["travel_speed_cm_per_second"] == 300
        assert V7[role]["travel_speed_variation_cm_per_second"] == 100
    assert V7["TimedC"]["at"] == [19050, 0, 0]
    assert V7["TimedC"]["top_z"] == 340 and V7["TimedC"]["active_seconds"] == 1.36
    assert V7["TimedC"]["stable_landing_seconds"] == 0.18
    assert CONFIG["timed_c_design"] == {
        "role": "mandatory_timed_transition",
        "run_only_gate": False,
        "walk_landing_after_disable_allowed": True,
    }, "C may remain mandatory even when Walk lands after its timer expires"
    assert V7["Terrain_FinalApproach"]["left"] == 23860 and V7["Terrain_FinalApproach"]["right"] == 24340
    assert V7["Terrain_FinalApproach"]["top"] == 240
    assert V7["Terrain_FinalIntermediate"]["left"] == 25130
    assert V7["Terrain_FinalIntermediate"]["right"] == 25440
    assert V7["Terrain_FinalIntermediate"]["top"] == 540
    assert V7["Terrain_Final"]["left"] == 26040 and V7["Terrain_Final"]["right"] == 28200
    assert V7["Terrain_Final"]["top"] == 840
    assert V7["Checkpoint08"]["at"] == [24080, 0, 332]
    assert V7["BounceFinal"]["at"] == [24420, 0, 188]
    assert V7["BounceFinal"]["at"][2] + V7["BounceFinal"]["extent"][2] == 200
    assert V7["BounceFinal"]["launch_velocity_z"] == 1100
    assert V7["TimedFinal"]["top_z"] == 540 and V7["TimedFinal"]["active_seconds"] == 0.60
    assert V7["TimedFinal"]["stable_landing_seconds"] == 0.1
    assert V7["TimedFinal"]["at"] == [24810, 0, 0]
    assert V7["TimedFinal"]["extent"][0] == 140
    assert V7["TimedFinal"]["top_z"] - (V7["BounceFinal"]["at"][2]
                                           + V7["BounceFinal"]["extent"][2]) > 264.7
    assert V7["Terrain_FinalIntermediate"]["left"] - (V7["TimedFinal"]["at"][0]
                                                       + V7["TimedFinal"]["extent"][0]) == 180
    assert V7["BounceFinal2"]["at"] == [25220, 0, 529]
    assert V7["BounceFinal2"]["at"][2] + V7["BounceFinal2"]["extent"][2] == 541
    assert V7["BounceFinal2"]["launch_velocity_z"] == 1100
    assert V7["TimedFinal2"]["at"] == [25640, 0, 0]
    assert V7["TimedFinal2"]["top_z"] == 840 and V7["TimedFinal2"]["active_seconds"] == 0.50
    assert V7["TimedFinal2"]["stable_landing_seconds"] == 0.1
    assert V7["TimedFinal2"]["extent"][0] == 140
    assert V7["TimedFinal2"]["top_z"] - (V7["BounceFinal2"]["at"][2]
                                             + V7["BounceFinal2"]["extent"][2]) > 264.7
    assert V7["Terrain_Final"]["left"] - (V7["TimedFinal2"]["at"][0]
                                           + V7["TimedFinal2"]["extent"][0]) == 260
    assert CONFIG["final_compound_design"]["hidden_pit_hazard_ids"] == [
        "CompoundHiddenPit01", "CompoundHiddenPit02", "CompoundHiddenPit03", "CompoundHiddenPit04"]
    hidden_pits = [V7["HazardCompoundHiddenPit%02d" % index] for index in range(1, 5)]
    assert all(not pit["b_reveal_on_spawn"] and pit["b_use_spike_cluster"] for pit in hidden_pits)
    assert [pit["at"] for pit in hidden_pits] == [
        [24580, 0, 100], [25040, 0, 400], [25455, 0, 430], [25840, 0, 750]]
    assert [pit["extent"] for pit in hidden_pits] == [[80, 160, 50]] * 4
    assert 26040 + 200 < V7["PickupRequired"]["at"][0] < V7["Goal"]["at"][0] < 28200 - 200
    assert V7["PickupRequired"]["at"][2] == 840 + 62
    assert V7["Goal"]["at"][2] == 840 + 92
    assert CONFIG["final_compound_design"]["compound_count"] == 2
    assert CONFIG["final_compound_design"]["bounce_pad_top_z_cm"] == [200, 541]
    assert CONFIG["final_compound_design"]["timed_platform_top_z_cm"] == [540, 840]
    assert CONFIG["final_compound_design"]["timed_platform_active_seconds"] == [0.60, 0.50]


def protected_hashes():
    result = {}
    for path in PROTECTED:
        assert path.is_file(), "Protected package missing: " + str(path)
        result[str(path.relative_to(PROJECT)).replace("\\", "/")] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def tags(actor):
    return {str(value) for value in actor.get_editor_property("tags")}


def values(vector):
    return [float(vector.x), float(vector.y), float(vector.z)]


def equal(actual, expected, description):
    assert len(actual) == len(expected) and all(abs(float(a) - float(b)) <= 0.05
                                               for a, b in zip(actual, expected)), description + ": " + str(actual)


def transform(spec):
    if spec["kind"] == "Terrain":
        position = [(spec["left"] + spec["right"]) / 2, 0,
                    spec["top"] - spec["thickness"] / 2]
        scale = [(spec["right"] - spec["left"]) / 100,
                 spec["depth"] / 100, spec["thickness"] / 100]
    else:
        position, scale = spec["at"], spec.get("scale", [1, 1, 1])
    rotation = [-45, -30, 0] if spec["kind"] == "KeyLight" else [0, 0, 0]
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
            if "show_checkpoint_label" in spec:
                result["show_checkpoint_label"] = spec["show_checkpoint_label"]
        if kind == "Pickup" and "pickup_order" in spec:
            result["pickup_order"] = spec["pickup_order"]
        if kind == "Pickup" and "b_required_for_goal" in spec:
            result["required_for_goal"] = spec["b_required_for_goal"]
        if "b_reveal_on_spawn" in spec:
            result["reveal_on_spawn"] = spec["b_reveal_on_spawn"]
        if "b_use_spike_cluster" in spec:
            result["use_spike_cluster"] = spec["b_use_spike_cluster"]
        return result
    if kind == "Bounce":
        return {"box_extent": unreal.Vector(*spec["extent"]),
                "launch_velocity_z": spec["launch_velocity_z"]}
    if kind == "Switch":
        result = dict({key: spec[key] for key in (
            "dock_left_x", "dock_right_x", "left_switch_x", "right_switch_x",
            "travel_speed_cm_per_second", "stable_boarding_seconds", "departure_cue_seconds")},
            platform_top_z=spec["top_z"], platform_extent=unreal.Vector(*spec["extent"]))
        optional = {
            "dock_left_top_z": "dock_left_top_z", "dock_right_top_z": "dock_right_top_z",
            "travel_speed_variation_cm_per_second": "travel_speed_variation_cm_per_second",
            "transit_trap_min_count": "transit_trap_min_count",
            "transit_trap_max_count": "transit_trap_max_count",
            "transit_trap_seed": "transit_trap_seed",
        }
        result.update({target: spec[source] for source, target in optional.items() if source in spec})
        return result
    if kind == "Timed":
        return {"platform_top_z": spec["top_z"], "platform_extent": unreal.Vector(*spec["extent"]),
                **{key: spec[key] for key in (
                    "stable_landing_seconds", "active_seconds", "disabled_seconds")}}
    return {}


def editor_property(actor, name):
    # UE Python may expose a C++ b-prefixed bool with either normalized spelling.
    candidates = {
        "reveal_on_spawn": ("reveal_on_spawn", "b_reveal_on_spawn", "bRevealOnSpawn"),
        "use_spike_cluster": ("use_spike_cluster", "b_use_spike_cluster", "bUseSpikeCluster"),
        "required_for_goal": ("required_for_goal", "b_required_for_goal", "bRequiredForGoal"),
        "show_checkpoint_label": ("show_checkpoint_label", "b_show_checkpoint_label", "bShowCheckpointLabel"),
    }.get(name, (name,))
    for candidate in candidates:
        try:
            actor.get_editor_property(candidate)
            return candidate
        except Exception:
            continue
    raise AssertionError("No editor property " + name + " on " + actor.get_class().get_path_name())


def verify_identity(actor, role, spec, generated=False):
    label = (V7_PREFIX if generated else SOURCE_PREFIX) + role
    assert actor.get_actor_label() == label
    assert actor.get_class().get_path_name() == CLASS_PATHS[spec["kind"]], "Class drift: " + label
    owner = CONFIG["owner_tag"] if generated else SOURCE_OWNER_TAG
    assert owner in tags(actor) and (not spec.get("tag") or spec["tag"] in tags(actor)), label + " ownership"
    if generated:
        assert "NaiwaP1V7StableId_" + spec["id"] in tags(actor), label + " stable ID"


def verify(actor, role, spec, generated=False):
    verify_identity(actor, role, spec, generated)
    label = (V7_PREFIX if generated else SOURCE_PREFIX) + role
    position, rotation, scale = transform(spec)
    equal(values(actor.get_actor_location()), position, label + " position")
    actual_rotation = actor.get_actor_rotation()
    equal([actual_rotation.pitch, actual_rotation.yaw, actual_rotation.roll], rotation, label + " rotation")
    equal(values(actor.get_actor_scale3d()), scale, label + " scale")
    for name, expected in parameters(spec).items():
        actual = actor.get_editor_property(editor_property(actor, name))
        if name in ("box_extent", "platform_extent", "respawn_offset"):
            equal(values(actual), values(expected), label + " " + name)
        elif isinstance(expected, bool):
            assert bool(actual) is expected, label + " " + name
        elif isinstance(expected, (int, float)):
            assert abs(float(actual) - float(expected)) <= 0.001, label + " " + name
        else:
            assert actual == expected, label + " " + name
    if spec["kind"] in ("Terrain", "Cue"):
        mesh = actor.get_component_by_class(unreal.StaticMeshComponent)
        assert mesh and mesh.get_editor_property("static_mesh").get_path_name() == "/Engine/BasicShapes/Cube.Cube"
        collision = mesh.get_collision_enabled()
        if spec["kind"] == "Cue":
            assert collision == unreal.CollisionEnabled.NO_COLLISION
        else:
            assert CONFIG["camera_terrain_tag"] in tags(actor)
            assert collision in (unreal.CollisionEnabled.QUERY_AND_PHYSICS,
                                 unreal.CollisionEnabled.QUERY_ONLY)
    return {"kind": spec["kind"], "id": spec.get("id", role), "location": values(actor.get_actor_location()),
            "scale": values(actor.get_actor_scale3d())}


def game_mode():
    bp = unreal.load_asset(ASSET_ROOT + "/BP_NaiwaP1GameMode")
    assert bp and isinstance(bp, unreal.Blueprint)
    klass = unreal.BlueprintEditorLibrary.generated_class(bp)
    defaults = unreal.get_default_object(klass)
    assert "BP_NaiwaP1Character" in defaults.get_editor_property("default_pawn_class").get_path_name()
    assert defaults.get_editor_property("player_controller_class").get_path_name() == "/Script/NaiwaRuntime.NaiwaP1PlayerController"
    assert int(defaults.get_editor_property("required_pickup_count")) == 1
    for kind in ("Hazard", "Switch", "Bounce", "Timed"):
        assert unreal.load_class(None, CLASS_PATHS[kind]), "Native mechanism missing: " + kind
    return klass


def collect(actors):
    result = {}
    for actor in actors.get_all_level_actors():
        if actor.get_class().get_path_name() in ("/Script/Engine.WorldSettings", "/Script/Engine.LevelBounds"):
            continue
        label = actor.get_actor_label()
        assert label not in result, "Duplicate actor label: " + label
        result[label] = actor
    return result


def source_labels():
    return {SOURCE_PREFIX + role for role in SOURCE_SPECS}


def final_labels():
    return {SOURCE_PREFIX + role for role in KEPT} | {V7_PREFIX + role for role in V7}


def preflight(mode_class):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP):
        assert not TARGET_FILE.exists(), "Target package exists without asset registration"
        assert unreal.EditorAssetLibrary.does_asset_exist(SOURCE_MAP)
        assert unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP, MAP), "Could not duplicate source map"
    assert levels.load_level(MAP), "Could not load v7 candidate"
    settings = unreal.EditorLevelLibrary.get_editor_world().get_world_settings()
    assert settings.get_editor_property("kill_z") == -1000
    assert settings.get_editor_property("default_game_mode").get_path_name() == mode_class.get_path_name()
    markers = tags(settings)
    found = collect(actors)
    if markers == SOURCE_FULL_MARKERS:
        assert set(found) == source_labels(), "Source clone actor drift"
        for role, spec in SOURCE_SPECS.items():
            verify(found[SOURCE_PREFIX + role], role, spec)
        return levels, actors, settings, found, False, False
    if markers == V7_MARKERS:
        assert set(found) == final_labels(), "Saved v7 actor drift"
        for role, spec in KEPT.items():
            verify(found[SOURCE_PREFIX + role], role, spec)
        for role, spec in V7.items():
            verify(found[V7_PREFIX + role], role, spec, True)
        return levels, actors, settings, found, True, False

    prior_configs = {marker for marker in markers if marker.startswith("NaiwaP1V7Cfg_")}
    if CONFIG["map_tag"] in markers and len(markers) == 2 and len(prior_configs) == 1:
        source_roles_found = {label[len(SOURCE_PREFIX):] for label in found if label.startswith(SOURCE_PREFIX)}
        v7_roles_found = {label[len(V7_PREFIX):] for label in found if label.startswith(V7_PREFIX)}
        assert source_roles_found == set(KEPT_SOURCE), "Prior V7 retained teaching actor inventory drift"
        assert len(v7_roles_found) in (45, 51, 56, len(V7)) and v7_roles_found.issubset(V7), "Prior V7 generated actor inventory drift"
        assert set(found) == ({SOURCE_PREFIX + role for role in KEPT_SOURCE}
                              | {V7_PREFIX + role for role in v7_roles_found}), "Prior V7 has unowned actors"
        for role, spec in KEPT_SOURCE.items():
            verify_identity(found[SOURCE_PREFIX + role], role, spec)
        for role in v7_roles_found:
            spec = V7[role]
            verify_identity(found[V7_PREFIX + role], role, spec, True)
        return levels, actors, settings, found, False, True
    raise AssertionError("Unknown map marker/config state: " + str(sorted(markers)))


def material(name):
    asset_name = "M_P1_V7_" + name
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


def build(actors, found, mats, stale_v7=False):
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    assert cube
    for role in set(SOURCE_SPECS).difference(KEPT):
        label = SOURCE_PREFIX + role
        if label in found:
            assert actors.destroy_actor(found[label]), "Could not remove superseded clone actor: " + label
    if stale_v7:
        for role in V7:
            label = V7_PREFIX + role
            if label in found:
                assert actors.destroy_actor(found[label]), "Could not replace prior V7 actor: " + label
    for role, override in CONFIG.get("retained_source_overrides", {}).items():
        actor = found[SOURCE_PREFIX + role]
        spec = KEPT[role]
        position, rotation, scale = transform(spec)
        actor.set_actor_location(unreal.Vector(*position), False, True)
        actor.set_actor_rotation(unreal.Rotator(*rotation), False)
        actor.set_actor_scale3d(unreal.Vector(*scale))
        for name, value in parameters(spec).items():
            actor.set_editor_property(editor_property(actor, name), value)
    found = collect(actors)
    for spec in CONFIG["extension_actors"]:
        role = spec["role"]
        klass = unreal.load_class(None, CLASS_PATHS[spec["kind"]])
        assert klass
        position, _, scale = transform(spec)
        actor = actors.spawn_actor_from_class(klass, unreal.Vector(*position))
        assert actor
        actor.set_actor_label(V7_PREFIX + role)
        role_tags = [CONFIG["owner_tag"], spec["tag"], "NaiwaP1V7StableId_" + spec["id"]]
        if spec["kind"] == "Terrain":
            role_tags.append(CONFIG["camera_terrain_tag"])
        actor.set_editor_property("tags", [unreal.Name(tag) for tag in role_tags])
        actor.set_actor_rotation(unreal.Rotator(0, 0, 0), False)
        actor.set_actor_scale3d(unreal.Vector(*scale))
        for name, value in parameters(spec).items():
            actor.set_editor_property(editor_property(actor, name), value)
        if spec["kind"] == "Terrain":
            mesh = actor.get_component_by_class(unreal.StaticMeshComponent)
            mesh.set_static_mesh(cube)
            mesh.set_material(0, mats[spec["material"]])
            mesh.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        elif spec["kind"] == "Switch":
            for component, color in (("platform_display", "switch_platform"),
                                     ("left_switch_display", "left_switch"),
                                     ("right_switch_display", "right_switch")):
                actor.get_editor_property(component).set_material(0, mats[color])
        else:
            component = "platform_display" if spec["kind"] == "Timed" else "display_mesh"
            actor.get_editor_property(component).set_material(0, mats[spec["material"]])
            if spec["kind"] == "Hazard":
                actor.get_editor_property("side_spike_left").set_material(0, mats[spec["material"]])
                actor.get_editor_property("side_spike_right").set_material(0, mats[spec["material"]])
        verify(actor, role, spec, True)


report = {"schema": CONFIG["schema"], "target_map": MAP, "source_map": SOURCE_MAP,
          "phase": CONFIG["phase"], "config_sha256": CONFIG_HASH,
          "builder_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
          "engine": unreal.SystemLibrary.get_engine_version(), "status": "running",
          "acceptance": "pending_A_B_C_route_checkpoint_failure_and_visual_review"}
before = None
try:
    validate_layout()
    before = protected_hashes()
    report["protected_sha256_before"] = before
    assert SOURCE_FILE.is_file()
    assert hashlib.sha256(SOURCE_FILE.read_bytes()).hexdigest() == CONFIG["source_map_sha256"]
    if TARGET_FILE.is_file():
        report["target_map_sha256_before"] = hashlib.sha256(TARGET_FILE.read_bytes()).hexdigest()
    levels, actors, settings, found, current_v7, stale_v7 = preflight(game_mode())
    prior_kept_specs = KEPT if current_v7 or stale_v7 else KEPT_SOURCE
    teaching_before = {role: verify(found[SOURCE_PREFIX + role], role, spec)
                       for role, spec in prior_kept_specs.items()}
    if not current_v7:
        mats = {name: material(name) for name in COLORS}
        build(actors, found, mats, stale_v7)
        settings.set_editor_property("tags", [unreal.Name(tag) for tag in sorted(V7_MARKERS)])
        assert levels.save_current_level(), "Could not save v7 candidate"
    assert levels.load_level(MAP), "Could not reload saved v7 candidate"
    settings = unreal.EditorLevelLibrary.get_editor_world().get_world_settings()
    found = collect(actors)
    assert set(found) == final_labels()
    assert tags(settings) == V7_MARKERS
    assert settings.get_editor_property("kill_z") == -1000
    assert settings.get_editor_property("default_game_mode").get_path_name() == game_mode().get_path_name()
    kept = {role: verify(found[SOURCE_PREFIX + role], role, spec)
            for role, spec in KEPT.items()}
    if current_v7:
        assert kept == teaching_before, "Retained teaching actor semantics changed"
    else:
        for role, before_semantics in teaching_before.items():
            if role not in CONFIG.get("retained_source_overrides", {}):
                assert kept[role] == before_semantics, "Unchanged retained teaching actor drifted: " + role
    extension = {role: verify(found[V7_PREFIX + role], role, spec, True)
                 for role, spec in V7.items()}
    assert protected_hashes() == before, "Protected source/P0/old map package changed"
    assert TARGET_FILE.is_file()
    report["target_map_sha256_after"] = hashlib.sha256(TARGET_FILE.read_bytes()).hexdigest()
    report["target_map_bytes"] = TARGET_FILE.stat().st_size
    report["layout"] = {
        "status": "v7_candidate_idempotent_verified" if current_v7 else
                  "prior_v7_candidate_refreshed" if stale_v7 else "source_cloned_and_v7_candidate_saved",
        "verified_actor_count": len(found), "retained_teaching_actor_count": len(kept),
        "extension_actor_count": len(extension), "checkpoint_count": len(CONFIG["checkpoints"]),
        "gap_count": sum(len(chain["gaps"]) for chain in CONFIG["gap_chains"]),
        "visible_pit_hazard_count": len([item for item in V7.values()
                                          if item["kind"] == "Hazard" and item["b_reveal_on_spawn"]]),
        "hidden_terminal_pit_hazard_count": len([item for item in V7.values()
                                                  if item["kind"] == "Hazard" and not item["b_reveal_on_spawn"]]),
        "visible_pit_spike_points": 33,
        "random_shuttle_traps_per_departure": [1, 2],
        "shuttle_speed_profile": {"base_cm_s": 300, "variation_cm_s": 100,
                                   "sampled_range_cm_s": [200, 400], "cycles_per_crossing": 2},
        "checkpoint_labels_enabled": CONFIG["show_checkpoint_labels"],
        "retained_teaching_bounce_rise_cm": KEPT["Terrain_BounceLanding"]["top"] - KEPT["Terrain_RightBank"]["top"],
        "documented_normal_jump_envelope_cm": 264.7,
        "c_shuttle_endpoint_top_z_cm": [V7["SwitchC"]["dock_left_top_z"], V7["SwitchC"]["dock_right_top_z"]],
        "timed_c_design": CONFIG["timed_c_design"],
        "kill_z": -1000, "required_pickup_count": CONFIG["required_pickup_count"],
        "physical_pickup_count": sum(item["kind"] == "Pickup" for item in V7.values()),
        "saved_semantics": {"transform_reconciliation": "all_owned_actors_match_config_after_save_reload",
                            "retained_actor_transforms": kept, "extension_actor_transforms": extension},
    }
    report["status"] = "saved_v7_candidate_pending_route_and_visual_review"
    unreal.log("NAIWA_P1_V7_BUILD_OK " + MAP)
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    unreal.log_error(report["error"])
    raise
finally:
    if before is not None:
        report["protected_sha256_after"] = protected_hashes()
        report["protected_packages_unchanged"] = report["protected_sha256_after"] == before
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
