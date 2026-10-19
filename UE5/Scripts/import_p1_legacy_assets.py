"""Import a small, pinned SFML first-level asset set into the isolated P1 tree.

Run with UnrealEditor-Cmd -run=PythonScript. Existing assets are verified and
left untouched; the source inventory is checked before any import starts.
"""

import hashlib
import json
import traceback
from datetime import datetime, timezone
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "UE5/NaiwaP0"
SOURCE_ROOT = ROOT / "UE5/SourceAssets/SFML_Level1"
MANIFEST = SOURCE_ROOT / "asset_manifest.json"
DEST_ROOT = "/Game/Naiwa/P1/Legacy"
OUTPUT = PROJECT / "Saved/NaiwaP1/legacy_asset_import.json"
OUTPUT.parent.mkdir(parents=True, exist_ok=True)

# Stable names avoid conflicts with other P1 or P0 imports. This selection is
# deliberately small; the complete 29-file source inventory stays preserved.
SELECTION = (
    ("wav", "Die.wav", "Audio", "SFX_P1_Die", "SoundWave"),
    ("wav", "Collection.wav", "Audio", "SFX_P1_Collection", "SoundWave"),
    ("wav", "Jump.wav", "Audio", "SFX_P1_Jump", "SoundWave"),
    ("png", "Spine.png", "Textures", "T_P1_Spine", "Texture2D"),
    ("png", "Tree.png", "Textures", "T_P1_Tree", "Texture2D"),
    ("png", "Leave_Left.png", "Textures", "T_P1_Leave_Left", "Texture2D"),
    ("png", "Leave_Right.png", "Textures", "T_P1_Leave_Right", "Texture2D"),
    ("png", "Cloud_Mass.png", "Textures", "T_P1_Cloud_Mass", "Texture2D"),
    ("png", "collection1.png", "Textures", "T_P1_Collection1", "Texture2D"),
)

P0_PROTECTED = (
    PROJECT / "Content/Naiwa/Maps/L_NaiwaGraybox.umap",
    PROJECT / "Content/Naiwa/Blueprints/BP_NaiwaCharacter.uasset",
    PROJECT / "Content/Naiwa/Blueprints/BP_NaiwaGameMode.uasset",
    PROJECT / "Content/Naiwa/Input/IMC_Naiwa.uasset",
)

report = {
    "schema": "NaiwaP1LegacyImportV1",
    "started_utc": datetime.now(timezone.utc).isoformat(),
    "engine": unreal.SystemLibrary.get_engine_version(),
    "manifest": str(MANIFEST),
    "destination_root": DEST_ROOT,
    "assets": [],
    "status": "started",
}


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def save_report():
    OUTPUT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


def protected_hashes():
    return {str(path.relative_to(PROJECT)): sha256_file(path) for path in P0_PROTECTED}


def asset_file(destination, name):
    relative = destination.removeprefix("/Game/")
    assert destination.startswith(DEST_ROOT + "/")
    return PROJECT / "Content" / relative / (name + ".uasset")


def source_provenance(asset, source):
    """Read UE import metadata; return the provenance paths without editing it."""
    filenames = []
    try:
        import_data = asset.get_editor_property("asset_import_data")
        if import_data:
            if hasattr(import_data, "extract_filenames"):
                filenames = list(import_data.extract_filenames())
            elif hasattr(import_data, "get_first_filename"):
                filenames = [import_data.get_first_filename()]
    except Exception as exc:
        return False, [], str(exc)
    desired = str(source.resolve()).replace("\\", "/").casefold()
    observed = [str(name).replace("\\", "/") for name in filenames if name]
    matched = any(
        (str(Path(name).resolve()).replace("\\", "/").casefold() == desired)
        if Path(name).is_absolute()
        else desired.endswith(name.casefold().lstrip("./"))
        for name in observed
    )
    return matched, observed, None


try:
    assert MANIFEST.is_file(), "Formal SFML source manifest is missing"
    project_dir = Path(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    ).resolve()
    assert project_dir == PROJECT.resolve(), "Refusing to import into another UE project"
    report["project"] = str(project_dir)
    inventory = json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
    assert inventory["count"] == 29 and len(inventory["assets"]) == 29
    by_name = {item["name"]: item for item in inventory["assets"]}
    assert len(by_name) == 29, "Duplicate formal source names"

    previous = {}
    if OUTPUT.is_file():
        try:
            old_report = json.loads(OUTPUT.read_text(encoding="utf-8"))
            if old_report.get("status") == "imported_or_verified":
                previous = {item["asset_path"]: item for item in old_report["assets"]}
        except (ValueError, KeyError):
            pass

    # Preflight every input and target before creating the first package.
    prepared = []
    for kind, name, folder, asset_name, class_name in SELECTION:
        entry = by_name[name]
        source = SOURCE_ROOT / kind / name
        assert entry["formal_source"] == str(source.relative_to(ROOT)).replace("\\", "/")
        assert entry["release_debug_hash_equal"] is True
        assert source.is_file(), "Source asset missing: " + str(source)
        assert source.stat().st_size == entry["bytes"], "Source size changed: " + name
        digest = sha256_file(source)
        assert digest == entry["sha256"], "Source SHA256 changed: " + name
        destination = DEST_ROOT + "/" + folder
        package = destination + "/" + asset_name
        disk_path = asset_file(destination, asset_name)
        existing = (
            unreal.load_asset(package)
            if unreal.EditorAssetLibrary.does_asset_exist(package)
            else None
        )
        assert not (disk_path.exists() and existing is None), "Unregistered target file: " + str(disk_path)
        if existing is not None:
            assert disk_path.is_file(), "Registered target has no saved file: " + package
            assert existing.get_class().get_name() == class_name, "Existing target has wrong class: " + package
            matched, filenames, issue = source_provenance(existing, source)
            earlier = previous.get(package + "." + asset_name, {})
            prior_match = (
                earlier.get("source_sha256") == digest
                and earlier.get("uasset_sha256") == sha256_file(disk_path)
            )
            assert matched or prior_match, "Existing target origin is unverified; refusing overwrite: " + package
        else:
            matched, filenames, issue = False, [], None
        prepared.append({
            "source": source,
            "source_sha256": digest,
            "source_bytes": source.stat().st_size,
            "destination": destination,
            "asset_name": asset_name,
            "asset_path": package + "." + asset_name,
            "asset_class": class_name,
            "uasset_file": disk_path,
            "existing": existing is not None,
            "source_metadata_paths": filenames,
            "source_metadata_error": issue,
        })

    report["p0_hashes_before"] = protected_hashes()
    for item in prepared:
        package = item["asset_path"].split(".")[0]
        if item["existing"]:
            action = "verified_existing_no_write"
        else:
            task = unreal.AssetImportTask()
            for key, value in (
                ("filename", str(item["source"])),
                ("destination_path", item["destination"]),
                ("destination_name", item["asset_name"]),
                ("automated", True),
                ("replace_existing", False),
                ("save", True),
            ):
                task.set_editor_property(key, value)
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            action = "imported_new"
        asset = unreal.load_asset(package)
        assert asset is not None, "Unreal import produced no asset: " + package
        assert asset.get_class().get_name() == item["asset_class"], "Wrong imported asset class: " + package
        assert item["uasset_file"].is_file(), "Imported asset was not saved: " + package
        matched, filenames, issue = source_provenance(asset, item["source"])
        if not item["existing"] and not matched:
            # The import task's filename and freshly created package establish
            # provenance for this run; the report hash authenticates reruns.
            report.setdefault("warnings", []).append(
                "Import metadata did not expose the source path: " + package
            )
        report["assets"].append({
            "source": str(item["source"].relative_to(ROOT)).replace("\\", "/"),
            "source_sha256": item["source_sha256"],
            "source_bytes": item["source_bytes"],
            "asset_path": item["asset_path"],
            "asset_class": item["asset_class"],
            "uasset_sha256": sha256_file(item["uasset_file"]),
            "source_metadata_paths": filenames,
            "source_metadata_matched": matched,
            "source_metadata_error": issue,
            "action": action,
        })

    report["p0_hashes_after"] = protected_hashes()
    assert report["p0_hashes_before"] == report["p0_hashes_after"], "A protected P0 asset changed"
    report["selected_count"] = len(SELECTION)
    report["new_count"] = sum(item["action"] == "imported_new" for item in report["assets"])
    report["existing_count"] = len(SELECTION) - report["new_count"]
    report["status"] = "imported_or_verified"
    report["finished_utc"] = datetime.now(timezone.utc).isoformat()
    save_report()
    unreal.log("NAIWA_P1_LEGACY_IMPORT_OK " + str(OUTPUT))
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    report["finished_utc"] = datetime.now(timezone.utc).isoformat()
    save_report()
    unreal.log_error("NAIWA_P1_LEGACY_IMPORT_FAILED " + report["error"])
    raise
