"""Apply a reversible, material-only P4 W1 soft-shadow strength adjustment.

This commandlet script loads and saves one P4-owned material. It never creates,
opens, or switches a level and does not run PIE/Standalone.
"""
from __future__ import annotations

import json
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
MATERIAL_PATH = "/Game/Naiwa/P4/Materials/W1/M_P4_W1_SoftShadow_RadialV6"
BACKUP_PATH = "/Game/Naiwa/P4/Materials/W1/M_P4_W1_SoftShadow_RadialV6_BeforeOpacityTune"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/W1/P4_W1_soft_shadow_tune_report_retry2.json"
EXPECTED_OLD_STRENGTH = 0.45
NEW_STRENGTH = 0.58

report = {
    "status": "running",
    "material": MATERIAL_PATH,
    "backup": BACKUP_PATH,
    "old_strength_expected": EXPECTED_OLD_STRENGTH,
    "new_strength": NEW_STRENGTH,
    "level_opened_or_changed": False,
}


def find_strength_constant(material):
    matches = []
    for expression in unreal.MaterialEditingLibrary.get_material_expressions(material):
        if not isinstance(expression, unreal.MaterialExpressionConstant):
            continue
        x, y = unreal.MaterialEditingLibrary.get_material_expression_node_position(expression)
        if x == 160 and y == 520:
            matches.append(expression)
    if len(matches) != 1:
        raise RuntimeError(f"Expected one shadow opacity constant at (160, 520), found {len(matches)}")
    return matches[0]


def read_strength(material) -> float:
    return float(find_strength_constant(material).get_editor_property("r"))


def main() -> None:
    material = unreal.load_asset(MATERIAL_PATH)
    if not material:
        raise FileNotFoundError(f"Missing P4 W1 soft-shadow material: {MATERIAL_PATH}")
    if material.get_editor_property("blend_mode") != unreal.BlendMode.BLEND_TRANSLUCENT:
        raise RuntimeError("Unexpected shadow material blend mode; refusing to edit")

    current_strength = read_strength(material)
    report["strength_before"] = current_strength
    if not unreal.EditorAssetLibrary.does_asset_exist(BACKUP_PATH):
        if not unreal.EditorAssetLibrary.duplicate_asset(MATERIAL_PATH, BACKUP_PATH):
            raise RuntimeError(f"Could not create rollback copy: {BACKUP_PATH}")
    backup = unreal.load_asset(BACKUP_PATH)
    if not backup:
        raise RuntimeError("Rollback copy could not be loaded")
    backup_strength = read_strength(backup)
    if abs(backup_strength - EXPECTED_OLD_STRENGTH) > 1e-6:
        if abs(backup_strength - NEW_STRENGTH) > 1e-6:
            raise RuntimeError(f"Unexpected rollback-copy strength: {backup_strength}")
        backup_expression = find_strength_constant(backup)
        backup_expression.set_editor_property("r", EXPECTED_OLD_STRENGTH)
        backup_errors = unreal.MaterialEditingLibrary.recompile_material(backup)
        if backup_errors:
            raise RuntimeError(f"Rollback material compile errors: {backup_errors}")
    if not unreal.EditorAssetLibrary.save_asset(BACKUP_PATH, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save rollback copy: {BACKUP_PATH}")
    backup = unreal.load_asset(BACKUP_PATH)
    backup_strength = read_strength(backup)
    if abs(backup_strength - EXPECTED_OLD_STRENGTH) > 1e-6:
        raise RuntimeError(f"Saved rollback copy does not read back as 0.45: {backup_strength}")

    if abs(current_strength - NEW_STRENGTH) <= 1e-6:
        report.update({
            "status": "already_tuned_backup_persisted",
            "strength_after": current_strength,
            "backup_strength": backup_strength,
            "backup_asset_persisted": True,
            "material_compile": "previously_passed",
        })
        return
    if abs(current_strength - EXPECTED_OLD_STRENGTH) > 1e-6:
        raise RuntimeError(f"Unexpected current opacity strength: {current_strength}")

    strength = find_strength_constant(material)
    strength.set_editor_property("r", NEW_STRENGTH)
    errors = unreal.MaterialEditingLibrary.recompile_material(material)
    if errors:
        raise RuntimeError(f"Soft-shadow material compile errors: {errors}")
    if not unreal.EditorAssetLibrary.save_asset(MATERIAL_PATH, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save tuned soft-shadow material: {MATERIAL_PATH}")

    actual_strength = read_strength(material)
    if abs(actual_strength - NEW_STRENGTH) > 1e-6:
        raise RuntimeError(f"Opacity strength readback mismatch: {actual_strength}")
    report.update({
        "status": "saved_pending_owner_visual_review",
        "strength_before": current_strength,
        "strength_after": actual_strength,
        "backup_strength": read_strength(backup),
        "backup_asset_persisted": True,
        "material_compile": "passed",
        "limitations": [
            "The shadow remains a soft radial ellipse approximation.",
            "Visual contact/readability and overlap ordering need owner review in the existing preview map.",
        ],
    })


try:
    main()
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    raise
finally:
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("NAIWA_P4_W1_SOFT_SHADOW_TUNE " + json.dumps(report, ensure_ascii=False))
