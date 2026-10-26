"""Import supplied P2 audio clips and enable looping on the three level tracks."""

import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
SOURCE_ROOT = ROOT / "UE5/SourceAssets/P2_Audio"
DEST_ROOT = "/Game/Naiwa/P2/Audio"
REPORT_PATH = ROOT / "UE5/Records/p2_audio_import.json"

AUDIO = [
    ("Ari Pulkkinen - Title Theme.mp3", "SW_P2_21_TitleTheme", True),
    ("阿成哥,霓虹兔 - 雨爱.mp3", "SW_P2_22_RainLove", True),
    ("雷米克斯 - 熊出没.mp3", "SW_P2_23_BoonieBears", True),
    ("嗯？.mp3", "SFX_P2_Miss", False),
    ("冰冰冰.mp3", "SFX_P2_GoalHit", False),
    ("鸡.mp3", "SFX_P2_EggCollision", False),
]


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def set_looping(sound, enabled):
    for property_name in ("looping", "b_looping"):
        try:
            sound.set_editor_property(property_name, enabled)
            return
        except Exception:
            pass
    raise RuntimeError("SoundWave looping property is unavailable: " + sound.get_name())


project_dir = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
if project_dir != PROJECT.resolve():
    raise RuntimeError("Refusing to import into unexpected project: " + str(project_dir))

unreal.EditorAssetLibrary.make_directory(DEST_ROOT)
report = {
    "schema": "NaiwaP2AudioImportV1",
    "started_utc": datetime.now(timezone.utc).isoformat(),
    "engine": unreal.SystemLibrary.get_engine_version(),
    "assets": [],
}

previous_hashes = {}
if REPORT_PATH.is_file():
    try:
        previous_report = json.loads(REPORT_PATH.read_text(encoding="utf-8"))
        previous_hashes = {
            Path(item["source"]).name: item["source_sha256"]
            for item in previous_report.get("assets", [])
        }
    except (OSError, ValueError, KeyError, TypeError):
        previous_hashes = {}

for source_name, asset_name, looping in AUDIO:
    source = SOURCE_ROOT / source_name
    if not source.is_file():
        raise RuntimeError("Missing source audio: " + str(source))

    package = DEST_ROOT + "/" + asset_name
    existing = unreal.load_asset(package) if unreal.EditorAssetLibrary.does_asset_exist(package) else None
    source_hash = sha256(source)
    if existing is not None and previous_hashes.get(source_name) == source_hash:
        sound = existing
        action = "verified_existing_unchanged_source"
    else:
        task = unreal.AssetImportTask()
        for key, value in (
            ("filename", str(source)),
            ("destination_path", DEST_ROOT),
            ("destination_name", asset_name),
            ("automated", True),
            ("replace_existing", existing is not None),
            ("save", True),
        ):
            task.set_editor_property(key, value)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        sound = unreal.load_asset(package)
        action = "imported_or_reimported"

    if sound is None or sound.get_class().get_name() != "SoundWave":
        raise RuntimeError("SoundWave import failed: " + package)
    set_looping(sound, looping)
    if not unreal.EditorAssetLibrary.save_loaded_asset(sound):
        raise RuntimeError("Could not save imported SoundWave: " + package)

    asset_file = PROJECT / "Content" / package.removeprefix("/Game/")
    asset_file = asset_file.with_suffix(".uasset")
    if not asset_file.is_file():
        raise RuntimeError("Imported SoundWave was not saved: " + str(asset_file))
    report["assets"].append({
        "source": str(source.relative_to(ROOT)).replace("\\", "/"),
        "source_sha256": source_hash,
        "asset_path": package + "." + asset_name,
        "asset_sha256": sha256(asset_file),
        "looping": looping,
        "action": action,
    })
    unreal.log_warning("[P2_AUDIO] %s action=%s looping=%s" % (asset_name, action, looping))

REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
unreal.log_warning("[P2_AUDIO] imported %d clips; report=%s" % (len(report["assets"]), REPORT_PATH))
