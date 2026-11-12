"""Import the Beta6 Nai Jia turnaround as a review reference texture only."""
from __future__ import annotations

import hashlib
import json
import struct
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = ROOT / "UE5/SourceAssets/P4/Beta6/Characters/Bosses"
MANIFEST_PATH = SOURCE_ROOT / "P4_Beta6_BossNaiJiaTurnaround_manifest_v01.json"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_BossNaiJiaTurnaround_import_report_v01.json"
CONTENT_ROOT = "/Game/Naiwa/P4/Art/Beta6/Characters/Reference"

IMPORT_SETTINGS = {
    "srgb": True,
    "compression_settings": unreal.TextureCompressionSettings.TC_DEFAULT,
    "compression_no_alpha": True,
    "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
    "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
    "never_stream": True,
}

report = {
    "schema": "naiwa.p4.beta6.character_reference_import/1",
    "status": "running",
    "engine": unreal.SystemLibrary.get_engine_version(),
    "runtime_reference": "none; static character turnaround reference only",
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def inspect_png(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"Expected a valid PNG file: {path}")
    width, height = struct.unpack(">II", header[16:24])
    if width < 1600 or height < 900 or abs(width / height - 16.0 / 9.0) > 0.01:
        raise ValueError(f"Character turnaround must be at least 1600x900 at 16:9: {width}x{height}")
    return width, height


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    if manifest.get("schema") != "naiwa.p4.beta6.character_reference_manifest/1":
        raise ValueError("Unexpected Beta6 character reference manifest schema")
    source = (ROOT / manifest["source_path"]).resolve()
    if not source.is_file():
        raise FileNotFoundError(source)
    source_hash = sha256_file(source)
    if source_hash.lower() != manifest.get("sha256", "").lower():
        raise ValueError(f"Character reference hash mismatch: {source}")
    width, height = inspect_png(source)
    if [width, height] != manifest.get("dimensions_px"):
        raise ValueError("Character reference dimensions changed since manifest creation")

    asset_path = manifest["ue_texture_asset"]
    asset_name = asset_path.rsplit("/", 1)[1]
    texture = unreal.load_asset(asset_path)
    imported_now = False
    if not texture:
        unreal.EditorAssetLibrary.make_directory(CONTENT_ROOT)
        task = unreal.AssetImportTask()
        task.set_editor_properties({
            "filename": str(source),
            "destination_path": CONTENT_ROOT,
            "destination_name": asset_name,
            "replace_existing": False,
            "automated": True,
            "save": False,
        })
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(asset_path)
        imported_now = True
    if not texture or not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Could not import character reference texture: {asset_path}")
    texture.set_editor_properties(IMPORT_SETTINGS)
    for key, expected in IMPORT_SETTINGS.items():
        if texture.get_editor_property(key) != expected:
            raise RuntimeError(f"Texture setting readback failed for {asset_path}: {key}")
    if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save character reference texture: {asset_path}")

    approved_date = manifest.get("owner_approved_date")
    if approved_date:
        manifest["status"] = "owner_approved_turnaround_imported_static_reference"
    else:
        manifest["status"] = "turnaround_imported_pending_owner_review"
    write_json(MANIFEST_PATH, manifest)
    report.update({
        "status": (
            "reference_texture_imported_owner_approved_static_reference"
            if approved_date else "reference_texture_imported_pending_owner_review"
        ),
        "asset_id": manifest["asset_id"],
        "source_path": manifest["source_path"],
        "source_sha256": source_hash,
        "dimensions_px": [width, height],
        "texture_asset": asset_path,
        "imported_now": imported_now,
        "texture_settings": "sRGB, TC_Default, no alpha, no mips, UI group, non-streaming",
        "review_gate": manifest["review_gate"],
    })
    if approved_date:
        report["owner_approved_date"] = approved_date
    write_json(REPORT_PATH, report)


try:
    main()
except Exception as exc:
    report["status"] = "failed"
    report["error"] = str(exc)
    report["traceback"] = traceback.format_exc()
    write_json(REPORT_PATH, report)
    raise
