"""Read-only UE Python exporter for the current P1 v7 map."""

import hashlib
import json
import math
import os
import re

import unreal


def argument(name, default=None):
    command_line = unreal.SystemLibrary.get_command_line()
    match = re.search(r"(?:^|\s)-" + re.escape(name) + r"=(?:\"([^\"]+)\"|([^\s]+))", command_line)
    if not match:
        if default is not None:
            return default
        raise RuntimeError("Missing command-line argument: -{}=".format(name))
    return match.group(1) or match.group(2)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def vec3(value):
    return [float(value.x), float(value.y), float(value.z)]


def path_of(value):
    if value is None:
        return None
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def rotate_vector(vector, rotation):
    roll = math.radians(float(rotation.roll))
    pitch = math.radians(float(rotation.pitch))
    yaw = math.radians(float(rotation.yaw))
    cr, sr = math.cos(roll), math.sin(roll)
    cp, sp = math.cos(pitch), math.sin(pitch)
    cy, sy = math.cos(yaw), math.sin(yaw)
    x, y, z = vector
    return [
        (cy * cp) * x + (cy * sp * sr - sy * cr) * y + (cy * sp * cr + sy * sr) * z,
        (sy * cp) * x + (sy * sp * sr + cy * cr) * y + (sy * sp * cr - cy * sr) * z,
        (-sp) * x + (cp * sr) * y + (cp * cr) * z,
    ]


def static_mesh_bounds(component):
    try:
        mesh = component.get_editor_property("static_mesh")
        if mesh is None:
            return None
        for property_name in ("extended_bounds", "bounds"):
            try:
                bounds = mesh.get_editor_property(property_name)
                origin = bounds.get_editor_property("origin")
                extent = bounds.get_editor_property("box_extent")
                local_origin = [float(origin.x), float(origin.y), float(origin.z)]
                local_extent = [float(extent.x), float(extent.y), float(extent.z)]
                scale = vec3(component.get_world_scale())
                scaled_origin = [local_origin[i] * scale[i] for i in range(3)]
                scaled_extent = [abs(local_extent[i] * scale[i]) for i in range(3)]
                rotation = component.get_world_rotation()
                rotated_origin = rotate_vector(scaled_origin, rotation)
                location = vec3(component.get_world_location())
                matrix_columns = [
                    rotate_vector([1, 0, 0], rotation),
                    rotate_vector([0, 1, 0], rotation),
                    rotate_vector([0, 0, 1], rotation),
                ]
                world_extent = [sum(abs(matrix_columns[j][i]) * scaled_extent[j] for j in range(3))
                                for i in range(3)]
                return {
                    "originCm": [location[i] + rotated_origin[i] for i in range(3)],
                    "extentCm": world_extent,
                    "source": "StaticMesh.{}.box_extent transformed by component world transform".format(property_name),
                }
            except Exception:
                continue
    except Exception:
        pass
    try:
        mesh = component.get_editor_property("static_mesh")
        mesh_path = path_of(mesh)
        # UE's Engine/BasicShapes assets are 100 cm primitives centered at the origin.
        # The current P1 v7 map uses only these three primitives for visible geometry.
        if mesh_path in ("/Engine/BasicShapes/Cube.Cube",
                         "/Engine/BasicShapes/Sphere.Sphere",
                         "/Engine/BasicShapes/Cone.Cone"):
            scale = vec3(component.get_world_scale())
            rotation = component.get_world_rotation()
            local_extent = [50.0, 50.0, 50.0]
            scaled_extent = [abs(local_extent[i] * scale[i]) for i in range(3)]
            matrix_columns = [
                rotate_vector([1, 0, 0], rotation),
                rotate_vector([0, 1, 0], rotation),
                rotate_vector([0, 0, 1], rotation),
            ]
            world_extent = [sum(abs(matrix_columns[j][i]) * scaled_extent[j] for j in range(3))
                            for i in range(3)]
            return {
                "originCm": vec3(component.get_world_location()),
                "extentCm": world_extent,
                "source": "UE Engine BasicShapes centered 100 cm primitive bounds",
            }
    except Exception:
        pass
    return None
    return None


def prop(obj, name, convert=None):
    try:
        value = obj.get_editor_property(name)
        if convert is not None:
            return convert(value)
        if isinstance(value, (bool, int, float, str)) or value is None:
            return value
        if hasattr(value, "x") and hasattr(value, "y") and hasattr(value, "z"):
            return vec3(value)
        if hasattr(value, "get_path_name"):
            return path_of(value)
        return str(value)
    except Exception:
        return None


def bounds_of(component):
    try:
        origin, extent, sphere_radius = component.get_component_bounds()
        return {
            "originCm": vec3(origin),
            "extentCm": vec3(extent),
            "sphereRadiusCm": float(sphere_radius),
        }
    except Exception:
        pass
    try:
        bounds = component.get_editor_property("bounds")
        origin = bounds.get_editor_property("origin")
        extent = bounds.get_editor_property("box_extent")
        sphere_radius = bounds.get_editor_property("sphere_radius")
        return {
            "originCm": vec3(origin),
            "extentCm": vec3(extent),
            "sphereRadiusCm": float(sphere_radius),
        }
    except Exception:
        pass
    try:
        extent = component.get_editor_property("box_extent")
        scale = component.get_world_scale()
        scaled_extent = [abs(float(extent.x) * float(scale.x)),
                         abs(float(extent.y) * float(scale.y)),
                         abs(float(extent.z) * float(scale.z))]
        return {
            "originCm": vec3(component.get_world_location()),
            "extentCm": scaled_extent,
            "sphereRadiusCm": max(scaled_extent),
            "source": "BoxComponent.box_extent transformed by component scale",
        }
    except Exception as error:
        mesh_bounds = static_mesh_bounds(component)
        if mesh_bounds is not None:
            return mesh_bounds
        return {"unavailable": str(error)}


def component_record(component):
    record = {
        "name": component.get_name(),
        "class": component.get_class().get_path_name(),
        "locationCm": vec3(component.get_world_location()),
        "rotationDeg": [float(component.get_world_rotation().roll),
                        float(component.get_world_rotation().pitch),
                        float(component.get_world_rotation().yaw)],
        "scale": vec3(component.get_world_scale()),
        "bounds": bounds_of(component),
        "collisionEnabled": prop(component, "collision_enabled", str),
        "collisionProfile": prop(component, "collision_profile_name", str),
        "visible": prop(component, "visible", bool),
        "mobility": prop(component, "mobility", str),
    }
    static_mesh = prop(component, "static_mesh", path_of)
    if static_mesh:
        record["staticMesh"] = static_mesh
    skeletal_mesh = prop(component, "skeletal_mesh", path_of)
    if skeletal_mesh:
        record["skeletalMesh"] = skeletal_mesh
    materials = []
    try:
        for index in range(component.get_num_materials()):
            materials.append(path_of(component.get_material(index)))
    except Exception:
        pass
    if materials:
        record["materials"] = materials
    return record


def actor_record(actor):
    rotation = actor.get_actor_rotation()
    tags = []
    try:
        tags = sorted(str(tag) for tag in actor.get_editor_property("tags"))
    except Exception:
        pass
    actor_class = actor.get_class().get_path_name()
    components = []
    for component in actor.get_components_by_class(unreal.PrimitiveComponent):
        components.append(component_record(component))
    components.sort(key=lambda item: item["name"])
    properties = {}
    for name in (
        "trigger_kind", "trigger_id", "pickup_order", "required_for_goal", "box_extent",
        "respawn_offset", "reveal_on_spawn", "use_spike_cluster", "show_checkpoint_label",
        "platform_top_z", "platform_extent", "travel_speed", "travel_speed_variation",
        "stable_boarding_seconds", "departure_cue_seconds", "left_dock_x", "right_dock_x",
        "travel_distance", "bounce_velocity_z", "bounce_horizontal_scale",
    ):
        value = prop(actor, name)
        if value is not None:
            properties[name] = value
    return {
        "id": actor.get_path_name(),
        "label": actor.get_actor_label(),
        "path": actor.get_path_name(),
        "class": actor_class,
        "tags": tags,
        "locationCm": vec3(actor.get_actor_location()),
        "rotationDeg": [float(rotation.roll), float(rotation.pitch), float(rotation.yaw)],
        "scale": vec3(actor.get_actor_scale3d()),
        "properties": properties,
        "components": components,
    }


project_root = argument("NaiwaWebRepoRoot")
map_asset = argument("NaiwaWebP1Map", "/Game/Naiwa/P1/Maps/L_NaiwaP1_v7")
config_path = argument("NaiwaWebP1Config")
map_file = argument("NaiwaWebP1MapFile")
output_path = os.path.abspath(argument("NaiwaWebP1Output"))
web_root = os.path.abspath(os.path.join(project_root, "WebNative"))
if os.path.commonpath([web_root, output_path]) != web_root:
    raise RuntimeError("P1 output must stay inside WebNative: {}".format(output_path))

with open(config_path, "r", encoding="utf-8") as stream:
    config = json.load(stream)

unreal.log("Loading P1 map read-only: {}".format(map_asset))
world = unreal.EditorLoadingAndSavingUtils.load_map(map_asset)
if world is None:
    raise RuntimeError("Failed to load P1 map: {}".format(map_asset))

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actor_subsystem.get_all_level_actors()
records = [actor_record(actor) for actor in actors if actor is not None]
records.sort(key=lambda item: (item["locationCm"][0], item["label"], item["id"]))

map_tags = []
try:
    map_tags = sorted(str(tag) for tag in world.get_world_settings().get_editor_property("tags"))
except Exception:
    pass

payload = {
    "schemaVersion": 1,
    "levelId": "1-1",
    "sourceMap": map_asset,
    "coordinateSystem": "UE X/Z side plane, Y depth; centimeters in source fields; world units are meters in derived fields",
    "engineVersion": unreal.SystemLibrary.get_engine_version(),
    "mapTags": map_tags,
    "killZCm": float(config.get("kill_z", -1000)),
    "requiredPickupCount": int(config.get("required_pickup_count", 1)),
    "configCheckpoints": config.get("checkpoints", []),
    "configExtensionActors": config.get("extension_actors", []),
    "actors": records,
    "recordCount": len(records),
    "sourceHashes": {
        "mapSha256": sha256(map_file),
        "configSha256": sha256(config_path),
    },
}

os.makedirs(os.path.dirname(output_path), exist_ok=True)
with open(output_path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(payload, stream, ensure_ascii=False, indent=2)
    stream.write("\n")

unreal.log("NAIWA_WEB_P1_EXPORT map={} actors={} output={}".format(map_asset, len(records), output_path))
