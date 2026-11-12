"""Create a new isolated review map for the P6 A1 v05 camera composition and dusk lighting."""
from __future__ import annotations

import json
from pathlib import Path
import traceback

import unreal


ROOT = Path(__file__).resolve().parents[2]
IMPORT_REPORT = ROOT / "UE5/Records/p6_art/evidence/P6_A1_CourtyardKit_UEImport_v05_M2Camera.json"
POINT_MAP = ROOT / "UE5/SourceAssets/P6/Manifests/P6_A1_CurrentPointArtMap_v01.json"
MAP_PATH = "/Game/Naiwa/P6/Art/A1/Maps/L_NaiwaP6_A1M2LightingReview_v07"
OWNER_TAG = "NaiwaP6A1M2LightingReviewV07"
REPORT = ROOT / "UE5/Records/p6_art/evidence/P6_A1_M2LightingReviewMap_v07.json"


def spawn(actor_class, label, location=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0)):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class, unreal.Vector(*location), unreal.Rotator(*rotation)
    )
    if not actor:
        raise RuntimeError("Could not spawn " + label)
    actor.set_actor_label(label)
    return actor


def configure_directional(actor, intensity, cast_shadows, color, temperature):
    component = actor.get_component_by_class(unreal.DirectionalLightComponent)
    component.set_editor_properties({
        "intensity": intensity,
        "cast_shadows": cast_shadows,
        "light_color": color,
        "use_temperature": True,
        "temperature": temperature,
        "light_source_angle": 7.0,
    })
    return component


report = {
    "status": "running",
    "engine": unreal.SystemLibrary.get_engine_version(),
    "map_asset": MAP_PATH,
    "owner_tag": OWNER_TAG,
    "review_only": True,
}

try:
    if not IMPORT_REPORT.is_file() or not POINT_MAP.is_file():
        raise RuntimeError("v05 import or current M2 camera report is missing")
    import_report = json.loads(IMPORT_REPORT.read_text(encoding="utf-8"))
    point_map = json.loads(POINT_MAP.read_text(encoding="utf-8"))
    if import_report.get("status") != "imported":
        raise RuntimeError("v05 StaticMesh import did not pass")
    mesh_path = import_report["mesh_asset"]
    mesh = unreal.load_asset(mesh_path)
    if not mesh or not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("v05 StaticMesh is missing: " + mesh_path)
    camera_config = point_map["camera"]
    location = tuple(float(value) for value in camera_config["location"])
    pitch_yaw_roll = tuple(float(value) for value in camera_config["rotation_deg"])
    unreal_rotation = (pitch_yaw_roll[2], pitch_yaw_roll[0], pitch_yaw_roll[1])
    ortho_width = float(camera_config["ortho_width_cm"])
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("Review map already exists; refusing to overwrite: " + MAP_PATH)

    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.new_level(MAP_PATH):
        raise RuntimeError("Could not create the isolated v07 review map")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("tags", [unreal.Name(OWNER_TAG)])
    world.get_world_settings().set_editor_property(
        "default_game_mode", unreal.load_class(None, "/Script/Engine.GameModeBase")
    )

    mesh_actor = spawn(unreal.StaticMeshActor, "NAIWA_P6_A1_v05_M2Camera_CourtyardKit")
    mesh_component = mesh_actor.get_component_by_class(unreal.StaticMeshComponent)
    mesh_component.set_static_mesh(mesh)
    mesh_component.set_editor_properties({
        "mobility": unreal.ComponentMobility.STATIC,
        "cast_shadow": True,
    })
    mesh_component.set_collision_profile_name("NoCollision")
    mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

    key = spawn(unreal.DirectionalLight, "NAIWA_P6_A1_v07_CoolMoonKey", (0.0, 0.0, 4200.0), (0.0, -48.0, -36.0))
    configure_directional(key, 2200.0, True, unreal.Color(180, 204, 255, 255), 8200.0)
    fill = spawn(unreal.DirectionalLight, "NAIWA_P6_A1_v07_CameraFill", (0.0, 0.0, 3900.0), (0.0, -22.0, 148.0))
    configure_directional(fill, 520.0, False, unreal.Color(169, 199, 255, 255), 9500.0)

    sky = spawn(unreal.SkyLight, "NAIWA_P6_A1_v07_SkyFill", (0.0, 0.0, 3500.0))
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_editor_properties({
        "intensity": 0.14,
        "source_type": unreal.SkyLightSourceType.SLS_CAPTURED_SCENE,
        "cast_shadows": False,
    })
    spawn(unreal.SkyAtmosphere, "NAIWA_P6_A1_v07_SkyAtmosphere")

    lanterns = [
        ("HouseLeft", (620.0, -1250.0, 980.0)),
        ("HouseRight", (2920.0, -1250.0, 980.0)),
        ("StageLeft", (-2440.0, -645.0, 915.0)),
        ("StageRight", (-460.0, -645.0, 915.0)),
    ]
    for suffix, point in lanterns:
        actor = spawn(unreal.PointLight, "NAIWA_P6_A1_v07_LanternBounce_" + suffix, point)
        component = actor.get_component_by_class(unreal.PointLightComponent)
        component.set_editor_properties({
            "intensity": 28000.0,
            "attenuation_radius": 1200.0,
            "light_color": unreal.Color(255, 157, 83, 255),
            "cast_shadows": False,
            "use_temperature": True,
            "temperature": 2700.0,
        })

    volume = spawn(unreal.PostProcessVolume, "NAIWA_P6_A1_v07_ExposureAndTone", (0.0, 0.0, 0.0))
    volume.set_editor_properties({"unbound": True, "blend_weight": 1.0, "blend_radius": 0.0})
    settings = volume.get_editor_property("settings")
    for prop in ("override_auto_exposure_method", "b_override_auto_exposure_method"):
        try:
            settings.set_editor_property(prop, True)
            break
        except Exception:
            pass
    settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
    for prop in ("override_auto_exposure_bias", "b_override_auto_exposure_bias"):
        try:
            settings.set_editor_property(prop, True)
            break
        except Exception:
            pass
    settings.set_editor_property("auto_exposure_bias", -0.65)
    for prop in ("override_color_saturation", "b_override_color_saturation"):
        try:
            settings.set_editor_property(prop, True)
            break
        except Exception:
            pass
    settings.set_editor_property("color_saturation", unreal.Vector4(1.08, 1.03, 1.12, 1.0))
    volume.set_editor_property("settings", settings)

    camera_actor = spawn(unreal.CameraActor, "NAIWA_P6_A1_v07_M2OrthoCamera", location, unreal_rotation)
    camera = camera_actor.get_component_by_class(unreal.CameraComponent)
    camera.set_editor_properties({
        "projection_mode": unreal.CameraProjectionMode.ORTHOGRAPHIC,
        "ortho_width": ortho_width,
        "constrain_aspect_ratio": False,
    })
    sky_component.recapture_sky()
    try:
        unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(*location), unreal.Rotator(*unreal_rotation))
    except Exception:
        pass
    if not levels.save_current_level():
        raise RuntimeError("Could not save the v07 review map")

    report.update({
        "status": "isolated_review_map_saved",
        "mesh_asset": mesh_path,
        "source_composition_offsets_cm": {"house_y": -2250.0, "stage_y": -2050.0, "tree_y": -450.0},
        "lighting": {
            "cool_key_lux": 2200.0,
            "camera_fill_lux": 520.0,
            "skylight_multiplier": 0.14,
            "lantern_point_lights": len(lanterns),
            "lantern_intensity_cd_each": 28000.0,
            "lantern_attenuation_radius_cm": 1200.0,
            "exposure": "manual, -0.65 EV",
            "color_saturation_rgba": [1.08, 1.03, 1.12, 1.0],
        },
        "camera": {
            "position_cm": list(location),
            "rotation_deg_pitch_yaw_roll": list(pitch_yaw_roll),
            "ortho_width_cm": ortho_width,
        },
        "m2_camera_matches_point_map": True,
        "m2_map_modified": False,
        "interaction_anchors_modified": False,
        "review_only": True,
    })
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()

REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
if report["status"] != "isolated_review_map_saved":
    unreal.log_error(report.get("error", "P6 A1 v07 review map build failed"))
    raise RuntimeError("P6 A1 v07 map build failed; inspect " + str(REPORT))
unreal.log("NAIWA_P6_A1_M2_REVIEW_V07_READY " + json.dumps(report, ensure_ascii=False))
