"""Read-only verification of imported P2 feedback assets and loop settings."""

from pathlib import Path

import unreal


PROJECT = str(Path(__file__).resolve().parents[2] / "UE5" / "NaiwaP0")
TEXTURES = (
    "/Game/Naiwa/P2/UI/T_P2_Failure_OneMinute.T_P2_Failure_OneMinute",
    "/Game/Naiwa/P2/UI/T_P2_Failure_Resets.T_P2_Failure_Resets",
    "/Game/Naiwa/P2/UI/T_P2_Failure_Exhausted.T_P2_Failure_Exhausted",
)
SOUNDS = (
    "/Game/Naiwa/P2/Audio/SFX_P2_Collection.SFX_P2_Collection",
    "/Game/Naiwa/P2/Audio/SFX_P2_FailureTrumpet.SFX_P2_FailureTrumpet",
)
MUSIC = (
    "/Game/Naiwa/P2/Audio/SW_P2_21_TitleTheme.SW_P2_21_TitleTheme",
    "/Game/Naiwa/P2/Audio/SW_P2_22_RainLove.SW_P2_22_RainLove",
    "/Game/Naiwa/P2/Audio/SW_P2_23_BoonieBears.SW_P2_23_BoonieBears",
)


project_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()).replace("\\", "/").rstrip("/")
if project_dir.lower() != PROJECT.lower():
    raise RuntimeError("Refusing to verify an unexpected project: " + project_dir)

for path in TEXTURES:
    asset = unreal.load_asset(path)
    if asset is None or asset.get_class().get_name() != "Texture2D":
        raise RuntimeError("Missing or invalid feedback texture: " + path)

for path in SOUNDS:
    asset = unreal.load_asset(path)
    if asset is None or asset.get_class().get_name() != "SoundWave":
        raise RuntimeError("Missing or invalid feedback sound: " + path)
    if asset.get_editor_property("looping"):
        raise RuntimeError("Feedback sound must not loop: " + path)

for path in MUSIC:
    asset = unreal.load_asset(path)
    if asset is None or asset.get_class().get_name() != "SoundWave":
        raise RuntimeError("Missing or invalid level music: " + path)
    if not asset.get_editor_property("looping"):
        raise RuntimeError("Level music is not looping: " + path)

unreal.log("[P2_FAILURE_FEEDBACK_VERIFY] PASS textures=3 cues=2 looping_music=3 read_only=1")
