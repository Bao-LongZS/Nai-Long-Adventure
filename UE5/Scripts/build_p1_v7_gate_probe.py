"""Build an isolated, one-use-at-a-time physics fixture for v7 gates A/B/C.

This package is not the integrated v7 level. It retains the proposed local
coordinates and native mechanisms so whole transitions can be measured before
the production map is frozen. Existing unrelated packages are never opened.
"""

import hashlib
import json
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
MAP = "/Game/Naiwa/P1/Maps/L_NaiwaP1_v7_GateProbe"
MAP_FILE = PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_v7_GateProbe.umap"
OUTPUT = PROJECT / "Saved/NaiwaP1/v7_gate_probe_build_v2.json"
BASELINE_OUTPUT = PROJECT / "Saved/NaiwaP1/v7_gate_probe_build.json"
MARKER = "NaiwaP1V7GateProbeV1"
PREFIX = "P1V7Probe_"
SCRIPT_HASH = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()

TERRAINS = [
    ("A_Left", 4500, 4900, 180), ("A_Right", 7500, 7900, 180),
    ("B_Left", 10100, 10520, 180), ("B_High", 13420, 14120, 480),
    ("C_Left", 16350, 16840, 240), ("C_Right", 19260, 20260, 240),
]
SWITCHES = [
    # gate, actor origin x, dock-left offset, dock-right offset, top, left/right switch offsets
    ("A", 6200, -1170, 940, 180, -1390, 1500),
    ("B", 11820, -1170, 1180, 180, -1450, 1500),
    ("C", 17840, -870, 870, 240, -1200, 1500),
]
CHECKPOINTS = [
    # Gate, safe static-bank centre, walking top, v7 stable checkpoint ID.
    ("A", 4700, 180, "IntegratedCheckpoint02"),
    ("B", 10370, 180, "IntegratedCheckpoint04"),
    ("C", 16640, 240, "IntegratedCheckpoint06"),
]
EXPECTED = ({PREFIX + "Terrain_" + role for role, *_ in TERRAINS}
            | {PREFIX + gate + "_Switch" for gate, *_ in SWITCHES}
            | {PREFIX + gate + "_Checkpoint" for gate, *_ in CHECKPOINTS}
            | {PREFIX + role for role in ("B_Pad", "C_Timed", "Start", "KeyLight", "SkyLight", "Atmosphere")})


def non_system_actors(actor_subsystem):
    return [actor for actor in actor_subsystem.get_all_level_actors()
            if actor.get_class().get_path_name() not in
            ("/Script/Engine.WorldSettings", "/Script/Engine.LevelBounds")]


def place(actor_subsystem, existing, role, klass, xyz, extra_tag=None):
    name = PREFIX + role
    actor = existing.get(name)
    if actor is None:
        actor = actor_subsystem.spawn_actor_from_class(klass, unreal.Vector(*xyz))
        assert actor, "Spawn failed: " + name
        actor.set_actor_label(name)
        existing[name] = actor
    expected_class_name = klass.__name__ if isinstance(klass, type) else klass.get_name()
    assert actor.get_class().get_name() == expected_class_name, "Class drift: " + name
    actor.set_actor_location(unreal.Vector(*xyz), False, False)
    actor.set_actor_rotation(unreal.Rotator(0, 0, 0), False)
    actor.set_actor_scale3d(unreal.Vector(1, 1, 1))
    actor.set_editor_property("tags", [unreal.Name(MARKER)] +
                              ([unreal.Name(extra_tag)] if extra_tag else []))
    return actor


def checkpoint_kind():
    enum = getattr(unreal, "NaiwaP1TriggerKind", None) or getattr(unreal, "ENaiwaP1TriggerKind", None)
    assert enum, "Native trigger enum missing"
    names = [name for name in dir(enum) if name.replace("_", "").lower() == "checkpoint"]
    assert len(names) == 1, "Cannot identify native checkpoint trigger kind"
    return getattr(enum, names[0])


def run():
    mode_bp = unreal.load_asset("/Game/Naiwa/P1/BP_NaiwaP1GameMode")
    assert mode_bp and isinstance(mode_bp, unreal.Blueprint)
    mode_class = unreal.BlueprintEditorLibrary.generated_class(mode_bp)
    assert mode_class
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    assert cube
    switch_class = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1SwitchPlatform")
    pad_class = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1BouncePad")
    timed_class = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1TimedPlatform")
    trigger_class = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1Trigger")
    assert switch_class and pad_class and timed_class and trigger_class

    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existed = unreal.EditorAssetLibrary.does_asset_exist(MAP)
    assert levels.load_level(MAP) if existed else levels.new_level(MAP)
    settings = unreal.EditorLevelLibrary.get_editor_world().get_world_settings()
    existing = {}
    if existed:
        assert MARKER in {str(tag) for tag in settings.get_editor_property("tags")}, "Unowned existing map"
        for actor in non_system_actors(actors):
            name = actor.get_actor_label()
            assert name in EXPECTED and name not in existing, "Unexpected/duplicate existing actor: " + name
            assert MARKER in {str(tag) for tag in actor.get_editor_property("tags")}, "Unowned actor: " + name
            existing[name] = actor
    settings.set_editor_property("tags", [unreal.Name(MARKER)])
    settings.set_editor_property("default_game_mode", mode_class)
    settings.set_editor_property("kill_z", -1000.0)

    for role, left, right, top in TERRAINS:
        assert left < right and top > 0
        actor = place(actors, existing, "Terrain_" + role, unreal.StaticMeshActor,
                      ((left + right) / 2, 0, top - 40), MARKER + "_Terrain")
        mesh = actor.get_component_by_class(unreal.StaticMeshComponent)
        assert mesh
        mesh.set_static_mesh(cube)
        mesh.set_collision_profile_name("BlockAll")
        actor.set_actor_scale3d(unreal.Vector((right - left) / 100, 6, 0.8))

    for gate, x, top, stable_id in CHECKPOINTS:
        checkpoint = place(actors, existing, gate + "_Checkpoint", trigger_class,
                           (x, 0, top + 92), MARKER + "_" + gate + "_Checkpoint")
        checkpoint.set_editor_properties({
            "trigger_kind": checkpoint_kind(), "trigger_id": unreal.Name(stable_id),
            "box_extent": unreal.Vector(75, 120, 90),
            "respawn_offset": unreal.Vector(0, 0, 0),
        })

    for gate, origin, left_dock, right_dock, top, left_switch, right_switch in SWITCHES:
        actor = place(actors, existing, gate + "_Switch", switch_class, (origin, 0, 0),
                      MARKER + "_" + gate + "_Switch")
        actor.set_editor_properties({
            "dock_left_x": float(left_dock), "dock_right_x": float(right_dock),
            "left_switch_x": float(left_switch), "right_switch_x": float(right_switch),
            "platform_top_z": float(top), "platform_extent": unreal.Vector(120, 160, 20),
            "travel_speed_cm_per_second": 300.0,
            "stable_boarding_seconds": 0.25, "departure_cue_seconds": 0.65,
        })

    # B baseline is intentionally same-height: 60 cm from shuttle edge to pad.
    pad = place(actors, existing, "B_Pad", pad_class, (13260, 0, 168), MARKER + "_B_Pad")
    pad.set_editor_properties({"box_extent": unreal.Vector(80, 120, 12),
                               "launch_velocity_z": 1150.0})
    timed = place(actors, existing, "C_Timed", timed_class, (19050, 0, 0), MARKER + "_C_Timed")
    timed.set_editor_properties({"platform_top_z": 340.0, "platform_extent": unreal.Vector(210, 160, 20),
                                 "stable_landing_seconds": 0.18, "active_seconds": 1.72,
                                 "disabled_seconds": 1.6})

    # Keep PlayerStart outside the checkpoint overlap. If the pawn spawns inside,
    # its initial BeginOverlap can precede GameMode character registration.
    place(actors, existing, "Start", unreal.PlayerStart, (4520, 0, 272))
    light = place(actors, existing, "KeyLight", unreal.DirectionalLight, (0, 0, 800))
    light.set_actor_rotation(unreal.Rotator(0, -45, -30), False)
    component = light.get_component_by_class(unreal.DirectionalLightComponent)
    assert component
    component.set_editor_property("intensity", 3.0)
    component.set_editor_property("atmosphere_sun_light", True)
    place(actors, existing, "SkyLight", unreal.SkyLight, (0, 0, 600))
    place(actors, existing, "Atmosphere", unreal.SkyAtmosphere, (0, 0, 0))

    assert set(existing) == EXPECTED, "Missing fixture actors: " + str(sorted(EXPECTED - set(existing)))
    assert levels.save_current_level(), "Failed to save gate probe map"
    assert MAP_FILE.is_file(), "Saved map file missing"
    baseline = json.loads(BASELINE_OUTPUT.read_text(encoding="utf-8")) if BASELINE_OUTPUT.is_file() else None
    return {
        "schema": "NaiwaP1V7GateProbeBuildV2", "status": "saved_isolated_fixture_pending_runtime",
        "map": MAP, "map_sha256": hashlib.sha256(MAP_FILE.read_bytes()).hexdigest(),
        "script_sha256": SCRIPT_HASH, "actor_count": len(existing),
        "checkpoint_ids": {gate: stable_id for gate, _, _, stable_id in CHECKPOINTS},
        "checkpoint_source": "three_native_overlap_triggers_on_static_left_banks",
        "baseline_build_report": str(BASELINE_OUTPUT) if baseline else None,
        "baseline_fixture_map_sha256": baseline.get("map_sha256") if baseline else None,
        "geometry": {"A": "shuttle edge 7260 to bank 7500; top 180",
                     "B": "shuttle edge 13120, 60 gap, pad 13180-13340 top 180, bank 13420 top 480",
                     "C": "shuttle edge 18830, timed 18840-19260 top 340 active 1.72, bank 19260 top 240"},
        "isolated_only": True, "existed": existed,
    }


try:
    result = run()
except Exception:
    result = {"schema": "NaiwaP1V7GateProbeBuildV2", "status": "failed",
              "map": MAP, "script_sha256": SCRIPT_HASH, "error": traceback.format_exc()}
    unreal.log_error(result["error"])
OUTPUT.parent.mkdir(parents=True, exist_ok=True)
OUTPUT.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("NAIWA_P1_V7_GATE_BUILD status={} report={}".format(result["status"], OUTPUT))
