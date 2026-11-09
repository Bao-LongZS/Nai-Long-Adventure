"""Read-only export of the P1 gameplay character and run clip from UE to GLB.

The Unreal project is opened only for asset loading/export. This script never
duplicates, edits, or saves a package in the Content directory.
"""
import json
import pathlib
import traceback

import unreal


ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "WebNative" / "public" / "assets"
MESH_PATH = "/Game/Naiwa/Gameplay/SK_Naiwa_Elastic"
RUN_PATH = "/Game/Naiwa/Gameplay/AS_Naiwa_Run"


def set_option(options, name, value):
    options.set_editor_property(name, value)


def export_asset(asset_path, output_path, *, animation=False):
    asset = unreal.load_asset(asset_path)
    if not asset:
        raise RuntimeError(f"Could not load UE asset: {asset_path}")

    options = unreal.GLTFExportOptions()
    set_option(options, "export_uniform_scale", 0.01)
    set_option(options, "export_vertex_skin_weights", True)
    set_option(options, "export_morph_targets", True)
    set_option(options, "export_animation_sequences", animation)
    set_option(options, "export_preview_mesh", False)
    set_option(options, "export_vertex_colors", True)
    set_option(options, "use_mesh_quantization", True)
    set_option(options, "texture_image_format", unreal.GLTFTextureImageFormat.PNG)

    task = unreal.AssetExportTask()
    task.set_editor_property("object", asset)
    task.set_editor_property("filename", str(output_path))
    task.set_editor_property("automated", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("options", options)
    ok = unreal.Exporter.run_asset_export_task(task)
    if not ok or not output_path.is_file() or output_path.stat().st_size == 0:
        errors = list(task.get_editor_property("errors")) if task.has_editor_property("errors") else []
        raise RuntimeError(f"GLB export failed for {asset_path}: {errors}")
    return {
        "asset": asset_path,
        "output": str(output_path.relative_to(ROOT)).replace("\\", "/"),
        "bytes": output_path.stat().st_size,
        "sha256": __import__("hashlib").sha256(output_path.read_bytes()).hexdigest(),
    }


try:
    OUT.mkdir(parents=True, exist_ok=True)
    mesh = unreal.load_asset(MESH_PATH)
    if not mesh or not isinstance(mesh, unreal.SkeletalMesh):
        raise RuntimeError(f"Expected a SkeletalMesh at {MESH_PATH}")
    morph_names = sorted(target.get_name() for target in mesh.get_editor_property("morph_targets"))
    required_morphs = {"Belly_Compress", "Belly_Rebound", "Belly_SoftLag"}
    if not required_morphs.issubset(morph_names):
        raise RuntimeError(f"Gameplay mesh is missing expected morph targets: {morph_names}")

    mesh_result = export_asset(MESH_PATH, OUT / "naiwa-character-e2.glb")
    run_result = export_asset(RUN_PATH, OUT / "naiwa-run.glb", animation=True)
    report = {
        "status": "PASS_UE_GLTF_EXPORT",
        "engine": unreal.SystemLibrary.get_engine_version(),
        "mesh": mesh_result,
        "animation": run_result,
        "morphTargets": morph_names,
        "meshHasVertexColors": mesh.has_vertex_colors(),
        "limitations": [
            "UE gameplay mesh and run sequence were exported as separate GLBs; runtime track binding must be checked in the browser.",
            "Custom Unreal material graphs may be baked or simplified by the glTF exporter.",
        ],
    }
    (ROOT / "WebNative" / "docs" / "gameplay-character-export.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    unreal.log("WEBNATIVE_CHARACTER_EXPORT_OK " + json.dumps(report, ensure_ascii=False))
except Exception:
    error = traceback.format_exc()
    unreal.log_error("WEBNATIVE_CHARACTER_EXPORT_FAILED " + error)
    (ROOT / "WebNative" / "docs" / "gameplay-character-export.json").write_text(
        json.dumps({"status": "FAIL_UE_GLTF_EXPORT", "error": error}, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    raise
