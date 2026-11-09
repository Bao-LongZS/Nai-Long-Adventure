"""Build an isolated P4 art-integration preview from approved A1/A3 sources.

The script owns only its preview map, its named P4 integration materials,
two P4 presentation DataTables, and assets under /Game/Naiwa/P4/UI/Alpha.
It never changes the W1 calibration map or P1/P2/P3 content.
"""
from __future__ import annotations

import csv
import hashlib
import json
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
P4_SOURCE = ROOT / "UE5/SourceAssets/P4"
A1_MANIFEST_PATH = P4_SOURCE / "Manifests/P4_A1_UE_Import_manifest_v02.json"
A3_RASTER_MANIFEST_PATH = P4_SOURCE / "Manifests/P4_A3_RuntimeRaster_manifest_v01.json"
UPGRADE_CSV_PATH = P4_SOURCE / "Data/P4UpgradeAlpha.csv"
FRAME_CSV_PATH = P4_SOURCE / "Data/P4_A1_RuntimeFrames.csv"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/A4/P4_ArtIntegrationPreview_build_report_v02.json"

MAP_PATH = "/Game/Naiwa/P4/Maps/L_NaiwaP4_ArtIntegrationPreview"
UI_ASSET_ROOT = "/Game/Naiwa/P4/UI/Alpha/RuntimeReady"
MATERIAL_ROOT = "/Game/Naiwa/P4/Materials/Alpha/A1"
FRAME_TABLE_PATH = "/Game/Naiwa/P4/Data/DT_P4_A1_RuntimeFrames"
UPGRADE_TABLE_PATH = "/Game/Naiwa/P4/Data/DT_P4_Upgrade_Alpha"
SPRITE_MATERIAL_PATH = f"{MATERIAL_ROOT}/M_P4_A1_SpriteRuntime"
OWNER_TAG = "NaiwaP4ArtIntegrationPreview"
ACTOR_TAG = "NaiwaP4ArtIntegrationGenerated"
GAME_MODE_PATH = "/Script/NaiwaRuntime.NaiwaP4ArtIntegrationGameMode"
CAMERA_PATH = "/Script/NaiwaRuntime.NaiwaP4CameraRig"

report = {
    "schema": "naiwa.p4.art_integration_preview_build/1",
    "status": "running",
    "map": MAP_PATH,
    "engine": unreal.SystemLibrary.get_engine_version(),
    "level_opened_or_changed": False,
    "runtime_played": False,
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def asset_object_path(package_path: str) -> str:
    asset_name = package_path.rsplit("/", 1)[-1]
    return f"{package_path}.{asset_name}"


def write_frame_csv(manifest: dict) -> list[str]:
    if manifest.get("schema") != "naiwa.p4.alpha_ue_import/1":
        raise ValueError(f"Unexpected A1 import manifest schema: {manifest.get('schema')!r}")
    if manifest.get("status") != "owner_approved_for_ue_import":
        raise ValueError(f"A1 import manifest is not approved: {manifest.get('status')!r}")
    frames = manifest.get("frames", [])
    if len(frames) != 33:
        raise ValueError(f"Expected 33 approved A1 frames, found {len(frames)}")

    rows = []
    for frame in frames:
        frame_id = frame["id"]
        action = frame["action"]
        pose = frame["pose"]
        view = frame["view"]
        foot_anchor = frame["foot_anchor_px_estimate"]
        if len(foot_anchor) != 2 or not (0 <= foot_anchor[1] <= 723):
            raise ValueError(f"Invalid foot anchor for {frame_id}: {foot_anchor}")
        texture_asset = f"/Game/Naiwa/P4/Art/Alpha/A1/Frames/{action}/T_P4_A1_{frame_id}"
        texture_object = asset_object_path(texture_asset)
        rows.append([
            frame_id,
            action,
            pose,
            view,
            f"Texture2D'{texture_object}'",
            foot_anchor[1],
        ])

    with FRAME_CSV_PATH.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(["Name", "ActionId", "PoseId", "ViewId", "FrameTexture", "FootAnchorYPx"])
        writer.writerows(rows)
    return [row[0] for row in rows]


def import_data_table(csv_path: Path, name: str, row_struct):
    factory = unreal.CSVImportFactory()
    settings = factory.get_editor_property("automated_import_settings")
    settings.set_editor_property("import_row_struct", row_struct)
    factory.set_editor_property("automated_import_settings", settings)

    task = unreal.AssetImportTask()
    task.set_editor_properties({
        "filename": str(csv_path),
        "destination_path": "/Game/Naiwa/P4/Data",
        "destination_name": name,
        "automated": True,
        "replace_existing": True,
        "save": True,
        "factory": factory,
    })
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset_path = f"/Game/Naiwa/P4/Data/{name}"
    table = unreal.load_asset(asset_path)
    if not table:
        raise RuntimeError(f"CSV import did not create {asset_path}: {task.get_editor_property('imported_object_paths')}")
    actual_struct = unreal.DataTableFunctionLibrary.get_data_table_row_struct(table)
    if not actual_struct or actual_struct.get_path_name() != row_struct.get_path_name():
        raise RuntimeError(f"Unexpected row struct for {asset_path}: {actual_struct}")
    return table


def import_ui_textures(raster_manifest: dict) -> list[dict]:
    if raster_manifest.get("schema") != "naiwa.p4.a3_runtime_raster/1":
        raise ValueError("Unexpected A3 raster manifest schema")
    if raster_manifest.get("status") not in {"rasterized_pending_ue_import", "imported_to_p4_content"}:
        raise ValueError(f"A3 raster sources are not ready to import: {raster_manifest.get('status')!r}")

    unreal.EditorAssetLibrary.make_directory(UI_ASSET_ROOT)
    imported = []
    for entry in raster_manifest.get("assets", []):
        source = (ROOT / entry["output_png"]).resolve()
        if not source.is_file() or sha256_file(source) != entry["output_sha256"]:
            raise ValueError(f"A3 raster hash check failed: {source}")
        asset_name = source.stem
        asset_path = f"{UI_ASSET_ROOT}/{asset_name}"
        texture = unreal.load_asset(asset_path)
        imported_now = False
        if not texture:
            task = unreal.AssetImportTask()
            task.set_editor_properties({
                "filename": str(source),
                "destination_path": UI_ASSET_ROOT,
                "destination_name": asset_name,
                "replace_existing": False,
                "automated": True,
                "save": True,
            })
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            texture = unreal.load_asset(asset_path)
            imported_now = True
        if not texture or not isinstance(texture, unreal.Texture2D):
            raise RuntimeError(f"A3 PNG import did not produce Texture2D: {asset_path}")
        if not imported_now:
            import_data = texture.get_editor_property("asset_import_data")
            existing_source = import_data.get_first_filename() if import_data else ""
            if not existing_source or Path(existing_source).resolve().as_posix().casefold() != source.as_posix().casefold():
                raise RuntimeError(f"Refusing to update an A3 texture with a different or unknown source: {asset_path}")
        texture.set_editor_properties({
            "srgb": True,
            "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
            "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
            "compression_no_alpha": False,
            "never_stream": True,
        })
        expected_settings = {
            "srgb": True,
            "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
            "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
            "compression_no_alpha": False,
            "never_stream": True,
        }
        for property_name, expected in expected_settings.items():
            if texture.get_editor_property(property_name) != expected:
                raise RuntimeError(f"A3 texture setting readback failed for {asset_path}: {property_name}")
        if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save UI Texture2D: {asset_path}")
        imported.append({
            "asset_id": entry["asset_id"],
            "asset_path": asset_path,
            "source_png": entry["output_png"],
            "source_sha256": entry["output_sha256"],
            "dimensions_px": entry["dimensions_px"],
            "alpha": entry["alpha"],
            "texture_group": "UI",
            "compression": "TC_EditorIcon (uncompressed RGBA8)",
            "mip_generation": "NoMipmaps",
            "result": "imported" if imported_now else "reused_same_source",
            "texture_readback": "passed",
        })
    if len(imported) != 6:
        raise RuntimeError(f"Expected 6 A3 runtime PNGs, imported {len(imported)}")
    return imported


def make_sprite_material(default_texture):
    unreal.EditorAssetLibrary.make_directory(MATERIAL_ROOT)
    material = unreal.load_asset(SPRITE_MATERIAL_PATH)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_P4_A1_SpriteRuntime", MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew()
        )
    if not material:
        raise RuntimeError(f"Could not create A1 runtime material: {SPRITE_MATERIAL_PATH}")
    material.set_editor_properties({
        "shading_model": unreal.MaterialShadingModel.MSM_UNLIT,
        "blend_mode": unreal.BlendMode.BLEND_TRANSLUCENT,
        "two_sided": True,
    })
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, 0, 0
    )
    sample.set_editor_properties({
        "texture": default_texture,
        "parameter_name": unreal.Name("P4FrameTexture"),
    })
    if not unreal.MaterialEditingLibrary.connect_material_property(
        sample, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    ):
        raise RuntimeError("Could not connect A1 sprite RGB to emissive color")
    if not unreal.MaterialEditingLibrary.connect_material_property(
        sample, "A", unreal.MaterialProperty.MP_OPACITY
    ):
        raise RuntimeError("Could not connect A1 sprite alpha to translucent opacity")
    errors = unreal.MaterialEditingLibrary.recompile_material(material)
    if errors:
        raise RuntimeError(f"A1 runtime sprite material compile errors: {errors}")
    if not unreal.EditorAssetLibrary.save_asset(SPRITE_MATERIAL_PATH, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save A1 runtime material: {SPRITE_MATERIAL_PATH}")
    return material


def set_owned(actor, label: str):
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(ACTOR_TAG)])
    return actor


def prepare_preview_map(game_mode_class, camera_class):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    unreal.EditorAssetLibrary.make_directory("/Game/Naiwa/P4/Maps")
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not levels.load_level(MAP_PATH):
            raise RuntimeError(f"Could not load existing P4 art integration map: {MAP_PATH}")
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        settings = world.get_world_settings()
        tags = [str(tag) for tag in settings.get_editor_property("tags")]
        if OWNER_TAG not in tags:
            raise RuntimeError(f"Refusing to replace an unowned map: {MAP_PATH}")
        if "NaiwaP4A2RefinedArena" not in tags:
            tags.append("NaiwaP4A2RefinedArena")
            settings.set_editor_property("tags", [unreal.Name(tag) for tag in tags])
        for actor in actor_subsystem.get_all_level_actors():
            tags = [str(tag) for tag in actor.get_editor_property("tags")]
            if ACTOR_TAG in tags:
                if not actor_subsystem.destroy_actor(actor):
                    raise RuntimeError(f"Could not clear generated actor {actor.get_name()}")
    else:
        if not levels.new_level(MAP_PATH):
            raise RuntimeError(f"Could not create P4 art integration map: {MAP_PATH}")
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        settings = world.get_world_settings()
        settings.set_editor_property("tags", [
            unreal.Name(OWNER_TAG), unreal.Name("NaiwaP4A2RefinedArena")
        ])
    settings.set_editor_property("default_game_mode", game_mode_class)
    camera = actor_subsystem.spawn_actor_from_class(camera_class, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    if not camera:
        raise RuntimeError("Could not spawn calibrated P4 camera rig")
    set_owned(camera, "P4A4_FixedCamera")
    player_start = actor_subsystem.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(550, 92.5, 0), unreal.Rotator(0, 0, 0)
    )
    if not player_start:
        raise RuntimeError("Could not spawn the P4 art-integration PlayerStart")
    set_owned(player_start, "P4A4_PlayerStart")

    if not levels.save_current_level():
        raise RuntimeError(f"Could not save P4 art integration preview map: {MAP_PATH}")


def update_manifest(path: Path, update):
    data = json.loads(path.read_text(encoding="utf-8"))
    update(data)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def run() -> dict:
    a1_manifest = json.loads(A1_MANIFEST_PATH.read_text(encoding="utf-8"))
    a3_raster_manifest = json.loads(A3_RASTER_MANIFEST_PATH.read_text(encoding="utf-8"))
    if not UPGRADE_CSV_PATH.is_file():
        raise FileNotFoundError(f"Missing Alpha card source CSV: {UPGRADE_CSV_PATH}")

    expected_frame_ids = write_frame_csv(a1_manifest)
    ui_textures = import_ui_textures(a3_raster_manifest)

    default_texture_path = "/Game/Naiwa/P4/Art/Alpha/A1/Frames/idle/T_P4_A1_idle_neutral_front"
    default_texture = unreal.load_asset(default_texture_path)
    if not default_texture:
        raise RuntimeError(f"The approved A1 frame import is missing: {default_texture_path}")
    sprite_material = make_sprite_material(default_texture)

    frame_table = import_data_table(
        FRAME_CSV_PATH, "DT_P4_A1_RuntimeFrames", unreal.NaiwaP4A1FrameRow.static_struct()
    )
    frame_ids = sorted(str(name) for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(frame_table))
    if frame_ids != sorted(expected_frame_ids):
        raise RuntimeError(f"A1 frame table rows do not match the manifest: {frame_ids}")

    upgrade_table = import_data_table(
        UPGRADE_CSV_PATH, "DT_P4_Upgrade_Alpha", unreal.NaiwaP4UpgradeRow.static_struct()
    )
    upgrade_ids = sorted(str(name) for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(upgrade_table))
    with UPGRADE_CSV_PATH.open("r", newline="", encoding="utf-8-sig") as stream:
        expected_upgrade_ids = sorted(row.get("Name", "").strip() for row in csv.DictReader(stream))
    if not expected_upgrade_ids or "" in expected_upgrade_ids or len(set(expected_upgrade_ids)) != len(expected_upgrade_ids):
        raise ValueError(f"Alpha upgrade CSV has missing or duplicate row names: {expected_upgrade_ids}")
    if upgrade_ids != expected_upgrade_ids:
        raise RuntimeError(f"Unexpected Alpha upgrade rows: {upgrade_ids}")

    game_mode_class = unreal.load_class(None, GAME_MODE_PATH)
    camera_class = unreal.load_class(None, CAMERA_PATH)
    if not game_mode_class or not camera_class:
        raise RuntimeError("P4 runtime classes are unavailable; compile the NaiwaRuntime module first")

    arena_material_path = "/Game/Naiwa/P4/Materials/Alpha/A2/M_P4_A2_RefinedBackdrop_v02"
    arena_material = unreal.load_asset(arena_material_path)
    if not arena_material or not isinstance(arena_material, unreal.MaterialInterface):
        raise RuntimeError(f"Refined A2 arena material is missing: {arena_material_path}")
    prepare_preview_map(game_mode_class, camera_class)
    report["level_opened_or_changed"] = True

    update_manifest(
        P4_SOURCE / "Manifests/P4_A3_AlphaCardIcons_manifest_v02.json",
        lambda data: data.update({"runtime_imported": True, "runtime_asset_root": UI_ASSET_ROOT}),
    )
    update_manifest(
        P4_SOURCE / "Manifests/P4_A3_CategoryMarks_manifest_v01.json",
        lambda data: data.update({"runtime_imported": True, "runtime_asset_root": UI_ASSET_ROOT}),
    )
    update_manifest(
        P4_SOURCE / "Manifests/P4_A3_AlphaCardFaces_manifest_v03.json",
        lambda data: data.update({
            "runtime_imported": True,
            "runtime_integrated": True,
            "runtime_preview_map": MAP_PATH,
        }),
    )
    update_manifest(
        P4_SOURCE / "Manifests/P4_A3_HUDAndTelegraph_manifest_v02.json",
        lambda data: data.update({
            "runtime_imported": True,
            "runtime_integrated": True,
            "runtime_preview_map": MAP_PATH,
            "runtime_limit": "Native HUD, card offer, and T-cycled telegraph style preview are wired; energy, wave, enemy, and world telegraph gameplay data await combat simulation.",
        }),
    )
    a3_raster_manifest["status"] = "imported_to_p4_content"
    a3_raster_manifest["runtime_asset_root"] = UI_ASSET_ROOT
    A3_RASTER_MANIFEST_PATH.write_text(json.dumps(a3_raster_manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    return {
        "status": "preview_assets_generated_pending_visual_review",
        "map": MAP_PATH,
        "frame_table": FRAME_TABLE_PATH,
        "frame_rows": len(frame_ids),
        "upgrade_table": UPGRADE_TABLE_PATH,
        "upgrade_rows": upgrade_ids,
        "sprite_material": SPRITE_MATERIAL_PATH,
        "sprite_material_alpha_mode": "Translucent; straight-alpha RGBA is connected to material opacity to preserve antialiased sprite edges",
        "texture_edge_quality": "Uncompressed RGBA8 with Sharpen0 mips; avoids emphasizing low-alpha source-edge specks",
        "ui_textures": ui_textures,
        "arena_backdrop_material": arena_material.get_path_name(),
        "a3_runtime_limit": "HUD health is read from P4 simulation; a T-cycled HUD style preview shows attack ring, displacement arrow, and unblockable starburst. Energy, wave, enemy, and world telegraph gameplay data are not implemented yet.",
        "card_confirm_limit": "Preview acknowledgement only; does not grant an upgrade.",
        "runtime_played": False,
    }


try:
    result = run()
    report.update(result)
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()

REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
unreal.log("NAIWA_P4_ART_INTEGRATION_PREVIEW " + json.dumps(report, ensure_ascii=False))
if report.get("status") != "preview_assets_generated_pending_visual_review":
    raise RuntimeError(report.get("error", "P4 art integration preview build failed"))
