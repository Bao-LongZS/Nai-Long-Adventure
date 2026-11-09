"""Import existing P5 WIP art into an isolated v06 asset package.

This script only imports textures and builds materials. It never loads, creates,
or saves a level; the existing A0 sample map remains under the owner's control.
"""
from __future__ import annotations

import json
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = ROOT / "UE5/SourceAssets/P5"
ART_ROOT = "/Game/Naiwa/P5/ArtSample_v06"
TEXTURE_DEST = ART_ROOT + "/Textures"
MATERIAL_DEST = ART_ROOT + "/Materials"
REPORT_PATH = ROOT / "UE5/Records/p5_m2/P5_A0_ArtSample_v06_asset_import_report_2026-10-30.json"

ASSETS = [
    {"key": "a4_body", "source": "Characters/Exports/P5_A4_NaiwaBodyAtlas_v05_WIP.png",
     "texture": "T_P5_A4_NaiwaBodyAtlas_v05_WIP", "material": "M_P5_A4_NaiwaBodyAtlas_v05_WIP", "alpha": "masked", "mask_clip": 0.5},
    {"key": "a4_snowboard", "source": "Characters/Exports/P5_A4_Snowboard_v01_WIP.png",
     "texture": "T_P5_A4_Snowboard_v01_WIP", "material": "M_P5_A4_Snowboard_v01_WIP", "alpha": "masked", "mask_clip": 0.25},
    {"key": "a5_mount", "source": "Environment/Exports/P5_A5_KangarooMount_v02_WIP.png",
     "texture": "T_P5_A5_KangarooMount_v02_WIP", "material": "M_P5_A5_KangarooMount_v02_WIP", "alpha": "masked", "mask_clip": 0.35},
    {"key": "a6_land_puff", "source": "FX/Exports/FX_LandPuff_v01_WIP.png",
     "texture": "T_P5_A6_LandPuff_v01_WIP", "material": "M_P5_A6_LandPuff_v01_WIP", "alpha": "translucent"},
    {"key": "a6_snowflake", "source": "FX/Exports/FX_Snowflake_v01_WIP.png",
     "texture": "T_P5_A6_Snowflake_v01_WIP", "material": "M_P5_A6_Snowflake_v01_WIP", "alpha": "translucent"},
    {"key": "a3_tree_b", "source": "Environment/Exports/O_Tree_B_v01_WIP.png",
     "texture": "T_P5_A3_TreeB_v01_WIP", "material": "M_P5_A0_TreeB", "alpha": "masked"},
    {"key": "a3_tree_c", "source": "Environment/Exports/O_Tree_C_v01_WIP.png",
     "texture": "T_P5_A3_TreeC_v01_WIP", "material": "M_P5_A0_TreeC", "alpha": "masked"},
    {"key": "a3_rock_b", "source": "Environment/Exports/O_Rock_B_v01_WIP.png",
     "texture": "T_P5_A3_RockB_v01_WIP", "material": "M_P5_A0_RockB", "alpha": "masked"},
    {"key": "a2_snow_track", "source": "Materials/Exports/T_Snow_Track_v01_WIP.png",
     "texture": "T_P5_A2_SnowTrack_v01_WIP", "material": "M_P5_A2_SnowTrack_v01_WIP", "alpha": "translucent"},
    {"key": "a1_mountain_far", "source": "Backgrounds/Exports/BG_Mountain_Far_v03_WIP.png",
     "texture": "T_P5_A0_MountainFar_v03_WIP", "material": "M_P5_A0_MountainFar", "alpha": "translucent"},
    {"key": "a1_forest_mid", "source": "Backgrounds/Exports/BG_Forest_Mid_v03_WIP.png",
     "texture": "T_P5_A0_ForestMid_v03_WIP", "material": "M_P5_A0_ForestMid", "alpha": "translucent"},
    {"key": "a1_ridge_near", "source": "Backgrounds/Exports/BG_Ridge_Near_v04_WIP.png",
     "texture": "T_P5_A0_RidgeNear_v04_WIP", "material": "M_P5_A0_RidgeNear", "alpha": "translucent"},
    {"key": "a7_palette", "source": "Materials/Exports/Palette_L4_v01_WIP.png",
     "texture": "T_P5_A7_PaletteL4_v01_WIP", "material": None, "alpha": None},
]


def import_texture(source: Path, asset_name: str) -> tuple[object, str]:
    if not source.is_file():
        raise FileNotFoundError(f"Missing P5 WIP art: {source}")
    asset_path = f"{TEXTURE_DEST}/{asset_name}"
    task = unreal.AssetImportTask()
    task.set_editor_properties({
        "filename": str(source),
        "destination_path": TEXTURE_DEST,
        "destination_name": asset_name,
        "replace_existing": unreal.EditorAssetLibrary.does_asset_exist(asset_path),
        "automated": True,
        "save": True,
    })
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(asset_path)
    if not texture:
        raise RuntimeError(f"Texture import did not create {asset_path}")
    texture.set_editor_properties({
        "srgb": True,
        "never_stream": True,
        "compression_settings": unreal.TextureCompressionSettings.TC_DEFAULT,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_SHARPEN2,
    })
    if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save imported texture {asset_path}")
    return texture, asset_path


def make_texture_material(name: str, texture: object, alpha_mode: str,
                          mask_clip: float = 0.01) -> str:
    if alpha_mode not in ("masked", "translucent", "opaque"):
        raise ValueError(f"Unsupported alpha mode: {alpha_mode}")
    path = f"{MATERIAL_DEST}/{name}"
    material = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, MATERIAL_DEST, unreal.Material, unreal.MaterialFactoryNew()
        )
    if not material:
        raise RuntimeError(f"Could not create material {path}")

    blend_mode = {
        "masked": unreal.BlendMode.BLEND_MASKED,
        "translucent": unreal.BlendMode.BLEND_TRANSLUCENT,
        "opaque": unreal.BlendMode.BLEND_OPAQUE,
    }[alpha_mode]
    material.set_editor_properties({
        "shading_model": unreal.MaterialShadingModel.MSM_UNLIT,
        "blend_mode": blend_mode,
        "two_sided": True,
    })
    if alpha_mode == "masked":
        material.set_editor_property("opacity_mask_clip_value", mask_clip)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, 0, 0
    )
    sample.set_editor_properties({"parameter_name": "A0Texture", "texture": texture})
    uv = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -1000, 240
    )
    scale = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -1000, 440
    )
    scale.set_editor_properties({
        "parameter_name": "A0UVScale",
        "default_value": unreal.LinearColor(1.0, 1.0, 0.0, 0.0),
    })
    offset = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -1000, 640
    )
    offset.set_editor_properties({
        "parameter_name": "A0UVOffset",
        "default_value": unreal.LinearColor(0.0, 0.0, 0.0, 0.0),
    })
    scale_rg = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionComponentMask, -760, 460
    )
    scale_rg.set_editor_properties({"r": True, "g": True, "b": False, "a": False})
    offset_rg = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionComponentMask, -760, 660
    )
    offset_rg.set_editor_properties({"r": True, "g": True, "b": False, "a": False})
    multiply = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -500, 300
    )
    add = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, -250, 300
    )
    connections = (
        (uv, "", multiply, "A"),
        (scale, "", scale_rg, ""),
        (scale_rg, "", multiply, "B"),
        (multiply, "", add, "A"),
        (offset, "", offset_rg, ""),
        (offset_rg, "", add, "B"),
        (add, "", sample, "UVs"),
    )
    for source, output, target, input_name in connections:
        if not unreal.MaterialEditingLibrary.connect_material_expressions(
            source, output, target, input_name
        ):
            raise RuntimeError(f"Could not wire UV input {input_name!r} for {path}")
    if not unreal.MaterialEditingLibrary.connect_material_property(
        sample, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    ):
        raise RuntimeError(f"Could not wire color input for {path}")
    alpha_property = (unreal.MaterialProperty.MP_OPACITY_MASK if alpha_mode == "masked"
                      else unreal.MaterialProperty.MP_OPACITY if alpha_mode == "translucent" else None)
    if alpha_property and not unreal.MaterialEditingLibrary.connect_material_property(
        sample, "A", alpha_property
    ):
        raise RuntimeError(f"Could not wire alpha input for {path}")
    errors = unreal.MaterialEditingLibrary.recompile_material(material)
    if errors:
        raise RuntimeError(f"Material compile failed for {path}: {errors}")
    if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save material {path}")
    if material.get_editor_property("blend_mode") != blend_mode:
        raise RuntimeError(f"Material blend mode did not persist for {path}")
    return path


def main() -> None:
    result = {
        "status": "running",
        "package": ART_ROOT,
        "scope": "isolated P5 candidate assets; no map was loaded, created, or saved",
        "engine": unreal.SystemLibrary.get_engine_version(),
        "assets": [],
    }
    unreal.EditorAssetLibrary.make_directory(TEXTURE_DEST)
    unreal.EditorAssetLibrary.make_directory(MATERIAL_DEST)
    for item in ASSETS:
        source = SOURCE_ROOT / item["source"]
        texture, texture_path = import_texture(source, item["texture"])
        material_path = None
        if item["material"]:
            material_path = make_texture_material(item["material"], texture, item["alpha"],
                item.get("mask_clip", 0.01))
        result["assets"].append({
            "key": item["key"],
            "source": str(source.relative_to(ROOT)).replace("\\", "/"),
            "texture": texture_path,
            "material": material_path,
            "alpha_mode": item["alpha"],
            "opacity_mask_clip_value": item.get("mask_clip") if item["alpha"] == "masked" else None,
            "wip": True,
        })
    result["status"] = "assets_imported_pending_editor_review"
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


try:
    main()
except Exception as error:  # write evidence even when a particular import/material fails
    failure = {
        "status": "asset_import_failed",
        "package": ART_ROOT,
        "scope": "isolated P5 candidate assets; no map was loaded, created, or saved",
        "error": repr(error),
        "traceback": traceback.format_exc(),
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(failure, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    raise
