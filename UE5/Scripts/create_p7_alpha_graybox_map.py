"""Create the isolated P7 Alpha graybox map and bind its P7-only GameMode."""
from __future__ import annotations

import json
import os
from pathlib import Path
import traceback

import unreal


ROOT = Path(__file__).resolve().parents[2]
MAP_ASSET = "/Game/Naiwa/P7/Maps/L_P7_Alpha01_Graybox"
MAP_FILE = ROOT / "UE5/NaiwaP0/Content/Naiwa/P7/Maps/L_P7_Alpha01_Graybox.umap"
GAME_MODE_PATH = "/Script/NaiwaP7Runtime.NaiwaP7AlphaGrayboxGameMode"
REPORT = Path(
    os.environ.get(
        "NAIWA_P7_MAP_REPORT",
        str(ROOT / "UE5/Records/p7_td/evidence/P7_AlphaGrayboxMapCreation.json"),
    )
)


report = {"status": "failed", "map": MAP_ASSET, "game_mode": GAME_MODE_PATH}
try:
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_ASSET):
        raise RuntimeError("Refusing to overwrite existing map: " + MAP_ASSET)

    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P7/Maps")
    if not unreal.EditorLevelLibrary.new_level(MAP_ASSET):
        raise RuntimeError("EditorLevelLibrary.new_level returned false")

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        raise RuntimeError("No editor world exists after new_level")
    world_settings = world.get_world_settings()
    if not world_settings:
        raise RuntimeError("New P7 level has no WorldSettings")

    game_mode_class = unreal.load_class(None, GAME_MODE_PATH)
    if not game_mode_class:
        raise RuntimeError("P7 Alpha GameMode class could not be loaded: " + GAME_MODE_PATH)
    world_settings.set_editor_property("default_game_mode", game_mode_class)

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("EditorLevelLibrary.save_current_level returned false")
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_ASSET):
        raise RuntimeError("P7 map package is not registered after save")
    if not MAP_FILE.is_file() or MAP_FILE.stat().st_size <= 0:
        raise RuntimeError("Saved map package is missing: " + str(MAP_FILE))

    reloaded_world_settings = unreal.EditorLevelLibrary.get_editor_world().get_world_settings()
    actual_game_mode = reloaded_world_settings.get_editor_property("default_game_mode")
    if not actual_game_mode or actual_game_mode.get_path_name() != GAME_MODE_PATH:
        raise RuntimeError(
            "Map GameMode did not round-trip: "
            + (actual_game_mode.get_path_name() if actual_game_mode else "None")
        )

    report = {
        "status": "passed",
        "map": MAP_ASSET,
        "map_file": str(MAP_FILE),
        "map_package_bytes": MAP_FILE.stat().st_size,
        "world_settings_property": "default_game_mode",
        "game_mode": actual_game_mode.get_path_name(),
        "data_fixture": "P7_Alpha01_v01",
        "scope": "isolated map creation and GameMode binding; runtime launch is validated separately",
    }
except Exception:
    report = {
        "status": "failed",
        "map": MAP_ASSET,
        "game_mode": GAME_MODE_PATH,
        "error": traceback.format_exc(),
    }

REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
if report["status"] != "passed":
    unreal.log_error(report.get("error", "P7 graybox map creation failed"))
    raise RuntimeError("P7 graybox map creation failed; see " + str(REPORT))
unreal.log("Created isolated P7 Alpha graybox map and assigned its P7-only GameMode.")
