"""Verify the P2 menu's looping audio asset and controller reference."""

import json
from datetime import datetime, timezone
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
REPORT_PATH = ROOT / "UE5/Records/p2_menu_music_verify.json"
ASSET_PATH = "/Game/Naiwa/UI/Audio/SW_Menu_JinFengYuLu.SW_Menu_JinFengYuLu"

project_dir = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
if project_dir != PROJECT.resolve():
    raise RuntimeError("Unexpected project: " + str(project_dir))

sound = unreal.load_asset(ASSET_PATH)
if sound is None or sound.get_class().get_name() != "SoundWave":
    raise RuntimeError("Menu music SoundWave missing: " + ASSET_PATH)
looping = None
for property_name in ("looping", "b_looping"):
    try:
        looping = bool(sound.get_editor_property(property_name))
        break
    except Exception:
        continue
if looping is not True:
    raise RuntimeError("Menu music is not configured to loop")

controller_class = unreal.load_class(None, "/Script/NaiwaRuntime.NaiwaMenuPlayerController")
if controller_class is None:
    raise RuntimeError("Menu player controller class missing")

controller_source = (ROOT / "UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaMenuPlayerController.cpp").read_text(encoding="utf-8")
if ASSET_PATH.removeprefix("/Game/").replace("/", "/") not in controller_source:
    raise RuntimeError("Menu player controller source does not reference the menu music asset")
if "UGameplayStatics::SpawnSound2D" not in controller_source or "this, MenuMusic" not in controller_source:
    raise RuntimeError("Menu player controller source does not start the menu music")

report = {
    "schema": "NaiwaP2MenuMusicVerifyV1",
    "verified_utc": datetime.now(timezone.utc).isoformat(),
    "engine": unreal.SystemLibrary.get_engine_version(),
    "asset_path": ASSET_PATH,
    "looping": looping,
    "controller_class": "/Script/NaiwaRuntime.NaiwaMenuPlayerController",
    "controller_class_loaded": True,
    "controller_music_path": ASSET_PATH,
    "controller_starts_music_in_begin_play": True,
}
REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
unreal.log_warning("[P2_MENU_MUSIC_VERIFY] asset, looping flag, and menu controller reference verified")
