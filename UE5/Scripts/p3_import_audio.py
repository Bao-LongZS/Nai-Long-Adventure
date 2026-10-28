"""Import the approved P3 audio pack and configure its reusable attenuation assets."""
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
SOURCE = ROOT / "UE5/SourceAssets/P3/Audio"
DEST = "/Game/Naiwa/P3/Audio"
ATT_DEST = DEST + "/Attenuation"
REPORT = ROOT / "UE5/Records/p3_audio/audio_import_report.json"

CLIPS = [
    ("00_BGM_TheEntertainer.mp3", "BGM_P3_Level", "BINK_AUDIO", "LOAD_ON_DEMAND", None),
    ("01_放弹.wav", "SFX_P3_PlaceBomb", "PCM", "FORCE_INLINE", None),
    ("02_标准十字爆.wav", "SFX_P3_Explode_Normal", "PCM", "FORCE_INLINE", None),
    ("03_豆奶大十字_低频版.wav", "SFX_P3_Explode_SoyLow", "ADPCM", "FORCE_INLINE", None),
    ("04_豆奶大十字_爆炸版.wav", "SFX_P3_Explode_Soy", "ADPCM", "LOAD_ON_DEMAND", None),
    ("05_奶茶径向爆.wav", "SFX_P3_Explode_TeaRadial", "PCM", "FORCE_INLINE", None),
    ("06_奶鼠分裂.wav", "SFX_P3_Explode_MouseSplit", "PCM", "FORCE_INLINE", None),
    ("07_小弹爆.wav", "SFX_P3_Explode_SmallPop", "PCM", "FORCE_INLINE", None),
    ("08_传送门.wav", "SFX_P3_Portal", "PCM", "FORCE_INLINE", "World"),
    ("09_传送带.wav", "AMB_P3_Conveyor", "BINK_AUDIO", "LOAD_ON_DEMAND", None),
    ("10_断裂地板.wav", "SFX_P3_FloorCrack", "PCM", "FORCE_INLINE", "World"),
    ("11_闸门开.wav", "SFX_P3_GateOpen", "PCM", "FORCE_INLINE", "World"),
    ("12_踩板触发.wav", "SFX_P3_PressurePlate", "PCM", "FORCE_INLINE", "World"),
    ("13_尖刺弹出.wav", "SFX_P3_SpikeOut", "PCM", "FORCE_INLINE", "World"),
    ("14_奶杀冲刺.wav", "SFX_P3_EnemyCharge", "ADPCM", "LOAD_ON_DEMAND", "World"),
    ("15_奶豪砸砖.wav", "SFX_P3_EnemyBrick", "PCM", "FORCE_INLINE", "World"),
    ("16_奶雨_长雨.wav", "AMB_P3_Rain", "BINK_AUDIO", "LOAD_ON_DEMAND", "Area"),
    ("17_受伤.wav", "SFX_P3_PlayerHurt", "PCM", "FORCE_INLINE", None),
    ("18_死亡.wav", "SFX_P3_PlayerDie", "PCM", "FORCE_INLINE", None),
    ("19_金蛋拾取.wav", "SFX_P3_GoldEggPick", "PCM", "FORCE_INLINE", None),
    ("20_奶鸡跳格.mp3", "SFX_P3_ItemChickenJump", "PCM", "FORCE_INLINE", None),
    ("21_通关.mp3", "SFX_P3_LevelClear", "PCM", "FORCE_INLINE", None),
    ("22_连杀.wav", "SFX_P3_ComboKill", "PCM", "FORCE_INLINE", None),
    ("23_狂暴stinger.wav", "SFX_P3_RageStinger", "PCM", "FORCE_INLINE", None),
    ("24_迷雾揭开.wav", "SFX_P3_FogReveal", "PCM", "FORCE_INLINE", None),
    ("26_低鸣氛围.wav", "AMB_P3_LowDrone", "PCM", "FORCE_INLINE", None),
]


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_attenuation(asset_tools, name, inner_radius, falloff, max_db=-60.0):
    path = ATT_DEST + "/" + name
    asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if asset is None:
        asset = asset_tools.create_asset(name, ATT_DEST, unreal.SoundAttenuation,
                                         unreal.SoundAttenuationFactory())
    if not asset:
        raise RuntimeError("Could not create attenuation asset: " + path)
    settings = asset.get_editor_property("attenuation")
    settings.set_editor_property("spatialize", True)
    settings.set_editor_property("attenuation_shape", unreal.AttenuationShape.SPHERE)
    settings.set_editor_property("attenuation_shape_extents", unreal.Vector(inner_radius, 0, 0))
    settings.set_editor_property("falloff_distance", falloff)
    settings.set_editor_property("distance_algorithm", unreal.AttenuationDistanceModel.LOGARITHMIC)
    settings.set_editor_property("enable_occlusion", False)
    settings.set_editor_property("enable_reverb_send", False)
    settings.set_editor_property("d_b_attenuation_at_max", max_db)
    asset.set_editor_property("attenuation", settings)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError("Could not save attenuation asset: " + path)
    return asset


def main():
    project_dir = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    if project_dir != PROJECT.resolve():
        raise RuntimeError("Refusing to import into unexpected project: " + str(project_dir))
    if not SOURCE.is_dir():
        raise RuntimeError("Missing versioned P3 audio source folder: " + str(SOURCE))
    unreal.EditorAssetLibrary.make_directory(DEST)
    unreal.EditorAssetLibrary.make_directory(ATT_DEST)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    attenuation = {
        # The top-down audio listener sits about 2050 cm above floor cues.
        "World": make_attenuation(tools, "ATT_P3_World", 2300.0, 1600.0),
        "Long": make_attenuation(tools, "ATT_P3_Long", 250.0, 2500.0),
        "Area": make_attenuation(tools, "ATT_P3_Area", 800.0, 2000.0),
    }
    report = {
        "schema": "NaiwaP3AudioImportV1",
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "engine": unreal.SystemLibrary.get_engine_version(),
        "assets": [],
        "omitted_duplicates": [{
            "source": "25_奶察发现.mp3",
            "asset": "/Game/Naiwa/P2/Audio/SFX_P2_Miss.SFX_P2_Miss",
            "reason": "SHA-256 identical to P2 SFX_P2_Miss",
        }],
    }
    for filename, asset_name, compression_name, loading_name, attenuation_name in CLIPS:
        source_path = SOURCE / filename
        if not source_path.is_file():
            raise RuntimeError("Missing source audio: " + str(source_path))
        package = DEST + "/" + asset_name
        existing = unreal.load_asset(package) if unreal.EditorAssetLibrary.does_asset_exist(package) else None
        task = unreal.AssetImportTask()
        for key, value in (
            ("filename", str(source_path)), ("destination_path", DEST),
            ("destination_name", asset_name), ("automated", True),
            ("replace_existing", existing is not None), ("save", True),
        ):
            task.set_editor_property(key, value)
        tools.import_asset_tasks([task])
        sound = unreal.load_asset(package)
        if sound is None or sound.get_class().get_name() != "SoundWave":
            raise RuntimeError("SoundWave import failed: " + package)
        sound.set_editor_property("looping", False)
        sound.set_sound_asset_compression_type(
            getattr(unreal.SoundAssetCompressionType, compression_name), True)
        sound.set_editor_property("loading_behavior",
                                  getattr(unreal.SoundWaveLoadingBehavior, loading_name))
        sound.set_editor_property("attenuation_settings",
                                  attenuation[attenuation_name] if attenuation_name else None)
        if not unreal.EditorAssetLibrary.save_loaded_asset(sound):
            raise RuntimeError("Could not save P3 SoundWave: " + package)
        asset_file = PROJECT / "Content" / package.removeprefix("/Game/")
        asset_file = asset_file.with_suffix(".uasset")
        report["assets"].append({
            "source": "UE5/SourceAssets/P3/Audio/" + filename,
            "source_sha256": sha256(source_path),
            "asset_path": package + "." + asset_name,
            "asset_sha256": sha256(asset_file),
            "compression": compression_name,
            "loading_behavior": loading_name,
            "looping": False,
            "attenuation": attenuation_name,
        })
        unreal.log_warning("[P3_AUDIO] imported " + asset_name)
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log_warning("[P3_AUDIO] imported %d SoundWaves; report=%s" % (len(report["assets"]), REPORT))


try:
    main()
except Exception as exc:
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps({"status": "failed", "error": repr(exc)},
                                 ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    raise
