"""Build the isolated P1 vertical slice; never save a P0 asset or map.

Run only after NaiwaP1GameMode and NaiwaP1Trigger have been compiled. The
P1 character Blueprint is copied once from the delivered P0 character. A
later run preserves that Blueprint's manually tuned camera settings.
"""

import hashlib
import json
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
CONFIG = json.loads((ROOT / "UE5/Scripts/p1_level_config_v1.json").read_text(encoding="utf-8"))
OUTPUT = PROJECT / "Saved/NaiwaP1/slice_build.json"
OUTPUT.parent.mkdir(parents=True, exist_ok=True)

assert CONFIG["schema"] == "NaiwaP1LevelV1"
ASSET_ROOT = CONFIG["asset_root"]
assert ASSET_ROOT == "/Game/Naiwa/P1", "P1 asset root must be isolated"
MAP = CONFIG["slice"]["map"]
assert MAP == "/Game/Naiwa/P1/Maps/L_NaiwaP1_Slice", "Refusing unexpected target map"
assert MAP != "/Game/Naiwa/Maps/L_NaiwaGraybox"
OWNER_TAG = CONFIG["owner_tag"]
MAP_TAG = CONFIG["map_tag"]
CUE_TAG = "NaiwaP1SliceCueV1"
CUE_LABEL = "P1_Cue_hazard_1"
FULL_MAP_FILE = PROJECT / "Content/Naiwa/P1/Maps/L_NaiwaP1_Full.umap"

P0_SOURCE_BP = "/Game/Naiwa/Blueprints/BP_NaiwaCharacter"
P1_CHARACTER_BP = ASSET_ROOT + "/BP_NaiwaP1Character"
P1_GAME_MODE_BP = ASSET_ROOT + "/BP_NaiwaP1GameMode"
P0_PROTECTED_FILES = [
    PROJECT / "Content/Naiwa/Maps/L_NaiwaGraybox.umap",
    PROJECT / "Content/Naiwa/Blueprints/BP_NaiwaCharacter.uasset",
    PROJECT / "Content/Naiwa/Blueprints/BP_NaiwaGameMode.uasset",
    PROJECT / "Content/Naiwa/Input/IMC_Naiwa.uasset",
]


def file_hash(path):
    assert path.is_file(), "Protected P0 asset missing: " + str(path)
    return hashlib.sha256(path.read_bytes()).hexdigest()


def protected_hashes():
    return {str(path.relative_to(PROJECT)): file_hash(path) for path in P0_PROTECTED_FILES}


def save(asset):
    assert asset.get_path_name().startswith(ASSET_ROOT + "/")
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset)


def blueprint(name, parent_class):
    path = ASSET_ROOT + "/" + name
    existing = unreal.load_asset(path)
    if existing:
        assert isinstance(existing, unreal.Blueprint), path + " is not a Blueprint"
        return existing, False
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, ASSET_ROOT, unreal.Blueprint, factory
    )
    assert created, "Unable to create " + path
    return created, True


def generated_class(bp):
    assert unreal.BlueprintEditorLibrary.compile_blueprint(bp), bp.get_path_name()
    cls = unreal.BlueprintEditorLibrary.generated_class(bp)
    assert cls, bp.get_path_name()
    return cls


def make_character():
    source = unreal.load_asset(P0_SOURCE_BP)
    assert source, "P0 character Blueprint unavailable"
    existing = unreal.load_asset(P1_CHARACTER_BP)
    created = existing is None
    if created:
        assert not unreal.EditorAssetLibrary.does_asset_exist(P1_CHARACTER_BP)
        bp = unreal.EditorAssetLibrary.duplicate_asset(P0_SOURCE_BP, P1_CHARACTER_BP)
        assert bp, "P1 character duplication failed"
    else:
        bp = existing
    cls = generated_class(bp)
    character = unreal.get_default_object(cls)
    camera = character.get_editor_property("side_camera")
    assert camera, "P1 character missing native SideCamera"
    # The component must belong to the P1 CDO, never to the P0 source CDO.
    assert "BP_NaiwaP1Character" in camera.get_path_name(), camera.get_path_name()
    if created:
        proposal = CONFIG["camera_candidate"]
        camera.set_editor_property(
            "relative_location",
            unreal.Vector(0.0, proposal["distance_cm"], proposal["height_offset_cm"]),
        )
        camera.set_editor_property("relative_rotation", unreal.Rotator(0.0, -90.0, 0.0))
        camera.set_editor_property("field_of_view", proposal["fov_degrees"])
        save(bp)
    position = camera.get_editor_property("relative_location")
    return bp, cls, {
        "created": created,
        "component": camera.get_path_name(),
        "distance_cm": position.y,
        "height_offset_cm": position.z,
        "fov_degrees": camera.get_editor_property("field_of_view"),
        "status": "candidate_pending_visible_window_review" if created else "existing_value_preserved",
    }


def make_game_mode(character_class):
    parent = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1GameMode")
    assert parent, "Build native NaiwaP1GameMode before generating the map"
    controller = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1PlayerController")
    assert controller, "Build native NaiwaP1PlayerController before generating the map"
    bp, created = blueprint("BP_NaiwaP1GameMode", parent)
    cls = generated_class(bp)
    defaults = unreal.get_default_object(cls)
    current_pawn = defaults.get_editor_property("default_pawn_class")
    current_controller = defaults.get_editor_property("player_controller_class")
    if created:
        defaults.set_editor_property("default_pawn_class", character_class)
        defaults.set_editor_property("player_controller_class", controller)
        save(bp)
        cls = generated_class(bp)
    else:
        assert current_pawn and current_pawn.get_path_name() == character_class.get_path_name(), (
            "Existing P1 game mode pawn differs; review it rather than overwriting user changes"
        )
        if current_controller and current_controller.get_path_name() == controller.get_path_name():
            pass
        else:
            # One-time upgrade of a generated slice from the engine default
            # controller. Any customized controller is preserved for review.
            assert current_controller and current_controller.get_path_name() == "/Script/Engine.PlayerController", (
                "Existing P1 game mode controller differs; review it rather than overwriting user changes"
            )
            defaults.set_editor_property("player_controller_class", controller)
            save(bp)
            cls = generated_class(bp)
    actual_controller = unreal.get_default_object(cls).get_editor_property("player_controller_class")
    assert actual_controller and actual_controller.get_path_name() == controller.get_path_name(), (
        "P1 game mode did not retain the P1 camera controller"
    )
    return bp, cls


def material(name, rgb):
    path = ASSET_ROOT + "/Materials/M_P1_" + name
    existing = unreal.load_asset(path)
    if existing:
        assert isinstance(existing, unreal.Material), path + " is not a Material"
        return existing
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_P1_" + name, ASSET_ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew()
    )
    assert mat
    expr = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionConstant3Vector, -350, 0
    )
    expr.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
    assert unreal.MaterialEditingLibrary.connect_material_property(
        expr, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    mat.set_editor_property("two_sided", True)
    errors = unreal.MaterialEditingLibrary.recompile_material(mat)
    assert not errors, str(errors)
    save(mat)
    return mat


def resolve_kind(name):
    enum = getattr(unreal, "NaiwaP1TriggerKind", None) or getattr(unreal, "ENaiwaP1TriggerKind", None)
    assert enum, "NaiwaP1TriggerKind enum unavailable; build P1 C++ first"
    matches = [item for item in dir(enum) if item.replace("_", "").lower() == name.lower()]
    assert len(matches) == 1, "Cannot resolve enum " + name + " from " + str(dir(enum))
    return getattr(enum, matches[0])


def add_owned(actor, label):
    assert actor
    actor.set_actor_label("P1_" + label)
    actor.set_editor_property("tags", [unreal.Name(OWNER_TAG)])
    return actor


def make_block(actors, cube, mats, spec):
    width = float(spec["width"])
    thickness = float(spec["thickness"])
    assert width >= 300.0 and thickness > 0.0
    actor = add_owned(
        actors.spawn_actor_from_class(
            unreal.StaticMeshActor,
            unreal.Vector(float(spec["x"]), 0.0, float(spec["top"]) - thickness / 2.0),
        ),
        "Block_" + spec["id"],
    )
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    assert component
    component.set_static_mesh(cube)
    component.set_material(0, mats[spec["material"]])
    actor.set_actor_scale3d(
        unreal.Vector(width / 100.0, float(spec.get("depth", 700.0)) / 100.0, thickness / 100.0)
    )
    return actor


def make_trigger(actors, native_class, mats, spec):
    assert spec["kind"] in ("Pickup", "Hazard", "Checkpoint", "Goal", "KillZ")
    position = spec["at"]
    extent = spec["extent"]
    actor = add_owned(
        actors.spawn_actor_from_class(
            native_class, unreal.Vector(*position)
        ),
        "Trigger_" + spec["id"],
    )
    actor.set_editor_properties({
        "trigger_kind": resolve_kind(spec["kind"]),
        "trigger_id": unreal.Name(spec["id"]),
        "box_extent": unreal.Vector(*extent),
    })
    if spec["kind"] == "Checkpoint":
        actor.set_editor_property("respawn_offset", unreal.Vector(0.0, 0.0, 0.0))
    if spec.get("material"):
        display = actor.get_editor_property("display_mesh")
        assert display, "P1 trigger DisplayMesh missing"
        display.set_material(0, mats[spec["material"]])
    return actor


def make_hazard_cue(actors, cube, mat):
    # The spike itself starts hidden. This small amber ground marker identifies
    # the dangerous gap without providing collision or revealing the spike.
    # Use a separate tag: the P1 camera treats OWNER_TAG static meshes as terrain.
    actor = actors.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(1510.0, 355.0, 22.0)
    )
    assert actor
    actor.set_actor_label(CUE_LABEL)
    actor.set_editor_property("tags", [unreal.Name(CUE_TAG)])
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    assert component
    component.set_static_mesh(cube)
    component.set_material(0, mat)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    actor.set_actor_scale3d(unreal.Vector(0.70, 0.08, 0.44))
    assert OWNER_TAG not in [str(tag) for tag in actor.get_editor_property("tags")]
    return actor


def make_map(game_mode_class, mats):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    exists = unreal.EditorAssetLibrary.does_asset_exist(MAP)
    if exists:
        assert levels.load_level(MAP)
    else:
        assert levels.new_level(MAP)
    world = unreal.EditorLevelLibrary.get_editor_world()
    settings = world.get_world_settings()
    tags = [str(tag) for tag in settings.get_editor_property("tags")]
    if exists:
        assert MAP_TAG in tags, "Existing map lacks P1 ownership tag; refusing overwrite"
        current = settings.get_editor_property("default_game_mode")
        assert current and current.get_path_name() == game_mode_class.get_path_name(), (
            "Existing map uses an unexpected game mode; refusing overwrite"
        )
        all_actors = actors.get_all_level_actors()
        owned = [actor.get_actor_label() for actor in all_actors
                 if OWNER_TAG in [str(tag) for tag in actor.get_editor_property("tags")]]
        required = {"P1_Start"}
        required.update("P1_Block_" + spec["id"] for spec in CONFIG["slice"]["blocks"])
        required.update("P1_Trigger_" + spec["id"] for spec in CONFIG["slice"]["triggers"])
        if required.issubset(set(owned)):
            cues = [actor for actor in all_actors if actor.get_actor_label() == CUE_LABEL]
            assert len(cues) <= 1, "Duplicate Slice hazard cues"
            if cues:
                assert CUE_TAG in [str(tag) for tag in cues[0].get_editor_property("tags")], (
                    "Hazard cue label exists without P1 Slice ownership tag"
                )
                return {"map": MAP, "status": "existing_map_preserved", "owned_actors": owned,
                        "hazard_cue": CUE_LABEL}
            cube = unreal.load_asset("/Engine/BasicShapes/Cube")
            assert cube
            make_hazard_cue(actors, cube, mats["hazard_cue"])
            assert levels.save_current_level(), "Unable to save Slice hazard cue"
            return {"map": MAP, "status": "slice_hazard_cue_added", "owned_actors": owned,
                    "hazard_cue": CUE_LABEL}
        # The ownership marker is saved before actors are spawned. A failed first
        # build can leave only that marker; it is safe to resume from there.
        assert not owned, (
            "Existing P1 map has a partial set of actors; inspect or version it before rebuilding. Missing: "
            + str(sorted(required.difference(owned)))
        )
    else:
        settings.set_editor_property("tags", [unreal.Name(MAP_TAG)])
        settings.set_editor_property("default_game_mode", game_mode_class)
        assert levels.save_current_level(), "Could not persist new P1 map ownership marker"

    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    assert cube
    blocks = [make_block(actors, cube, mats, spec) for spec in CONFIG["slice"]["blocks"]]
    native_trigger = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaP1Trigger")
    assert native_trigger, "Build native NaiwaP1Trigger before generating the map"
    triggers = [make_trigger(actors, native_trigger, mats, spec) for spec in CONFIG["slice"]["triggers"]]
    make_hazard_cue(actors, cube, mats["hazard_cue"])

    player = add_owned(
        actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(*CONFIG["slice"]["spawn"])),
        "Start",
    )
    light = add_owned(
        actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 800.0)),
        "KeyLight",
    )
    light.set_actor_rotation(unreal.Rotator(-45.0, -30.0, 0.0), False)
    light.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("intensity", 3.0)
    add_owned(actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0.0, 0.0, 600.0)), "SkyLight")
    add_owned(actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0.0, 0.0, 0.0)), "Sky")
    assert player and light
    assert levels.save_current_level(), "Unable to save P1 slice map"
    return {"map": MAP, "status": "new_map_saved",
            "blocks": [a.get_actor_label() for a in blocks],
            "triggers": [a.get_actor_label() for a in triggers],
            "hazard_cue": CUE_LABEL}


report = {"schema": CONFIG["schema"], "engine": unreal.SystemLibrary.get_engine_version(),
          "status": "running", "target_map": MAP}
before = protected_hashes()
full_map_before = file_hash(FULL_MAP_FILE) if FULL_MAP_FILE.exists() else None
try:
    assert all(path.exists() for path in P0_PROTECTED_FILES)
    bp_character, character_class, camera = make_character()
    bp_mode, mode_class = make_game_mode(character_class)
    colors = {
        "ground": (0.10, 0.23, 0.31),
        "platform": (0.23, 0.50, 0.57),
        "pickup": (0.98, 0.72, 0.09),
        "checkpoint": (0.12, 0.85, 0.35),
        "goal": (0.10, 0.45, 0.97),
        "hazard": (0.94, 0.11, 0.16),
        "hazard_cue": (1.0, 0.56, 0.04),
    }
    mats = {name: material(name, color) for name, color in colors.items()}
    report["camera"] = camera
    report["assets"] = {"character": bp_character.get_path_name(),
                        "game_mode": bp_mode.get_path_name(),
                        "player_controller": "/Script/NaiwaRuntime.NaiwaP1PlayerController",
                        "materials": {key: mat.get_path_name() for key, mat in mats.items()}}
    report["layout"] = make_map(mode_class, mats)
    report["p0_protected_sha256"] = before
    assert protected_hashes() == before, "A protected P0 asset changed during P1 generation"
    if full_map_before is not None:
        assert file_hash(FULL_MAP_FILE) == full_map_before, "P1 Full baseline changed during Slice generation"
    report["status"] = "saved_candidate_pending_playable_camera_and_walk_run_review"
    unreal.log("NAIWA_P1_SLICE_BUILD_OK " + MAP)
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    unreal.log_error(report["error"])
    raise
finally:
    OUTPUT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
