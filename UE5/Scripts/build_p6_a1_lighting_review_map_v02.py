"""Create an isolated, bright UE review map for the imported P6 A1 v04 kit."""
from __future__ import annotations

import json
from pathlib import Path
import traceback

import unreal


ROOT = Path(__file__).resolve().parents[2]
IMPORT_REPORT = ROOT / "UE5/Records/p6_art/evidence/P6_A1_CourtyardKit_UEImport_v04.json"
MAP_PATH = "/Game/Naiwa/P6/Art/A1/Maps/L_NaiwaP6_A1LightingReview_v02"
OWNER_TAG = "NaiwaP6A1LightingReviewV02"
REPORT = ROOT / "UE5/Records/p6_art/evidence/P6_A1_LightingReviewMap_v02.json"


def set_first_property(target, names, value):
    last_error = None
    for name in names:
        try:
            target.set_editor_property(name, value)
            return name
        except Exception as error:
            last_error = error
    raise RuntimeError("Could not set any of " + repr(names) + ": " + str(last_error))


def spawn(actor_class, label, location=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0)):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class,
        unreal.Vector(*location),
        unreal.Rotator(*rotation),
    )
    if not actor:
        raise RuntimeError("Could not spawn " + label)
    actor.set_actor_label(label)
    return actor


def configure_directional(actor, intensity, cast_shadows, color):
    component = actor.get_component_by_class(unreal.DirectionalLightComponent)
    component.set_editor_properties({
        "intensity": intensity,
        "cast_shadows": cast_shadows,
        "light_color": color,
        "use_temperature": True,
        "temperature": 5400.0,
        "light_source_angle": 3.0,
    })
    return component


report = {
    "status": "running",
    "engine": unreal.SystemLibrary.get_engine_version(),
    "map_asset": MAP_PATH,
    "owner_tag": OWNER_TAG,
}

try:
    if not IMPORT_REPORT.is_file():
        raise RuntimeError("Run the v04 mesh importer first; report missing: " + str(IMPORT_REPORT))
    import_report = json.loads(IMPORT_REPORT.read_text(encoding="utf-8"))
    if import_report.get("status") != "imported" or not import_report.get("mesh_asset"):
        raise RuntimeError("The A1 v04 mesh import report is not successful")
    mesh_path = import_report["mesh_asset"]
    report["mesh_asset"] = mesh_path
    mesh = unreal.load_asset(mesh_path)
    if not mesh or not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Imported A1 StaticMesh is missing: " + mesh_path)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("Review map already exists; refusing to overwrite: " + MAP_PATH)

    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.new_level(MAP_PATH):
        raise RuntimeError("Could not create isolated A1 review map")
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    settings = world.get_world_settings()
    settings.set_editor_property("tags", [unreal.Name(OWNER_TAG)])
    settings.set_editor_property("default_game_mode", unreal.load_class(None, "/Script/Engine.GameModeBase"))

    mesh_actor = spawn(unreal.StaticMeshActor, "NAIWA_P6_A1_v04_CourtyardKit")
    mesh_component = mesh_actor.get_component_by_class(unreal.StaticMeshComponent)
    mesh_component.set_static_mesh(mesh)
    mesh_component.set_editor_properties({
        "mobility": unreal.ComponentMobility.STATIC,
        "cast_shadow": True,
    })
    mesh_component.set_collision_profile_name("NoCollision")
    mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

    key = spawn(unreal.DirectionalLight, "NAIWA_P6_A1_Review_Key_Sun", (0.0, 0.0, 4200.0), (-48.0, -36.0, 0.0))
    configure_directional(key, 65000.0, True, unreal.Color(255, 240, 209, 255))
    fill = spawn(unreal.DirectionalLight, "NAIWA_P6_A1_Review_CameraFill", (0.0, 0.0, 3900.0), (-22.0, 148.0, 0.0))
    configure_directional(fill, 18000.0, False, unreal.Color(179, 209, 255, 255))

    sky = spawn(unreal.SkyLight, "NAIWA_P6_A1_Review_SkyFill", (0.0, 0.0, 3500.0))
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_editor_properties({
        "intensity": 1.45,
        "source_type": unreal.SkyLightSourceType.SLS_CAPTURED_SCENE,
        "cast_shadows": False,
    })
    spawn(unreal.SkyAtmosphere, "NAIWA_P6_A1_Review_SkyAtmosphere")

    lanterns = [
        ("HouseLeft", (620.0, 1000.0, 980.0)),
        ("HouseRight", (2920.0, 1000.0, 980.0)),
        ("StageLeft", (-2440.0, 1405.0, 915.0)),
        ("StageRight", (-460.0, 1405.0, 915.0)),
    ]
    for suffix, location in lanterns:
        actor = spawn(unreal.PointLight, "NAIWA_P6_A1_Review_LanternGlow_" + suffix, location)
        component = actor.get_component_by_class(unreal.PointLightComponent)
        component.set_editor_properties({
            "intensity": 180.0,
            "attenuation_radius": 520.0,
            "light_color": unreal.Color(255, 168, 87, 255),
            "cast_shadows": False,
            "use_temperature": True,
            "temperature": 2850.0,
        })

    volume = spawn(unreal.PostProcessVolume, "NAIWA_P6_A1_Review_Exposure", (0.0, 0.0, 0.0))
    volume.set_editor_properties({"unbound": True, "blend_weight": 1.0, "blend_radius": 0.0})
    pp = volume.get_editor_property("settings")
    exposure_method = unreal.AutoExposureMethod.AEM_MANUAL
    set_first_property(pp, ("override_auto_exposure_method", "b_override_auto_exposure_method"), True)
    pp.set_editor_property("auto_exposure_method", exposure_method)
    set_first_property(pp, ("override_auto_exposure_bias", "b_override_auto_exposure_bias"), True)
    pp.set_editor_property("auto_exposure_bias", 0.65)
    set_first_property(pp, ("override_color_saturation", "b_override_color_saturation"), True)
    pp.set_editor_property("color_saturation", unreal.Vector4(1.05, 1.05, 1.05, 1.0))
    volume.set_editor_property("settings", pp)

    camera_actor = spawn(
        unreal.CameraActor,
        "NAIWA_P6_A1_Review_OrthoCamera",
        (0.0, 5500.0, 5400.0),
        (-40.0, -90.0, 0.0),
    )
    camera = camera_actor.get_component_by_class(unreal.CameraComponent)
    camera.set_editor_properties({
        "projection_mode": unreal.CameraProjectionMode.ORTHOGRAPHIC,
        "ortho_width": 12000.0,
        "constrain_aspect_ratio": False,
    })
    sky_component.recapture_sky()

    try:
        unreal.EditorLevelLibrary.set_level_viewport_camera_info(
            unreal.Vector(0.0, 5500.0, 5400.0), unreal.Rotator(-40.0, -90.0, 0.0)
        )
    except Exception:
        pass
    if not levels.save_current_level():
        raise RuntimeError("Could not save A1 lighting review map")

    report.update({
        "status": "isolated_review_map_saved",
        "lighting": {
            "key_sun_lux": 65000.0,
            "camera_fill_lux": 18000.0,
            "skylight_multiplier": 1.45,
            "lantern_point_lights": len(lanterns),
            "exposure": "manual, +0.65 EV",
        },
        "camera": {"position_cm": [0.0, 5500.0, 5400.0], "rotation_deg": [-40.0, -90.0, 0.0], "ortho_width_cm": 12000.0},
        "runtime_game_mode": "/Script/Engine.GameModeBase",
        "m2_map_modified": False,
    })
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()

REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
if report["status"] != "isolated_review_map_saved":
    unreal.log_error(report.get("error", "P6 A1 review map build failed"))
    raise RuntimeError("P6 A1 lighting review map failed; see " + str(REPORT))
unreal.log("NAIWA_P6_A1_LIGHTING_REVIEW_READY " + json.dumps(report, ensure_ascii=False))
