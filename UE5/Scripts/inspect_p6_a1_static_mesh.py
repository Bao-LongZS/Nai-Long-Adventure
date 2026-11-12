"""Inspect the imported P6 A1 mesh's built UE LOD data and record evidence."""
from __future__ import annotations

import json
from pathlib import Path
import traceback

import unreal


ROOT = Path(__file__).resolve().parents[2]
MESH_PATH = "/Game/Naiwa/P6/Art/A1/CourtyardKit_v04/P6_A1_CourtyardKit_v04_UECombined.P6_A1_CourtyardKit_v04_UECombined"
REPORT = ROOT / "UE5/Records/p6_art/evidence/P6_A1_StaticMeshInspection_v01.json"


def safe_value(target, name):
    try:
        value = getattr(target, name)
        if callable(value):
            return {"callable": True}
        return str(value)
    except Exception as error:
        return {"error": str(error)}


report = {"status": "running", "engine": unreal.SystemLibrary.get_engine_version(), "mesh_asset": MESH_PATH}
try:
    mesh = unreal.load_asset(MESH_PATH)
    if not mesh or not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Imported A1 StaticMesh is missing")

    exposed_methods = [
        name for name in dir(mesh)
        if any(token in name.lower() for token in ("triangle", "vertex", "section", "lod", "nanite", "bound"))
    ]
    calls = {}
    for method_name in ("get_num_lods", "get_num_triangles", "get_num_vertices", "get_num_sections"):
        method = getattr(mesh, method_name, None)
        if not callable(method):
            calls[method_name] = {"available": False}
            continue
        result = {"available": True}
        for args, key in (((), "no_args"), ((0,), "lod_0")):
            try:
                result[key] = method(*args)
            except Exception as error:
                result[key] = {"error": str(error)}
        calls[method_name] = result

    property_values = {}
    for property_name in ("render_data", "source_models", "nanite_settings", "bounds", "static_materials"):
        try:
            value = mesh.get_editor_property(property_name)
            if property_name == "static_materials":
                property_values[property_name] = len(value)
            elif property_name == "source_models":
                property_values[property_name] = len(value)
            else:
                property_values[property_name] = str(value)
        except Exception as error:
            property_values[property_name] = {"error": str(error)}

    report.update({
        "status": "inspected",
        "mesh_asset": mesh.get_path_name(),
        "bounds": str(mesh.get_bounds()) if callable(getattr(mesh, "get_bounds", None)) else None,
        "api_calls": calls,
        "relevant_exposed_methods": exposed_methods,
        "properties": property_values,
    })
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()

REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
if report["status"] != "inspected":
    unreal.log_error(report.get("error", "P6 A1 inspection failed"))
    raise RuntimeError("P6 A1 StaticMesh inspection failed; see " + str(REPORT))
unreal.log("NAIWA_P6_A1_MESH_INSPECTED " + json.dumps(report, ensure_ascii=False))
