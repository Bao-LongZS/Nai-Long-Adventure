"""Read camera defaults without writing any UE asset or level."""

import json
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "UE5/NaiwaP0/Saved/NaiwaP1/camera_audit.json"
OUTPUT.parent.mkdir(parents=True, exist_ok=True)


def camera_values(component):
    if component is None:
        return None
    location = component.get_editor_property("relative_location")
    rotation = component.get_editor_property("relative_rotation")
    return {
        "component_path": component.get_path_name(),
        "relative_location_cm": [location.x, location.y, location.z],
        "relative_rotation_degrees": [rotation.pitch, rotation.yaw, rotation.roll],
        "field_of_view_degrees": component.get_editor_property("field_of_view"),
    }


def blueprint_default(path):
    blueprint = unreal.load_asset(path)
    if blueprint is None:
        return {"path": path, "available": False}
    generated = unreal.BlueprintEditorLibrary.generated_class(blueprint)
    if generated is None:
        return {"path": path, "available": False, "reason": "generated class unavailable"}
    obj = unreal.get_default_object(generated)
    return {"path": path, "available": True, "class": generated.get_path_name(), "default": obj}


report = {
    "engine": unreal.SystemLibrary.get_engine_version(),
    "audit_type": "saved_blueprint_defaults_read_only",
    "template_source_candidates": {
        "cpp_camera_distance_cm": 1000.0,
        "cpp_camera_z_offset_cm": 100.0,
        "cpp_fov_degrees": 65.0,
        "source": "UE_5.8/Templates/TP_ThirdPerson/Source/TP_ThirdPerson/Variant_SideScrolling",
        "warning": "C++ source values are candidates; Blueprint overrides and actual game view must be read separately."
    },
}

p0 = blueprint_default("/Game/Naiwa/Blueprints/BP_NaiwaCharacter")
assert p0["available"], "Saved P0 character blueprint is missing"
report["p0_character"] = {
    "path": p0["path"],
    "class": p0["class"],
    "camera": camera_values(p0["default"].get_editor_property("side_camera")),
}

template_manager = blueprint_default(
    "/Game/Variant_SideScroller/Blueprints/BP_SideScrollingCameraManager"
)
if template_manager["available"]:
    manager = template_manager.pop("default")
    values = {}
    for property_name in ("current_zoom", "camera_z_offset", "camera_x_min_bounds", "camera_x_max_bounds"):
        try:
            values[property_name] = manager.get_editor_property(property_name)
        except Exception as exc:
            values[property_name] = {"unavailable": str(exc)}
    template_manager["camera_properties"] = values
report["workcopy_template_camera_manager"] = template_manager

controller = blueprint_default(
    "/Game/Variant_SideScroller/Blueprints/BP_SideScrollingPlayerController"
)
if controller["available"]:
    controller_default = controller.pop("default")
    try:
        manager_class = controller_default.get_editor_property("player_camera_manager_class")
        controller["camera_manager_class"] = manager_class.get_path_name() if manager_class else None
    except Exception as exc:
        controller["camera_manager_class_error"] = str(exc)
report["workcopy_template_controller"] = controller

game_mode = blueprint_default(
    "/Game/Variant_SideScroller/Blueprints/BP_SideScrollingGameMode"
)
if game_mode["available"]:
    mode_default = game_mode.pop("default")
    for property_name in ("default_pawn_class", "player_controller_class"):
        try:
            value = mode_default.get_editor_property(property_name)
            game_mode[property_name] = value.get_path_name() if value else None
        except Exception as exc:
            game_mode[property_name + "_error"] = str(exc)
report["workcopy_template_game_mode"] = game_mode

report["status"] = "read_only_complete"
OUTPUT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
unreal.log("NAIWA_P1_CAMERA_AUDIT_OK " + str(OUTPUT))
