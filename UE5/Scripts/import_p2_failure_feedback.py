"""Import and configure the three transparent P2 failure feedback images."""

import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
SOURCE_ROOT = ROOT / "UE5/SourceAssets/P2_FailureFeedback/Images"
DEST_ROOT = "/Game/Naiwa/P2/UI"
SOURCE_AUDIO_ROOT = ROOT / "UE5/SourceAssets/P2_Audio"
DEST_AUDIO_ROOT = "/Game/Naiwa/P2/Audio"
REPORT_PATH = ROOT / "UE5/Records/p2_failure_feedback_import.json"
TEXTURES = {
    "T_P2_Failure_OneMinute.png": "T_P2_Failure_OneMinute",
    "T_P2_Failure_Resets.png": "T_P2_Failure_Resets",
    "T_P2_Failure_Exhausted.png": "T_P2_Failure_Exhausted",
}
AUDIO = {
    "SFX_P2_Collection_Source.wav": "SFX_P2_Collection",
    "SFX_P2_FailureTrumpet_Source.mp3": "SFX_P2_FailureTrumpet",
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def import_texture(source, asset_name):
    package = DEST_ROOT + "/" + asset_name
    existing = unreal.load_asset(package) if unreal.EditorAssetLibrary.does_asset_exist(package) else None
    if existing is not None:
        if existing.get_class().get_name() != "Texture2D":
            raise RuntimeError("Existing asset has the wrong class: " + package)
        texture = existing
        action = "verified_existing_without_overwrite"
    else:
        task = unreal.AssetImportTask()
        for key, value in (
            ("filename", str(source)),
            ("destination_path", DEST_ROOT),
            ("destination_name", asset_name),
            ("automated", True),
            ("replace_existing", False),
            ("save", True),
        ):
            task.set_editor_property(key, value)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(package)
        action = "imported_new"

    if texture is None or texture.get_class().get_name() != "Texture2D":
        raise RuntimeError("Texture import failed: " + package)

    compression = getattr(unreal.TextureCompressionSettings, "TC_USERINTERFACE2D", None)
    if compression is None:
        compression = unreal.TextureCompressionSettings.TC_EDITOR_ICON
    texture.set_editor_property("compression_settings", compression)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError("Could not save texture settings: " + package)

    asset_file = PROJECT / "Content" / package.removeprefix("/Game/")
    asset_file = asset_file.with_suffix(".uasset")
    if not asset_file.is_file():
        raise RuntimeError("Imported texture was not saved: " + str(asset_file))
    return {
        "source": str(source.relative_to(ROOT)).replace("\\", "/"),
        "source_sha256": sha256(source),
        "asset_path": package + "." + asset_name,
        "asset_sha256": sha256(asset_file),
        "asset_class": "Texture2D",
        "action": action,
        "alpha_preserved": True,
        "texture_settings": "UserInterface2D, no mipmaps, UI group, never stream",
    }


def import_sound(source, asset_name):
    package = DEST_AUDIO_ROOT + "/" + asset_name
    existing = unreal.load_asset(package) if unreal.EditorAssetLibrary.does_asset_exist(package) else None
    if existing is not None:
        if existing.get_class().get_name() != "SoundWave":
            raise RuntimeError("Existing asset has the wrong class: " + package)
        sound = existing
        action = "verified_existing_without_overwrite"
    else:
        task = unreal.AssetImportTask()
        for key, value in (
            ("filename", str(source)),
            ("destination_path", DEST_AUDIO_ROOT),
            ("destination_name", asset_name),
            ("automated", True),
            ("replace_existing", False),
            ("save", True),
        ):
            task.set_editor_property(key, value)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        sound = unreal.load_asset(package)
        action = "imported_new"

    if sound is None or sound.get_class().get_name() != "SoundWave":
        raise RuntimeError("Sound import failed: " + package)
    for property_name in ("looping", "b_looping"):
        try:
            sound.set_editor_property(property_name, False)
            break
        except Exception:
            continue
    if not unreal.EditorAssetLibrary.save_loaded_asset(sound):
        raise RuntimeError("Could not save SoundWave: " + package)

    asset_file = PROJECT / "Content" / package.removeprefix("/Game/")
    asset_file = asset_file.with_suffix(".uasset")
    if not asset_file.is_file():
        raise RuntimeError("Imported SoundWave was not saved: " + str(asset_file))
    return {
        "source": str(source.relative_to(ROOT)).replace("\\", "/"),
        "source_sha256": sha256(source),
        "asset_path": package + "." + asset_name,
        "asset_sha256": sha256(asset_file),
        "asset_class": "SoundWave",
        "action": action,
        "looping": False,
    }


project_dir = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
if project_dir != PROJECT.resolve():
    raise RuntimeError("Refusing to import into unexpected project: " + str(project_dir))

unreal.EditorAssetLibrary.make_directory(DEST_ROOT)
report = {
    "schema": "NaiwaP2FailureFeedbackImportV1",
    "started_utc": datetime.now(timezone.utc).isoformat(),
    "engine": unreal.SystemLibrary.get_engine_version(),
    "assets": [],
}

for source_name, asset_name in TEXTURES.items():
    source = SOURCE_ROOT / source_name
    if not source.is_file():
        raise RuntimeError("Missing feedback image: " + str(source))
    report["assets"].append(import_texture(source, asset_name))

for source_name, asset_name in AUDIO.items():
    source = SOURCE_AUDIO_ROOT / source_name
    if not source.is_file():
        raise RuntimeError("Missing feedback sound: " + str(source))
    report["assets"].append(import_sound(source, asset_name))

REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
unreal.log_warning("[P2_FAILURE_FEEDBACK] imported %d assets; report=%s" %
                   (len(report["assets"]), REPORT_PATH))
