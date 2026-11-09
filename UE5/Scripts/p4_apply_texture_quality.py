"""Update P4 imported 2D textures to a lossless fixed-camera profile."""
from pathlib import Path
import json
import traceback
import unreal

ROOT = Path(__file__).resolve().parents[2]
P4 = ROOT / "UE5/SourceAssets/P4"
REPORT = ROOT / "UE5/Records/p4_alpha/evidence/A2/P4_TextureQuality_report.json"
report = {"status": "running", "level_opened_or_changed": False, "runtime_played": False, "assets": []}

def configure(path, profile):
    texture = unreal.load_asset(path)
    if not texture or not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Missing Texture2D: {path}")
    if profile == "character":
        settings = {"srgb": True, "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
                    "compression_no_alpha": False, "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_SHARPEN0,
                    "lod_group": unreal.TextureGroup.TEXTUREGROUP_CHARACTER, "never_stream": True}
        mip_name, group_name = "Sharpen0", "Character"
    elif profile == "background":
        settings = {"srgb": True, "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
                    "compression_no_alpha": False, "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_SHARPEN0,
                    "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI, "never_stream": True}
        mip_name, group_name = "Sharpen0", "UI"
    else:
        settings = {"srgb": True, "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
                    "compression_no_alpha": False, "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
                    "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI, "never_stream": True}
        mip_name, group_name = "NoMipmaps", "UI"
    texture.set_editor_properties(settings)
    for key, expected in settings.items():
        if texture.get_editor_property(key) != expected:
            raise RuntimeError(f"Readback failed for {path}: {key}")
    if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {path}")
    report["assets"].append({"asset_path": path, "profile": profile, "compression": "TC_EditorIcon (uncompressed RGBA8)",
                             "mip_generation": mip_name, "texture_group": group_name, "never_stream": True,
                             "srgb": True, "alpha_compression": False, "readback": "passed"})

def main():
    a1 = json.loads((P4 / "Manifests/P4_A1_UE_Import_manifest_v02.json").read_text(encoding="utf-8"))
    w3 = json.loads((P4 / "Manifests/P4_W3_StaticPreview_manifest_v02.json").read_text(encoding="utf-8"))
    a3 = json.loads((P4 / "Manifests/P4_A3_RuntimeRaster_manifest_v01.json").read_text(encoding="utf-8"))
    for frame in a1["frames"]:
        configure(f"/Game/Naiwa/P4/Art/Alpha/A1/Frames/{frame['action']}/T_P4_A1_{frame['id']}", "character")
    for asset in w3["assets"]:
        configure(asset["texture_asset"], "character")
    for asset in a3["assets"]:
        name = Path(asset["output_png"]).stem
        configure(f"/Game/Naiwa/P4/UI/Alpha/RuntimeReady/{name}", "ui")
    configure("/Game/Naiwa/P4/Art/Alpha/A2/T_P4_A2_ArenaScene_v02", "background")
    report.update({"status": "passed_property_readback_pending_visual_review", "engine": unreal.SystemLibrary.get_engine_version(),
                   "character_asset_count": sum(item["profile"] == "character" for item in report["assets"]),
                   "ui_asset_count": sum(item["profile"] == "ui" for item in report["assets"]),
                   "background_asset_count": sum(item["profile"] == "background" for item in report["assets"]),
                   "limitations": ["Texture settings were read back; runtime visual review is still required."]})

try:
    main()
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    raise
finally:
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("NAIWA_P4_TEXTURE_QUALITY " + json.dumps(report, ensure_ascii=False))
