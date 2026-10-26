"""Brighten P2 sky, scenery layers, ground, and foliage without changing egg materials."""

import json
from datetime import datetime, timezone
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
PACKAGE_ROOT = "/Game/Naiwa/P2/Art/Geo/Materials"
REPORT_PATH = ROOT / "UE5/Records/p2_geo_palette_update.json"

# Softer, brighter pastel tones bring the backdrop closer to the warm egg colors.
PALETTE = {
    "MI_P2Geo_Cloud": "F8FCFF",
    "MI_P2Geo_Sky1": "A9D6EE",
    "MI_P2Geo_Sky2": "BAE0F1",
    "MI_P2Geo_SkyTop": "C7E5F4",
    "MI_P2Geo_SkyMid": "DDEFF8",
    "MI_P2Geo_SkyBot": "F2F9FD",
    "MI_P2Geo_L4": "EAF5EE",
    "MI_P2Geo_L3": "D9EDDF",
    "MI_P2Geo_L2": "C4E3CF",
    "MI_P2Geo_L1": "A3D5B7",
    "MI_P2Geo_Ground": "D5E5B0",
    "MI_P2Geo_Light": "F8F8E8",
    "MI_P2Geo_Leaf": "90CA7A",
    "MI_P2Geo_LeafLight": "A6D98C",
}


def hex_to_linear(value):
    def channel(start):
        encoded = int(value[start:start + 2], 16) / 255.0
        return encoded / 12.92 if encoded <= 0.04045 else ((encoded + 0.055) / 1.055) ** 2.4

    return unreal.LinearColor(channel(0), channel(2), channel(4), 1.0)


project_dir = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
if project_dir != PROJECT.resolve():
    raise RuntimeError("Refusing to edit assets in unexpected project: " + str(project_dir))

report = {
    "schema": "NaiwaP2GeoPaletteUpdateV1",
    "updated_utc": datetime.now(timezone.utc).isoformat(),
    "assets": [],
}

for name, color in PALETTE.items():
    package = PACKAGE_ROOT + "/" + name
    material = unreal.load_asset(package)
    if material is None or material.get_class().get_name() != "MaterialInstanceConstant":
        raise RuntimeError("Missing P2 Geo material instance: " + package)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        material, "BaseColor", hex_to_linear(color)
    )
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Could not save P2 Geo material instance: " + package)
    report["assets"].append({"asset_path": package + "." + name, "srgb_hex": color})
    unreal.log_warning("[P2_GEO_PALETTE] %s #%s" % (name, color))

REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
unreal.log_warning("[P2_GEO_PALETTE] updated %d materials" % len(report["assets"]))
