"""Prepare the approved P4 A1 art for technical handoff.

The accepted source sheets and extracted Frames/ PNGs are treated as immutable.
This script writes conservative alpha-speck-cleaned copies, action atlases with
transparent gutters, and a separate UE import manifest under RuntimeReady/.
"""

from __future__ import annotations

import hashlib
import html
import json
from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
ASSET_ROOT = ROOT / "UE5/SourceAssets/P4/Characters/Alpha/A1"
SOURCE_FRAMES = ASSET_ROOT / "Frames"
RUNTIME_ROOT = ASSET_ROOT / "RuntimeReady"
CLEAN_FRAMES = RUNTIME_ROOT / "FramesClean"
ATLAS_DIR = RUNTIME_ROOT / "Atlases"
SOURCE_MANIFEST_PATH = ROOT / "UE5/SourceAssets/P4/Manifests/P4_A1_Animation_manifest.json"
IMPORT_MANIFEST_PATH = ROOT / "UE5/SourceAssets/P4/Manifests/P4_A1_UE_Import_manifest.json"
REPORT_PATH = ROOT / "UE5/Records/p4_alpha/evidence/W2/P4_A1_AtlasPrep_report.json"
REVIEW_PATH = ROOT / "UE5/Records/p4_alpha/evidence/W2/P4_A1_Atlas_TechnicalReview.html"

VIEWS = ("front", "back", "side_right")
CELL_SIZE = (724, 724)
OUTER_PADDING = 8
GUTTER = 16
MIN_REMOTE_COMPONENT_PIXELS = 16
MIN_COMPONENT_DISTANCE_PX = 24
ALPHA_VISIBLE_THRESHOLD = 16


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def connected_runs(mask: np.ndarray) -> tuple[list[tuple[int, int, int, int]], dict[int, list[int]]]:
    """Label 8-connected alpha components by joining horizontal runs."""
    parents: list[int] = []

    def find(node: int) -> int:
        while parents[node] != node:
            parents[node] = parents[parents[node]]
            node = parents[node]
        return node

    def union(first: int, second: int) -> None:
        first_root = find(first)
        second_root = find(second)
        if first_root != second_root:
            parents[second_root] = first_root

    runs: list[tuple[int, int, int, int]] = []
    previous: list[tuple[int, int, int]] = []
    for y, row in enumerate(mask):
        changes = np.diff(np.pad(row.astype(np.int8), (1, 1)))
        starts = np.flatnonzero(changes == 1).tolist()
        ends = np.flatnonzero(changes == -1).tolist()
        current: list[tuple[int, int, int]] = []
        cursor = 0
        for start, end in zip(starts, ends):
            node = len(parents)
            parents.append(node)
            runs.append((start, end, y, node))
            current.append((start, end, node))
            while cursor < len(previous) and previous[cursor][1] < start - 1:
                cursor += 1
            scan = cursor
            while scan < len(previous) and previous[scan][0] <= end:
                if previous[scan][1] >= start - 1:
                    union(node, previous[scan][2])
                scan += 1
        previous = current

    # Resolve roots after all adjacent rows have been joined.
    resolved = [(start, end, y, find(node)) for start, end, y, node in runs]
    stats: dict[int, list[int]] = {}
    for start, end, y, root in resolved:
        if root not in stats:
            stats[root] = [0, start, y, end, y]
        component = stats[root]
        component[0] += end - start
        component[1] = min(component[1], start)
        component[2] = min(component[2], y)
        component[3] = max(component[3], end)
        component[4] = max(component[4], y)
    return resolved, stats


def clean_frame(source_path: Path, output_path: Path) -> dict:
    """Remove only tiny alpha components well outside the main character."""
    with Image.open(source_path) as opened:
        image = opened.convert("RGBA")
    rgba = np.array(image, dtype=np.uint8)
    alpha = rgba[:, :, 3]
    resolved_runs, components = connected_runs(alpha > 0)
    if not components:
        raise ValueError(f"No visible pixels in {source_path}")

    main_root = max(components, key=lambda root: components[root][0])
    main = components[main_root]
    discarded: set[int] = set()
    for root, component in components.items():
        if root == main_root or component[0] > MIN_REMOTE_COMPONENT_PIXELS:
            continue
        dx = max(main[1] - component[3], component[1] - main[3], 0)
        dy = max(main[2] - component[4], component[2] - main[4], 0)
        if dx * dx + dy * dy > MIN_COMPONENT_DISTANCE_PX**2:
            discarded.add(root)

    removed_pixels = 0
    if discarded:
        for start, end, y, root in resolved_runs:
            if root in discarded:
                rgba[y, start:end, 3] = 0
                removed_pixels += end - start

    output_path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(rgba, "RGBA").save(output_path, format="PNG", optimize=True)

    # Use the main opaque character component for an anchor estimate so a stray
    # opaque pixel cannot move the foot line. The original frame stays intact.
    primary = np.zeros(alpha.shape, dtype=bool)
    for start, end, y, root in resolved_runs:
        if root == main_root:
            primary[y, start:end] = True
    opaque = primary & (alpha >= 128)
    opaque_rows = np.flatnonzero(opaque.any(axis=1))
    if len(opaque_rows) == 0:
        raise ValueError(f"No opaque character pixels in {source_path}")
    foot_y = int(opaque_rows[-1])
    y0 = max(0, foot_y - 15)
    pixels = rgba[y0 : foot_y + 1]
    brown = (
        primary[y0 : foot_y + 1]
        & (alpha[y0 : foot_y + 1] >= 128)
        & (pixels[:, :, 0] < 155)
        & (pixels[:, :, 1] < 125)
        & (pixels[:, :, 2] < 105)
    )
    brown_x = np.flatnonzero(brown.any(axis=0))
    if brown_x.size:
        counts = brown.sum(axis=0)
        foot_x = int(round(float(np.average(np.arange(counts.size), weights=counts))))
    else:
        foot_x = int(round((main[1] + main[3] - 1) / 2))

    alpha16 = np.where(rgba[:, :, 3] >= ALPHA_VISIBLE_THRESHOLD, 255, 0).astype(np.uint8)
    visible_box = Image.fromarray(alpha16, "L").getbbox()
    return {
        "original_file": str(source_path.relative_to(ASSET_ROOT)).replace("\\", "/"),
        "clean_file": str(output_path.relative_to(ASSET_ROOT)).replace("\\", "/"),
        "original_sha256": sha256(source_path),
        "clean_sha256": sha256(output_path),
        "source_size_px": list(image.size),
        "component_count_alpha_gt_0": len(components),
        "removed_component_count": len(discarded),
        "removed_alpha_pixels": removed_pixels,
        "largest_component_pixels": main[0],
        "visible_rect_alpha16_px": list(visible_box) if visible_box else None,
        "foot_anchor_px_estimate": [foot_x, foot_y],
    }


def atlas_for_action(action: dict, frames_by_id: dict[str, dict], records: list[dict]) -> dict:
    pose_order = action["poses"]
    atlas_path = ATLAS_DIR / f"P4_A1_{action['id']}_atlas.png"
    atlas_relative_path = str(atlas_path.relative_to(ASSET_ROOT)).replace("\\", "/")
    width = 2 * OUTER_PADDING + len(VIEWS) * CELL_SIZE[0] + (len(VIEWS) - 1) * GUTTER
    height = 2 * OUTER_PADDING + len(pose_order) * CELL_SIZE[1] + (len(pose_order) - 1) * GUTTER
    atlas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    rects = []

    for row, pose in enumerate(pose_order):
        for col, view in enumerate(VIEWS):
            candidates = [
                frame for frame in frames_by_id.values()
                if frame["action"] == action["id"] and frame["pose"] == pose and frame["view"] == view
            ]
            if len(candidates) != 1:
                raise ValueError(f"Expected one frame for {action['id']}/{pose}/{view}, found {len(candidates)}")
            frame = candidates[0]
            record = next(item for item in records if item["id"] == frame["id"])
            clean_path = ASSET_ROOT / record["clean_file"]
            with Image.open(clean_path) as opened:
                cell = opened.convert("RGBA")
            if cell.size != CELL_SIZE:
                raise ValueError(f"Unexpected frame size for {clean_path}: {cell.size}")
            x = OUTER_PADDING + col * (CELL_SIZE[0] + GUTTER)
            y = OUTER_PADDING + row * (CELL_SIZE[1] + GUTTER)
            atlas.alpha_composite(cell, (x, y))
            frame["atlas_file"] = atlas_relative_path
            frame["atlas_cell_rect_px"] = [x - GUTTER // 2 if col else 0, y - GUTTER // 2 if row else 0,
                                           x + CELL_SIZE[0] + GUTTER // 2,
                                           y + CELL_SIZE[1] + GUTTER // 2]
            frame["atlas_sprite_rect_px"] = [x, y, x + CELL_SIZE[0], y + CELL_SIZE[1]]
            frame["atlas_grid"] = {"row": row, "column": col}
            rects.append(frame["id"])

    atlas_path.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(atlas_path, format="PNG", optimize=True)
    return {
        "action": action["id"],
        "name_zh": action["name_zh"],
        "file": str(atlas_path.relative_to(ASSET_ROOT)).replace("\\", "/"),
        "size_px": list(atlas.size),
        "row_order": pose_order,
        "column_order": list(VIEWS),
        "frame_ids": rects,
        "sha256": sha256(atlas_path),
    }


def render_review_html(accepted_frames: list[dict], records: list[dict], atlases: list[dict]) -> str:
    record_by_id = {item["id"]: item for item in records}
    atlas_sections = []
    for atlas in atlases:
        rel = "../../../../SourceAssets/P4/Characters/Alpha/A1/" + atlas["file"]
        atlas_sections.append(
            f'<section><h2>{html.escape(atlas["name_zh"])} · {html.escape(atlas["action"])}</h2>'
            f'<p>{atlas["size_px"][0]} × {atlas["size_px"][1]} px；列：正面 / 背面 / 右侧面；行：'
            f'{" / ".join(html.escape(p) for p in atlas["row_order"])}</p>'
            f'<a href="{rel}"><img class="atlas" src="{rel}" alt="{atlas["action"]} atlas"></a></section>'
        )

    rows = []
    for frame in accepted_frames:
        rec = record_by_id[frame["id"]]
        original = "../../../../SourceAssets/P4/Characters/Alpha/A1/" + frame["file"]
        clean = "../../../../SourceAssets/P4/Characters/Alpha/A1/" + rec["clean_file"]
        rows.append(
            f'<tr><th>{html.escape(frame["id"])}</th>'
            f'<td><img class="cell" src="{original}" alt="accepted original"></td>'
            f'<td><img class="cell" src="{clean}" alt="cleaned runtime copy"></td>'
            f'<td>{rec["removed_alpha_pixels"]} alpha pixels removed; foot anchor estimate '
            f'{rec["foot_anchor_px_estimate"]}</td></tr>'
        )

    return """<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>P4 A1 图集与 UE 交接复核</title><style>
*{box-sizing:border-box}body{margin:0;padding:24px;background:#11151a;color:#e9edf1;font:14px/1.5 "Segoe UI","Microsoft YaHei",sans-serif}
h1{margin:0 0 8px}p{color:#bac4ce}.status{display:inline-block;padding:5px 9px;border:1px solid #bc9854;border-radius:4px;color:#f3d89f;background:#3b3020}
section{margin:22px 0;padding:16px;background:#1c2229;border:1px solid #303a44;border-radius:8px}h2{margin:0 0 8px;font-size:18px}.atlas{display:block;max-width:min(100%,1200px);max-height:560px;object-fit:contain;object-position:left top;background-color:#e7e8e9;background-image:linear-gradient(45deg,#c8cbd0 25%,transparent 25%),linear-gradient(-45deg,#c8cbd0 25%,transparent 25%),linear-gradient(45deg,transparent 75%,#c8cbd0 75%),linear-gradient(-45deg,transparent 75%,#c8cbd0 75%);background-size:20px 20px;background-position:0 0,0 10px,10px -10px,-10px 0}
table{width:100%;border-collapse:collapse;margin-top:24px}th,td{border-bottom:1px solid #34404a;padding:8px;text-align:left;vertical-align:middle}.cell{width:112px;height:112px;object-fit:contain;background-color:#e7e8e9;background-image:linear-gradient(45deg,#c8cbd0 25%,transparent 25%),linear-gradient(-45deg,#c8cbd0 25%,transparent 25%),linear-gradient(45deg,transparent 75%,#c8cbd0 75%),linear-gradient(-45deg,transparent 75%,#c8cbd0 75%);background-size:16px 16px;background-position:0 0,0 8px,8px -8px,-8px 0}
</style></head><body>
<span class="status">A1 视觉验收已通过 · 技术图集候选，UE 导入与画面校准待做</span>
<h1>P4 A1 动作图集交接复核</h1>
<p>原始三视图条带和已验收的 Frames/ PNG 均保持不变。RuntimeReady/FramesClean/ 仅移除满足“连通块 ≤16 像素且离主体轮廓超过 24 像素”的远端透明孤点；动作格不缩放、不裁切、不平移。图集四周留 8 px，格间留 16 px。此页用于核对原图与清理副本及查看图集；不代表 UE 导入验收。</p>
""" + "\n".join(atlas_sections) + """
<h2>原始帧与导入副本逐帧对照</h2><table><thead><tr><th>Frame ID</th><th>已验收原帧</th><th>清理副本</th><th>处理摘要</th></tr></thead><tbody>
""" + "\n".join(rows) + "\n</tbody></table></body></html>"


def main() -> None:
    with SOURCE_MANIFEST_PATH.open("r", encoding="utf-8") as stream:
        source_manifest = json.load(stream)
    accepted_frames = source_manifest["frames"]
    if len(accepted_frames) != 33:
        raise ValueError(f"Expected 33 approved frame cells; found {len(accepted_frames)}")

    clean_records = []
    for frame in accepted_frames:
        source_path = ASSET_ROOT / frame["file"]
        output_path = CLEAN_FRAMES / frame["action"] / Path(frame["file"]).name
        clean_records.append(clean_frame(source_path, output_path))
        clean_records[-1]["id"] = frame["id"]

    frames_by_id = {frame["id"]: dict(frame) for frame in accepted_frames}
    record_by_id = {record["id"]: record for record in clean_records}
    for frame in frames_by_id.values():
        frame["foot_anchor_px_estimate"] = record_by_id[frame["id"]]["foot_anchor_px_estimate"]
        frame["clean_file"] = record_by_id[frame["id"]]["clean_file"]

    atlas_records = [atlas_for_action(action, frames_by_id, clean_records) for action in source_manifest["actions"]]
    total_removed = sum(record["removed_alpha_pixels"] for record in clean_records)
    report = {
        "status": "prepared_pending_owner_review_ue_import_and_visual_calibration",
        "date": "2026-10-28",
        "source_visual_acceptance": source_manifest["generation"]["owner_visual_acceptance"],
        "source_manifest": str(SOURCE_MANIFEST_PATH.relative_to(ROOT)).replace("\\", "/"),
        "source_cells": len(accepted_frames),
        "cleaned_cells": len(clean_records),
        "action_atlases": len(atlas_records),
        "alpha_cleanup_rule": {
            "connectivity": 8,
            "alpha_mask": "alpha > 0",
            "remove_only_when": f"component area <= {MIN_REMOTE_COMPONENT_PIXELS} pixels and Euclidean distance from largest component bbox > {MIN_COMPONENT_DISTANCE_PX} px",
            "source_files_modified": False,
            "total_alpha_pixels_removed_from_derivatives": total_removed,
        },
        "atlas_layout": {
            "cell_size_px": list(CELL_SIZE),
            "outer_padding_px": OUTER_PADDING,
            "gutter_px": GUTTER,
            "rows": "pose order from source manifest",
            "columns": list(VIEWS),
            "mip_bleed_guard": "transparent gutters; still validate UE sampler/UV behavior in engine",
        },
        "texture_import": {
            "color_space": "sRGB",
            "alpha": "straight alpha retained",
            "texture_group": "Character",
            "mip_generation": "Sharpen0",
            "first_runtime_import_recommendation": "Use cleaned per-frame PNGs as independent Texture2D assets until atlas UV sampling is visually confirmed; the prior W1 atlas material needed this safe path.",
        },
        "foot_anchor_method": "Bottommost alpha>=128 pixel row within the largest alpha-connected character component; X is weighted center of brown opaque pixels in the lowest 16 rows.",
        "frames": [
            {
                **{key: frame[key] for key in ("id", "action", "pose", "view", "file")},
                "clean_file": record_by_id[frame["id"]]["clean_file"],
                "original_sha256": record_by_id[frame["id"]]["original_sha256"],
                "clean_sha256": record_by_id[frame["id"]]["clean_sha256"],
                "foot_anchor_px_estimate": record_by_id[frame["id"]]["foot_anchor_px_estimate"],
                "visible_rect_alpha16_px": record_by_id[frame["id"]]["visible_rect_alpha16_px"],
                "removed_component_count": record_by_id[frame["id"]]["removed_component_count"],
                "removed_alpha_pixels": record_by_id[frame["id"]]["removed_alpha_pixels"],
                "atlas_file": frame.get("atlas_file"),
                "atlas_cell_rect_px": frame.get("atlas_cell_rect_px"),
                "atlas_sprite_rect_px": frame.get("atlas_sprite_rect_px"),
                "atlas_grid": frame.get("atlas_grid"),
            }
            for frame in frames_by_id.values()
        ],
        "atlases": atlas_records,
        "evidence": str(REVIEW_PATH.relative_to(ROOT)).replace("\\", "/"),
    }
    # Ensure atlas fields are present in the output mapping after atlas creation.
    for item in report["frames"]:
        source_frame = frames_by_id[item["id"]]
        for field in ("atlas_file", "atlas_cell_rect_px", "atlas_sprite_rect_px", "atlas_grid"):
            item[field] = source_frame[field]
    IMPORT_MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    IMPORT_MANIFEST_PATH.write_text(json.dumps({"schema": "naiwa.p4.alpha_ue_import/1", **report}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    REVIEW_PATH.parent.mkdir(parents=True, exist_ok=True)
    REVIEW_PATH.write_text(render_review_html(accepted_frames, clean_records, atlas_records), encoding="utf-8")

    print(f"approved_cells={len(accepted_frames)}")
    print(f"cleaned_cells={len(clean_records)}")
    print(f"action_atlases={len(atlas_records)}")
    print(f"removed_alpha_pixels={total_removed}")
    print(f"import_manifest={IMPORT_MANIFEST_PATH.relative_to(ROOT)}")
    print(f"review={REVIEW_PATH.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
