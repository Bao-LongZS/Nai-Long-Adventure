"""Import the P2 bow-draw clip onto the pinned Naiwa skeleton."""
import hashlib
import json
import pathlib
import traceback
import unreal

ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCE = ROOT / "UE5/Animation/P2_Archer/Naiwa_P2_SlingshotDraw.fbx"
REPORT_PATH = ROOT / "UE5/Records/import_p2_archer_animation.json"
FOLDER = "/Game/Naiwa/P2/Art/Character"
ASSET = FOLDER + "/AS_Naiwa_P2_SlingshotDraw"
report = {
    "phase": "p2_archer_animation_import",
    "engine": unreal.SystemLibrary.get_engine_version(),
    "source": str(SOURCE),
    "warnings": [],
}


def save_report():
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


try:
    report["source_sha256"] = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    original_mesh = unreal.load_asset("/Game/Naiwa/SK_Naiwa")
    assert original_mesh, "Pinned SK_Naiwa is not available"
    skeleton = original_mesh.get_editor_property("skeleton")
    assert skeleton, "Pinned Naiwa skeleton is not available"
    options = unreal.FbxImportUI()
    options.set_editor_properties({
        "automated_import_should_detect_type": False,
        "mesh_type_to_import": unreal.FBXImportType.FBXIT_ANIMATION,
        "import_as_skeletal": False,
        "import_mesh": False,
        "import_animations": True,
        "create_physics_asset": False,
        "import_materials": False,
        "import_textures": False,
        "skeleton": skeleton,
    })
    animation_settings = options.get_editor_property("anim_sequence_import_data")
    animation_settings.set_editor_properties({
        "use_default_sample_rate": False,
        "animation_length": unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME,
        "import_custom_attribute": True,
    })
    task = unreal.AssetImportTask()
    task.set_editor_properties({
        "filename": str(SOURCE),
        "destination_path": FOLDER,
        "destination_name": "AS_Naiwa_P2_SlingshotDraw",
        "automated": True,
        "replace_existing": True,
        "save": True,
        "options": options,
    })
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    report["imported_paths"] = list(task.get_editor_property("imported_object_paths"))
    sequence = unreal.load_asset(ASSET)
    assert sequence and isinstance(sequence, unreal.AnimSequence), "P2 slingshot animation was not imported"
    imported_skeleton = sequence.get_editor_property("skeleton")
    assert imported_skeleton == skeleton, "Animation imported onto a different skeleton"
    report.update(
        status="imported",
        asset=sequence.get_path_name(),
        skeleton=imported_skeleton.get_path_name(),
        duration_seconds=sequence.get_play_length(),
        sampled_keys=sequence.get_number_of_sampled_keys() if hasattr(sequence, "get_number_of_sampled_keys") else None,
        source_unchanged=hashlib.sha256(SOURCE.read_bytes()).hexdigest() == report["source_sha256"],
    )
    assert report["duration_seconds"] > 0.9, "Imported animation is unexpectedly short"
    assert unreal.EditorAssetLibrary.save_directory(FOLDER, only_if_is_dirty=False, recursive=True)
    save_report()
    unreal.log("NAIWA_P2_ARCHER_ANIMATION_IMPORT_OK")
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    save_report()
    unreal.log_error(report["error"])
    raise
