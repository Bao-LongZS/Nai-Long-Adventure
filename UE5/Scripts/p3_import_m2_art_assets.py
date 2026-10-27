"""Import approved P3 M2 portraits and sprites without loading or saving a level."""
import json
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PORTRAIT_SOURCE = ROOT / "UE5/SourceAssets/P3/Portraits/Processed"
SPRITE_SOURCE = ROOT / "UE5/SourceAssets/P3/Sprites"
PORTRAIT_DEST = "/Game/Naiwa/P3/Portraits/Processed"
SPRITE_DEST = "/Game/Naiwa/P3/Sprites"
MATERIAL_DEST = "/Game/Naiwa/P3/Materials"
PARENT_MATERIAL = f"{MATERIAL_DEST}/M_P3Portrait"
REPORT = ROOT / "UE5/Records/p3_m2/art_import_report.json"


def import_texture(source: Path, destination: str):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", source.stem)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset_path = f"{destination}/{source.stem}"
    texture = unreal.load_asset(asset_path)
    assert texture, f"Could not import approved P3 texture: {source}"
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SHARPEN0)
    assert unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False), (
        f"Could not save P3 texture: {asset_path}"
    )
    return texture


def create_texture_material(name: str, texture, parameterized: bool):
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_path = f"{MATERIAL_DEST}/{name}"
    # Keep existing authored master materials intact. The Python wrapper for
    # DeleteMaterialExpression can assert in UE 5.8 when editing rooted graph
    # nodes, so only create the named preview materials once.
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return asset_path
    material = asset_tools.create_asset(name, MATERIAL_DEST, unreal.Material, unreal.MaterialFactoryNew())
    assert material, f"Could not create P3 sprite material: {asset_path}"
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("opacity_mask_clip_value", 0.08)
    sample_type = (unreal.MaterialExpressionTextureSampleParameter2D if parameterized
                   else unreal.MaterialExpressionTextureSample)
    sample = unreal.MaterialEditingLibrary.create_material_expression(material, sample_type, -620, 0)
    sample.set_editor_property("texture", texture)
    if parameterized:
        sample.set_editor_property("parameter_name", unreal.Name("Portrait"))
    unreal.MaterialEditingLibrary.connect_material_property(
        sample, "A", unreal.MaterialProperty.MP_OPACITY_MASK
    )

    if parameterized:
        color = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionVectorParameter, -620, 260
        )
        color.set_editor_property("parameter_name", unreal.Name("Color"))
        color.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
        multiply = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionMultiply, -260, 0
        )
        unreal.MaterialEditingLibrary.connect_material_expressions(sample, "RGB", multiply, "A")
        unreal.MaterialEditingLibrary.connect_material_expressions(color, "", multiply, "B")
        unreal.MaterialEditingLibrary.connect_material_property(
            multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
        )
    else:
        unreal.MaterialEditingLibrary.connect_material_property(
            sample, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR
        )

    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False), (
        f"Could not save P3 sprite material: {asset_path}"
    )
    return asset_path


def main():
    unreal.EditorAssetLibrary.make_directory(PORTRAIT_DEST)
    unreal.EditorAssetLibrary.make_directory(SPRITE_DEST)
    unreal.EditorAssetLibrary.make_directory(MATERIAL_DEST)

    portraits = sorted(PORTRAIT_SOURCE.glob("SPR_*.png"))
    sprites = sorted(SPRITE_SOURCE.glob("SPR_*.png"))
    required_portraits = {
        "SPR_Player_Neutral_A", "SPR_Player_Hit_A", "SPR_Naicha_Chase_A",
        "SPR_Naicha_Catch_A", "SPR_Naiyu_Neutral_A", "SPR_Naiyu_Attack_A",
        "SPR_Naisha_Rage_A", "SPR_Naihao_Neutral_A",
    }
    assert required_portraits.issubset({source.stem for source in portraits}), (
        f"Approved portrait set incomplete: {sorted(source.stem for source in portraits)}"
    )
    assert len(sprites) == 28, f"Expected 28 approved world/HUD sprites, found {len(sprites)}"

    imported_portraits = {source.stem: import_texture(source, PORTRAIT_DEST) for source in portraits}
    imported_sprites = {source.stem: import_texture(source, SPRITE_DEST) for source in sprites}
    all_textures = {**imported_portraits, **imported_sprites}

    parent = create_texture_material("M_P3Portrait", imported_portraits["SPR_Player_Neutral_A"], True)
    materials = {}
    for texture_name, texture in all_textures.items():
        material_name = f"M_P3Sprite_{texture_name}"
        materials[texture_name] = create_texture_material(material_name, texture, False)

    report = {
        "status": "approved_art_imported",
        "engine": unreal.SystemLibrary.get_engine_version(),
        "portrait_count": len(imported_portraits),
        "sprite_count": len(imported_sprites),
        "parent_material": parent,
        "texture_paths": {
            **{name: f"{PORTRAIT_DEST}/{name}" for name in imported_portraits},
            **{name: f"{SPRITE_DEST}/{name}" for name in imported_sprites},
        },
        "preview_materials": materials,
        "level_loaded_or_saved": False,
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log(f"P3 M2 approved art import complete: {len(imported_portraits)} portraits, "
               f"{len(imported_sprites)} sprites; no level was loaded or saved.")


try:
    main()
except Exception:
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps({"status": "failed"}, ensure_ascii=False, indent=2), encoding="utf-8")
    raise
