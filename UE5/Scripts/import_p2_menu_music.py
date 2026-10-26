"""Import and enable looping for the P2 main-menu music."""

import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
SOURCE = ROOT / "UE5/SourceAssets/P2_Audio/奇奇妙妙 - 金风玉露.mp3"
DEST_ROOT = "/Game/Naiwa/UI/Audio"
ASSET_NAME = "SW_Menu_JinFengYuLu"
REPORT_PATH = ROOT / "UE5/Records/p2_menu_music_import.json"


project_dir = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
if project_dir != PROJECT.resolve():
    raise RuntimeError("Refusing to import into unexpected project: " + str(project_dir))
if not SOURCE.is_file():
    raise RuntimeError("Missing source audio: " + str(SOURCE))

unreal.EditorAssetLibrary.make_directory(DEST_ROOT)
package = DEST_ROOT + "/" + ASSET_NAME
existing = unreal.load_asset(package) if unreal.EditorAssetLibrary.does_asset_exist(package) else None

task = unreal.AssetImportTask()
for key, value in (
    ("filename", str(SOURCE)),
    ("destination_path", DEST_ROOT),
    ("destination_name", ASSET_NAME),
    ("automated", True),
    ("replace_existing", existing is not None),
    ("save", True),
):
    task.set_editor_property(key, value)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

sound = unreal.load_asset(package)
if sound is None or sound.get_class().get_name() != "SoundWave":
    raise RuntimeError("SoundWave import failed: " + package)
for property_name in ("looping", "b_looping"):
    try:
        sound.set_editor_property(property_name, True)
        break
    except Exception:
        continue
else:
    raise RuntimeError("SoundWave looping property is unavailable: " + sound.get_name())
if not unreal.EditorAssetLibrary.save_loaded_asset(sound):
    raise RuntimeError("Could not save imported SoundWave: " + package)

asset_file = PROJECT / "Content" / package.removeprefix("/Game/")
asset_file = asset_file.with_suffix(".uasset")
if not asset_file.is_file():
    raise RuntimeError("Imported SoundWave was not saved: " + str(asset_file))

report = {
    "schema": "NaiwaP2MenuMusicImportV1",
    "started_utc": datetime.now(timezone.utc).isoformat(),
    "engine": unreal.SystemLibrary.get_engine_version(),
    "source": str(SOURCE.relative_to(ROOT)).replace("\\", "/"),
    "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
    "asset_path": package + "." + ASSET_NAME,
    "asset_sha256": hashlib.sha256(asset_file.read_bytes()).hexdigest(),
    "looping": True,
}
REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
unreal.log_warning("[P2_MENU_MUSIC] imported %s looping=True" % ASSET_NAME)
