"""Import the merged P6 A1 v04 OBJ as one editable UE static mesh.

The canonical 1,523-object OBJ remains untouched. This companion OBJ retains
its geometry and 40 used material slots while presenting one object to UE.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import traceback

import unreal


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "UE5/SourceAssets/P6/Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_v04_UECombined.obj"
SOURCE_MTL = SOURCE.with_suffix(".mtl")
DEST = "/Game/Naiwa/P6/Art/A1/CourtyardKit_v04"
REPORT = ROOT / "UE5/Records/p6_art/evidence/P6_A1_CourtyardKit_UEImport_v04.json"
ENGINE_MESH_PATH = DEST + "/P6_A1_CourtyardKit_v04_UECombined"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


report = {
    "status": "running",
    "engine": unreal.SystemLibrary.get_engine_version(),
    "source_obj": str(SOURCE),
    "source_mtl": str(SOURCE_MTL),
    "destination": DEST,
}

try:
    if not SOURCE.is_file() or not SOURCE_MTL.is_file():
        raise FileNotFoundError("The generated combined v04 OBJ/MTL pair is incomplete")

    source_lines = SOURCE.read_text(encoding="utf-8").splitlines()
    source_object_groups = sum(line.startswith("o ") for line in source_lines)
    source_vertices = sum(line.startswith("v ") for line in source_lines)
    source_uvs = sum(line.startswith("vt ") for line in source_lines)
    source_faces = sum(line.startswith("f ") for line in source_lines)
    faces_with_uvs = sum(
        line.startswith("f ") and all("/" in token for token in line.split()[1:])
        for line in source_lines
    )
    source_materials = {line[7:].strip() for line in source_lines if line.startswith("usemtl ")}
    source_material_batches = sum(line.startswith("usemtl ") for line in source_lines)
    if (
        source_object_groups, source_vertices, source_uvs, source_faces,
        faces_with_uvs, len(source_materials), source_material_batches
    ) != (1, 54944, 54944, 49534, 49534, 40, 40):
        raise RuntimeError(
            "Combined v04 source geometry/material preflight changed unexpectedly: " +
            repr((source_object_groups, source_vertices, source_uvs, source_faces, faces_with_uvs, len(source_materials), source_material_batches))
        )

    expected_source = str(SOURCE.resolve()).casefold()
    existing = unreal.load_asset(ENGINE_MESH_PATH)
    if existing and not isinstance(existing, unreal.StaticMesh):
        raise RuntimeError("The expected UE destination exists but is not a StaticMesh")
    if existing:
        import_data = existing.get_editor_property("asset_import_data")
        imported_from = import_data.get_first_filename() if import_data else ""
        if not imported_from or str(Path(imported_from).resolve()).casefold() != expected_source:
            raise RuntimeError("Refusing to replace an existing mesh with a different or unknown source")
        import_status = "reimported_same_source"
    else:
        unreal.EditorAssetLibrary.make_directory(DEST)
        import_status = "imported"

    task = unreal.AssetImportTask()
    task.set_editor_properties({
        "filename": str(SOURCE),
        "destination_path": DEST,
        "destination_name": "P6_A1_CourtyardKit_v04_UECombined",
        "replace_existing": bool(existing),
        "automated": True,
        "save": True,
    })
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported_paths = list(task.get_editor_property("imported_object_paths"))
    mesh = unreal.load_asset(ENGINE_MESH_PATH)
    candidates = []
    for asset_path in unreal.EditorAssetLibrary.list_assets(DEST, recursive=True, include_folder=False):
        candidate = unreal.load_asset(asset_path)
        if isinstance(candidate, unreal.StaticMesh):
            candidates.append(candidate)
    if len(candidates) != 1:
        raise RuntimeError(
            "Expected one combined StaticMesh, found " + str(len(candidates)) +
            "; imported paths=" + json.dumps(imported_paths, ensure_ascii=False)
        )
    if not mesh:
        mesh = candidates[0]
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("OBJ import did not produce a StaticMesh: " + mesh.get_path_name())

    if not unreal.EditorAssetLibrary.save_asset(mesh.get_path_name(), only_if_is_dirty=False):
        raise RuntimeError("Could not save imported StaticMesh: " + mesh.get_path_name())

    materials = []
    for slot in mesh.get_editor_property("static_materials"):
        material = slot.get_editor_property("material_interface")
        materials.append(material.get_path_name() if material else None)
    bounds = mesh.get_bounds()
    origin = bounds.origin
    extent = bounds.box_extent
    mesh_bounds = {
        "x": [origin.x - extent.x, origin.x + extent.x],
        "y": [origin.y - extent.y, origin.y + extent.y],
        "z": [origin.z - extent.z, origin.z + extent.z],
    }
    report.update({
        "status": "imported",
        "result": import_status,
        "mesh_asset": mesh.get_path_name(),
        "source_obj_sha256": sha256(SOURCE),
        "source_mtl_sha256": sha256(SOURCE_MTL),
        "source_object_groups_merged": 1523,
        "combined_source_object_groups": source_object_groups,
        "axis_policy": "UE OBJ Y handedness flip pre-compensated in the UE companion source; canonical coordinates unchanged",
        "imported_mesh_bounds_cm": mesh_bounds,
        "vertex_count": source_vertices,
        "preview_uv_count": source_uvs,
        "preview_uv_policy": "generated planar XY import-preview UVs only; not final unwrap",
        "face_count": source_faces,
        "used_material_count": len(source_materials),
        "source_material_batch_count": source_material_batches,
        "lod_count": mesh.get_num_lods(),
        "lod0_fallback_vertex_count": mesh.get_num_vertices(0),
        "lod0_fallback_triangle_count": mesh.get_num_triangles(0),
        "nanite_vertex_count": mesh.get_num_nanite_vertices(),
        "nanite_triangle_count": mesh.get_num_nanite_triangles(),
        "lod0_section_count": mesh.get_num_sections(0),
        "imported_static_mesh_count": len(candidates),
        "material_slot_count": len(materials),
        "material_slots": materials,
        "imported_object_paths": imported_paths,
    })
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()

REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
if report["status"] != "imported":
    unreal.log_error(report.get("error", "P6 A1 import failed"))
    raise RuntimeError("P6 A1 import failed; see " + str(REPORT))
unreal.log("NAIWA_P6_A1_V04_IMPORTED " + json.dumps(report, ensure_ascii=False))
