"""Create the isolated P1 full-level candidate after the slice smoke passes.

This script only creates /Game/Naiwa/P1 materials and L_NaiwaP1_Full. A
complete existing map is preserved; a partial owned map is never overwritten.
"""

import hashlib
import json
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
CONFIG_PATH = ROOT / "UE5/Scripts/p1_full_level_config_v1.json"
CONFIG = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
OUTPUT = PROJECT / "Saved/NaiwaP1/full_build.json"
OUTPUT.parent.mkdir(parents=True, exist_ok=True)

assert CONFIG["schema"] == "NaiwaP1FullLevelV1"
ASSET_ROOT = CONFIG["asset_root"]
MAP = CONFIG["map"]
assert ASSET_ROOT == "/Game/Naiwa/P1"
assert MAP == "/Game/Naiwa/P1/Maps/L_NaiwaP1_Full"
OWNER_TAG = CONFIG["owner_tag"]
MAP_TAG = CONFIG["map_tag"]
CONFIG_HASH = hashlib.sha256(CONFIG_PATH.read_bytes()).hexdigest()
CONFIG_TAG = "NaiwaP1FullCfg_" + CONFIG_HASH[:16]

PROTECTED = [
    PROJECT / "Content/Naiwa/Maps/L_NaiwaGraybox.umap",
    PROJECT / "Content/Naiwa/Blueprints/BP_NaiwaCharacter.uasset",
    PROJECT / "Content/Naiwa/Blueprints/BP_NaiwaGameMode.uasset",
    PROJECT / "Content/Naiwa/Input/IMC_Naiwa.uasset",
    PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Slice.umap",
    PROJECT / "Content/Naiwa/P1/BP_NaiwaP1Character.uasset",
    PROJECT / "Content/Naiwa/P1/BP_NaiwaP1GameMode.uasset",
]


def protected_hashes():
    return {
        str(path.relative_to(PROJECT)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in PROTECTED
    }


def slice_smoke_evidence():
    reports = sorted((PROJECT / "Saved/NaiwaP1").glob("smoke_*.json"), reverse=True)
    for path in reports:
        try:
            data = json.loads(path.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError):
            continue
        if (data.get("schema") == "NaiwaP1SmokeV1" and data.get("status") == "PASS"
                and data.get("map") == "L_NaiwaP1_Slice"):
            return str(path.relative_to(PROJECT))
    raise AssertionError("P1 slice smoke PASS report required before full-map generation")


def surface_at(x):
    for spec in CONFIG["terrain_blocks"]:
        if spec["x"] - spec["width"] / 2 <= x < spec["x"] + spec["width"] / 2:
            return float(spec["top"])
    raise AssertionError("No terrain under X=" + str(x))


def validate_layout():
    terrain = CONFIG["terrain_blocks"]
    auxiliary = CONFIG["auxiliary_platforms"]
    triggers = CONFIG["triggers"]
    assert len(terrain) == 11, "Expected eleven primary terrain blocks"
    assert len([item for item in triggers if item["kind"] == "Hazard"]) == 9
    for kind in ("Pickup", "Checkpoint", "Goal", "KillZ"):
        assert len([item for item in triggers if item["kind"] == kind]) == 1
    assert len({item["id"] for item in terrain + auxiliary}) == len(terrain + auxiliary)
    assert len({item["id"] for item in triggers}) == len(triggers)
    assert terrain == sorted(terrain, key=lambda item: item["x"])
    assert abs(CONFIG["spawn"][2] - (surface_at(CONFIG["spawn"][0]) + 92.2)) < 1.0
    for left, right in zip(terrain, terrain[1:]):
        gap = (right["x"] - right["width"] / 2) - (left["x"] + left["width"] / 2)
        assert gap <= 0.0, "Primary path has an unplanned gap"
        assert float(right["top"]) - float(left["top"]) <= 80.0, "Step exceeds candidate jump rise"
    surfaces = terrain + auxiliary
    for platform in auxiliary:
        x1 = platform["x"] - platform["width"] / 2 + 0.01
        x2 = platform["x"] + platform["width"] / 2 - 0.01
        assert platform["top"] - platform["thickness"] - max(surface_at(x1), surface_at(x2)) >= 185.0, (
            "Overhead platform obstructs the 180cm player on the primary path: " + platform["id"]
        )
    by_platform = {item["id"]: item for item in auxiliary}
    for name in ("leaf_west", "tree_canopy"):
        platform = by_platform[name]
        assert platform["x"] - platform["width"] / 2 >= 1700.0
        assert platform["x"] + platform["width"] / 2 <= 1930.0
    eastern_cloud = by_platform["cloud_east"]
    assert eastern_cloud["x"] - eastern_cloud["width"] / 2 >= 3300.0
    assert eastern_cloud["x"] + eastern_cloud["width"] / 2 <= 3540.0
    for trigger in triggers:
        assert len(trigger["at"]) == len(trigger["extent"]) == 3
        assert all(float(value) > 0 for value in trigger["extent"])
        if trigger["kind"] != "Hazard":
            continue
        x, _, z = trigger["at"]
        bottom = z - trigger["extent"][2]
        assert any(abs(bottom - spec["top"]) < 0.01 and
                   spec["x"] - spec["width"] / 2 < x < spec["x"] + spec["width"] / 2
                   for spec in surfaces), "Hazard is not placed on a solid platform: " + trigger["id"]
    by_kind = {kind: next(item for item in triggers if item["kind"] == kind)
               for kind in ("Checkpoint", "Pickup", "Goal")}
    assert by_kind["Checkpoint"]["at"][0] < by_kind["Pickup"]["at"][0] < by_kind["Goal"]["at"][0]
    for kind in ("Checkpoint", "Pickup"):
        trigger = by_kind[kind]
        assert abs(trigger["at"][2] - surface_at(trigger["at"][0]) - 92.0) < 1.0
    checkpoint = by_kind["Checkpoint"]
    offset = checkpoint["respawn_offset"]
    respawn_x = checkpoint["at"][0] + offset[0]
    respawn_z = checkpoint["at"][2] + offset[2]
    assert respawn_z - 90.0 >= surface_at(respawn_x) + 5.0
    assert all(abs(respawn_x - item["at"][0]) > 42.0 + item["extent"][0] + 20.0
               for item in triggers if item["kind"] == "Hazard"
               and abs(item["at"][2] - item["extent"][2] - surface_at(respawn_x)) < 0.01)


def material(name, color):
    path = ASSET_ROOT + "/Materials/M_P1_" + name
    existing = unreal.load_asset(path)
    if existing:
        assert isinstance(existing, unreal.Material), path
        return existing
    created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_P1_" + name, ASSET_ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew()
    )
    assert created and created.get_path_name().startswith(ASSET_ROOT + "/")
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        created, unreal.MaterialExpressionConstant3Vector, -350, 0
    )
    expression.set_editor_property("constant", unreal.LinearColor(*color, 1.0))
    assert unreal.MaterialEditingLibrary.connect_material_property(
        expression, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    created.set_editor_property("two_sided", True)
    assert not unreal.MaterialEditingLibrary.recompile_material(created)
    assert unreal.EditorAssetLibrary.save_loaded_asset(created)
    return created


def resolve_kind(name):
    enum = getattr(unreal, "NaiwaP1TriggerKind", None) or getattr(unreal, "ENaiwaP1TriggerKind", None)
    assert enum, "NaiwaP1TriggerKind unavailable; build P1 native code first"
    matches = [item for item in dir(enum) if item.replace("_", "").lower() == name.lower()]
    assert len(matches) == 1, "Cannot resolve trigger kind " + name
    return getattr(enum, matches[0])


def add_owned(actor, label):
    assert actor
    actor.set_actor_label("P1_" + label)
    actor.set_editor_property("tags", [unreal.Name(OWNER_TAG)])
    return actor


def add_block(actors, cube, materials, spec, prefix):
    thickness = float(spec["thickness"])
    block = add_owned(actors.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(float(spec["x"]), 0.0, float(spec["top"]) - thickness / 2.0)),
        prefix + spec["id"])
    component = block.get_component_by_class(unreal.StaticMeshComponent)
    assert component
    component.set_static_mesh(cube)
    component.set_material(0, materials[spec.get("material", "ground")])
    block.set_actor_scale3d(unreal.Vector(float(spec["width"]) / 100.0, 7.0, thickness / 100.0))
    return block


def add_trigger(actors, trigger_class, materials, spec):
    trigger = add_owned(actors.spawn_actor_from_class(trigger_class, unreal.Vector(*spec["at"])),
                        "Trigger_" + spec["id"])
    trigger.set_editor_properties({
        "trigger_kind": resolve_kind(spec["kind"]),
        "trigger_id": unreal.Name(spec["id"]),
        "box_extent": unreal.Vector(*spec["extent"]),
    })
    if spec["kind"] == "Checkpoint":
        trigger.set_editor_property("respawn_offset", unreal.Vector(*spec["respawn_offset"]))
    if spec.get("material"):
        display = trigger.get_editor_property("display_mesh")
        assert display
        display.set_material(0, materials[spec["material"]])
    return trigger


def build_map(game_mode_class, materials):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    exists = unreal.EditorAssetLibrary.does_asset_exist(MAP)
    assert levels.load_level(MAP) if exists else levels.new_level(MAP)
    world = unreal.EditorLevelLibrary.get_editor_world()
    settings = world.get_world_settings()
    tags = {str(tag) for tag in settings.get_editor_property("tags")}
    required = {"P1_Start", "P1_KeyLight", "P1_SkyLight", "P1_Sky"}
    required.update("P1_Terrain_" + item["id"] for item in CONFIG["terrain_blocks"])
    required.update("P1_Aux_" + item["id"] for item in CONFIG["auxiliary_platforms"])
    required.update("P1_Trigger_" + item["id"] for item in CONFIG["triggers"])
    if exists:
        assert MAP_TAG in tags and CONFIG_TAG in tags, "Existing full map has different ownership/config; refusing overwrite"
        current_mode = settings.get_editor_property("default_game_mode")
        assert current_mode and current_mode.get_path_name() == game_mode_class.get_path_name(), (
            "Existing full map uses another GameMode; refusing overwrite"
        )
        owned = {actor.get_actor_label() for actor in actors.get_all_level_actors()
                 if OWNER_TAG in [str(tag) for tag in actor.get_editor_property("tags")]}
        if required.issubset(owned):
            return {"map": MAP, "status": "existing_map_preserved", "owned_count": len(owned)}
        assert not owned, "Existing full map has partial generated actors; inspect before rebuilding: " + str(sorted(required - owned))
    else:
        settings.set_editor_property("tags", [unreal.Name(MAP_TAG), unreal.Name(CONFIG_TAG)])
        settings.set_editor_property("default_game_mode", game_mode_class)
        assert levels.save_current_level(), "Could not save full-map ownership marker"

    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    trigger_class = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1Trigger")
    assert cube and trigger_class
    for spec in CONFIG["terrain_blocks"]:
        add_block(actors, cube, materials, spec, "Terrain_")
    for spec in CONFIG["auxiliary_platforms"]:
        add_block(actors, cube, materials, spec, "Aux_")
    for spec in CONFIG["triggers"]:
        add_trigger(actors, trigger_class, materials, spec)

    add_owned(actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(*CONFIG["spawn"])), "Start")
    light = add_owned(actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 800.0)),
                      "KeyLight")
    light.set_actor_rotation(unreal.Rotator(-45.0, -30.0, 0.0), False)
    light.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("intensity", 3.0)
    add_owned(actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0.0, 0.0, 600.0)), "SkyLight")
    add_owned(actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0.0, 0.0, 0.0)), "Sky")
    assert levels.save_current_level(), "Unable to save P1 full map"
    return {"map": MAP, "status": "new_map_saved", "owned_count": len(required),
            "terrain_count": len(CONFIG["terrain_blocks"]),
            "auxiliary_count": len(CONFIG["auxiliary_platforms"]),
            "hazard_count": sum(item["kind"] == "Hazard" for item in CONFIG["triggers"])}


report = {"schema": CONFIG["schema"], "engine": unreal.SystemLibrary.get_engine_version(),
          "status": "running", "target_map": MAP, "config_sha256": CONFIG_HASH}
before = protected_hashes()
try:
    validate_layout()
    report["slice_smoke_evidence"] = slice_smoke_evidence()
    game_mode = unreal.load_asset(ASSET_ROOT + "/BP_NaiwaP1GameMode")
    assert game_mode and isinstance(game_mode, unreal.Blueprint)
    game_mode_class = unreal.BlueprintEditorLibrary.generated_class(game_mode)
    assert game_mode_class
    materials = {
        "ground": material("ground", (0.10, 0.23, 0.31)),
        "pickup": material("pickup", (0.98, 0.72, 0.09)),
        "checkpoint": material("checkpoint", (0.12, 0.85, 0.35)),
        "goal": material("goal", (0.10, 0.45, 0.97)),
        "hazard": material("hazard", (0.94, 0.11, 0.16)),
        "tree": material("tree", (0.32, 0.18, 0.10)),
        "leaf": material("leaf", (0.16, 0.48, 0.20)),
        "cloud": material("cloud", (0.72, 0.86, 0.95)),
    }
    report["layout"] = build_map(game_mode_class, materials)
    assert protected_hashes() == before, "Protected P0/slice resource changed during full-map generation"
    report["protected_sha256"] = before
    report["status"] = "saved_candidate_pending_full_walk_run_review"
    unreal.log("NAIWA_P1_FULL_BUILD_OK " + MAP)
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    unreal.log_error(report["error"])
    raise
finally:
    OUTPUT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
