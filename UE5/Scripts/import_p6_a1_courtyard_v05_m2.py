"""Import the isolated P6 A1 v05 M2-camera composition candidate."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import traceback

import unreal


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "UE5/SourceAssets/P6/Environment/A1_CourtyardKit/Models/P6_A1_CourtyardKit_v05_M2Camera_UECombined.obj"
SOURCE_MTL = SOURCE.with_suffix(".mtl")
DEST = "/Game/Naiwa/P6/Art/A1/CourtyardKit_v05_M2Camera"
REPORT = ROOT / "UE5/Records/p6_art/evidence/P6_A1_CourtyardKit_UEImport_v05_M2Camera.json"
ENGINE_MESH_PATH = DEST + "/P6_A1_CourtyardKit_v05_M2Camera_UECombined"


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
    "review_only": True,
}

try:
    if not SOURCE.is_file() or not SOURCE_MTL.is_file():
        raise FileNotFoundError("The v05 M2 camera OBJ/MTL pair is incomplete")
    lines = SOURCE.read_text(encoding="utf-8").splitlines()
    groups = sum(line.startswith("o ") for line in lines)
    vertices = sum(line.startswith("v ") for line in lines)
    uvs = sum(line.startswith("vt ") for line in lines)
    faces = sum(line.startswith("f ") for line in lines)
    materials = {line[7:].strip() for line in lines if line.startswith("usemtl ")}
    batches = sum(line.startswith("usemtl ") for line in lines)
    expected = (1, 54944, 54944, 49534, 40, 40)
    actual = (groups, vertices, uvs, faces, len(materials), batches)
    if actual != expected:
        raise RuntimeError("v05 source preflight changed unexpectedly: " + repr(actual))

    expected_source = str(SOURCE.resolve()).casefold()
    existing = unreal.load_asset(ENGINE_MESH_PATH)
    if existing and not isinstance(existing, unreal.StaticMesh):
        raise RuntimeError("The v05 destination exists but is not a StaticMesh")
    if existing:
        import_data = existing.get_editor_property("asset_import_data")
        imported_from = import_data.get_first_filename() if import_data else ""
        if not imported_from or str(Path(imported_from).resolve()).casefold() != expected_source:
            raise RuntimeError("Refusing to replace an asset with a different or unknown source")
        import_status = "reimported_same_source"
    else:
        unreal.EditorAssetLibrary.make_directory(DEST)
        import_status = "imported"

    task = unreal.AssetImportTask()
    task.set_editor_properties({
        "filename": str(SOURCE),
        "destination_path": DEST,
        "destination_name": "P6_A1_CourtyardKit_v05_M2Camera_UECombined",
        "replace_existing": bool(existing),
        "automated": True,
        "save": True,
    })
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported_paths = list(task.get_editor_property("imported_object_paths"))
    mesh = unreal.load_asset(ENGINE_MESH_PATH)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("OBJ import did not produce the expected StaticMesh: " + ENGINE_MESH_PATH)
    if not unreal.EditorAssetLibrary.save_asset(mesh.get_path_name(), only_if_is_dirty=False):
        raise RuntimeError("Could not save the v05 StaticMesh")
    materials_out = []
    for slot in mesh.get_editor_property("static_materials"):
        interface = slot.get_editor_property("material_interface")
        materials_out.append(interface.get_path_name() if interface else None)
    bounds = mesh.get_bounds()
    origin, extent = bounds.origin, bounds.box_extent
    report.update({
        "status": "imported",
        "result": import_status,
        "mesh_asset": mesh.get_path_name(),
        "source_obj_sha256": sha256(SOURCE),
        "source_mtl_sha256": sha256(SOURCE_MTL),
        "imported_mesh_bounds_cm": {
            "x": [origin.x - extent.x, origin.x + extent.x],
            "y": [origin.y - extent.y, origin.y + extent.y],
            "z": [origin.z - extent.z, origin.z + extent.z],
        },
        "vertex_count": vertices,
        "face_count": faces,
        "source_material_count": len(materials),
        "lod_count": mesh.get_num_lods(),
        "nanite_triangle_count": mesh.get_num_nanite_triangles(),
        "section_count": mesh.get_num_sections(0),
        "material_slot_count": len(materials_out),
        "material_slots": materials_out,
        "imported_object_paths": imported_paths,
        "review_only": True,
    })
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()

REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
if report["status"] != "imported":
    unreal.log_error(report.get("error", "P6 A1 v05 import failed"))
    raise RuntimeError("P6 A1 v05 import failed; inspect " + str(REPORT))
unreal.log("NAIWA_P6_A1_V05_M2_CAMERA_IMPORTED " + json.dumps(report, ensure_ascii=False))
