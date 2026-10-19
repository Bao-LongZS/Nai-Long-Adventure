"""Import the user supplied P1 feedback audio and visual assets into UE."""

import hashlib
import json
import traceback
from datetime import datetime, timezone
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
SOURCE_ROOT = ROOT / "UE5/SourceAssets/P1_Feedback"
MANIFEST_PATH = SOURCE_ROOT / "feedback_source_manifest.json"
DEST_ROOT = "/Game/Naiwa/P1/Feedback"
OUTPUT = PROJECT / "Saved/NaiwaP1/feedback_asset_import.json"
OUTPUT.parent.mkdir(parents=True, exist_ok=True)

AUDIO_NAMES = {
    "Background_Music_1.wav": "SFX_P1_Feedback_BackgroundMusic1",
    "Background_Music_2.wav": "SFX_P1_Feedback_BackgroundMusic2",
    "Collection.wav": "SFX_P1_Feedback_Collection",
    "Collection_1.wav": "SFX_P1_Feedback_Collection1",
    "Collection_2.wav": "SFX_P1_Feedback_Collection2",
    "Die.wav": "SFX_P1_Feedback_Die",
    "Die_1.wav": "SFX_P1_Feedback_Die1",
    "Die_3.wav": "SFX_P1_Feedback_Die3",
    "Jump.wav": "SFX_P1_Feedback_Jump",
    "哎哟我去.wav": "SFX_P1_Feedback_CheckpointEarly",
    "哟嚯嚯嚯.wav": "SFX_P1_Feedback_CheckpointLate",
}
TEXTURE_FILES = {
    "Big_Laugh_reference.png": "T_P1_Feedback_BigLaughReference",
}

REPORT = {
    "schema": "NaiwaP1FeedbackImportV1",
    "started_utc": datetime.now(timezone.utc).isoformat(),
    "engine": unreal.SystemLibrary.get_engine_version(),
    "assets": [],
    "status": "started",
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def import_file(source, folder, name, class_name, replace_existing=False):
    package = folder + "/" + name
    existing = unreal.load_asset(package) if unreal.EditorAssetLibrary.does_asset_exist(package) else None
    if existing:
        assert existing.get_class().get_name() == class_name, "Existing target has wrong class: " + package
    if existing and not replace_existing:
        asset = existing
        action = "verified_existing_without_write"
    else:
        task = unreal.AssetImportTask()
        for key, value in (
            ("filename", str(source)),
            ("destination_path", folder),
            ("destination_name", name),
            ("automated", True),
            ("replace_existing", bool(existing) or replace_existing),
            ("save", True),
        ):
            task.set_editor_property(key, value)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        asset = unreal.load_asset(package)
        action = "reimported_existing_source" if existing else "imported_new"
    assert asset is not None and asset.get_class().get_name() == class_name, "Import failed: " + package
    disk_file = PROJECT / "Content" / package.removeprefix("/Game/")
    disk_file = disk_file.with_suffix(".uasset")
    assert disk_file.is_file(), "Imported package was not saved: " + str(disk_file)
    REPORT["assets"].append({
        "source": str(source.relative_to(ROOT)).replace("\\", "/"),
        "source_sha256": sha256(source),
        "asset_path": package + "." + name,
        "asset_class": class_name,
        "uasset_sha256": sha256(disk_file),
        "action": action,
    })
    return asset


def prepare_ui_texture(texture):
    compression_enum = unreal.TextureCompressionSettings
    compression = getattr(compression_enum, "TC_USERINTERFACE2D", None)
    if compression is None:
        compression = getattr(compression_enum, "TC_EDITOR_ICON")
    texture.set_editor_property("compression_settings", compression)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture), "Could not save UI texture settings"
    package = texture.get_path_name().split(".", 1)[0]
    disk_file = PROJECT / "Content" / package.removeprefix("/Game/")
    disk_file = disk_file.with_suffix(".uasset")
    asset_path = texture.get_path_name()
    for entry in REPORT["assets"]:
        if entry.get("asset_path") == asset_path:
            entry["uasset_sha256"] = sha256(disk_file)
            entry["ui_texture_settings"] = "UserInterface2D, no mipmaps, UI group, never stream"
            break


def build_pickup_material():
    package = DEST_ROOT + "/Materials/M_P1_FeedbackPickup"
    existing = unreal.load_asset(package)
    if existing:
        assert isinstance(existing, unreal.Material), "Pickup material has the wrong class"
        material = existing
        action = "updated_to_untextured_color_material"
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_P1_FeedbackPickup", DEST_ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
        action = "created_untextured_color_material"
    assert material
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    expressions = unreal.MaterialEditingLibrary.get_material_expressions(material)
    color = next((expression for expression in expressions
                  if expression.get_class().get_name() == "MaterialExpressionVectorParameter"
                  and expression.get_editor_property("parameter_name") == "PickupColor"), None)
    if not color:
        color = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionVectorParameter, -400, -80)
        color.set_editor_property("parameter_name", "PickupColor")
        color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.72, 0.08, 1.0))
    opacity = next((expression for expression in expressions
                    if expression.get_class().get_name() == "MaterialExpressionScalarParameter"
                    and expression.get_editor_property("parameter_name") == "SpriteOpacity"), None)
    if not opacity:
        opacity = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionScalarParameter, -400, 160)
        opacity.set_editor_property("parameter_name", "SpriteOpacity")
        opacity.set_editor_property("default_value", 1.0)
    assert unreal.MaterialEditingLibrary.connect_material_property(
        color, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert unreal.MaterialEditingLibrary.connect_material_property(
        opacity, "", unreal.MaterialProperty.MP_OPACITY)
    assert not unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)
    material_file = PROJECT / "Content/Naiwa/P1/Feedback/Materials/M_P1_FeedbackPickup.uasset"
    REPORT["assets"].append({
        "source": "generated_untextured_color_and_fade_material",
        "asset_path": package + ".M_P1_FeedbackPickup",
        "asset_class": "Material",
        "uasset_sha256": sha256(material_file),
        "action": action,
    })
    return material


try:
    project_dir = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    assert project_dir == PROJECT.resolve(), "Refusing to import into a different project"
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    audio_by_name = {item["name"]: item for item in manifest["audio"]}
    assert set(audio_by_name) == set(AUDIO_NAMES)

    for source_name, asset_name in AUDIO_NAMES.items():
        source = SOURCE_ROOT / "Audio" / source_name
        assert source.is_file() and sha256(source) == audio_by_name[source_name]["normalized_copy_sha256"]
        import_file(source, DEST_ROOT + "/Audio", asset_name, "SoundWave")

    texture_assets = {}
    for source_name, asset_name in TEXTURE_FILES.items():
        texture_assets[source_name] = import_file(
            SOURCE_ROOT / "Images" / source_name, DEST_ROOT + "/Textures", asset_name, "Texture2D")
        prepare_ui_texture(texture_assets[source_name])

    frame_folder = DEST_ROOT + "/DeathLaughFrames"
    frame_report = []
    for frame in manifest["death_laugh"]["frames"]:
        source = ROOT / frame["path"]
        assert source.is_file() and sha256(source) == frame["sha256"]
        name = "T_P1_DeathLaugh_%02d" % frame["index"]
        # These stable package paths were first populated by a superseded GIF.
        # Always replace their source bytes so no old frames survive in the loop.
        texture = import_file(source, frame_folder, name, "Texture2D", replace_existing=True)
        prepare_ui_texture(texture)
        frame_report.append({"index": frame["index"], "duration_ms": frame["duration_ms"],
                             "asset_path": frame_folder + "/" + name + "." + name})

    build_pickup_material()
    REPORT["death_laugh_frames"] = frame_report
    REPORT["death_laugh_source_url"] = manifest["death_laugh"]["source_url"]
    REPORT["death_laugh_source_start_seconds"] = manifest["death_laugh"]["source_start_seconds"]
    REPORT["death_laugh_frame_rate"] = manifest["death_laugh"]["source_fps"]
    REPORT["death_laugh_loop_duration_ms"] = manifest["death_laugh"]["duration_ms"]
    REPORT["status"] = "imported"
    REPORT["finished_utc"] = datetime.now(timezone.utc).isoformat()
    OUTPUT.write_text(json.dumps(REPORT, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log("NAIWA_P1_FEEDBACK_IMPORT_OK " + str(OUTPUT))
except Exception:
    REPORT["status"] = "failed"
    REPORT["error"] = traceback.format_exc()
    REPORT["finished_utc"] = datetime.now(timezone.utc).isoformat()
    OUTPUT.write_text(json.dumps(REPORT, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log_error(REPORT["error"])
    raise
