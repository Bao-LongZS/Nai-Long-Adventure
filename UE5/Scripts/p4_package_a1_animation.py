"""Package the P4 A1 generated source strips into lossless pose-cell PNGs.

The source strips are preserved. This script performs exact, non-retouching
three-column extraction and writes the frame manifest and owner-review HTML.
"""

from __future__ import annotations

import hashlib
import json
from datetime import date
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = ROOT / "UE5/SourceAssets/P4/Characters/Alpha/A1/SourceSheets"
FRAME_DIR = ROOT / "UE5/SourceAssets/P4/Characters/Alpha/A1/Frames"
MANIFEST_PATH = ROOT / "UE5/SourceAssets/P4/Manifests/P4_A1_Animation_manifest.json"
REVIEW_PATH = ROOT / "UE5/Records/p4_alpha/evidence/W2/P4_A1_Animation_review.html"

VIEWS = ("front", "back", "side_right")
CELL_SIZE = (724, 724)
ALPHA_VISIBLE_THRESHOLD = 16

POSES = (
    ("idle", "neutral", "P4_A1_Idle_source_3view.png"),
    ("move", "step_a", "P4_A1_Move_StepA_source_3view.png"),
    ("move", "step_b", "P4_A1_Move_StepB_source_3view.png"),
    ("attack", "windup", "P4_A1_Attack_Windup_source_3view.png"),
    ("attack", "contact", "P4_A1_Attack_Contact_source_3view.png"),
    ("dash", "launch", "P4_A1_Dash_Launch_source_3view.png"),
    ("dash", "stride", "P4_A1_Dash_Stride_source_3view.png"),
    ("defend", "guard", "P4_A1_Defend_source_3view.png"),
    ("hit", "recoil", "P4_A1_Hit_source_3view.png"),
    ("laugh", "open", "P4_A1_Laugh_OpenBelly_source_3view.png"),
    ("laugh", "recover", "P4_A1_Laugh_Recover_source_3view.png"),
)

ACTION_NAMES = {
    "idle": "待机",
    "move": "移动",
    "attack": "普攻",
    "dash": "冲撞",
    "defend": "防御",
    "hit": "受击",
    "laugh": "大笑",
}
POSE_NAMES = {
    "neutral": "稳定站姿",
    "step_a": "跨步 A",
    "step_b": "跨步 B",
    "windup": "起手蓄势",
    "contact": "出拳接触",
    "launch": "冲撞起步",
    "stride": "冲撞跨步",
    "guard": "防御架势",
    "recoil": "受击后仰",
    "open": "开怀大笑（抱肚）",
    "recover": "大笑回收",
}
VIEW_NAMES = {"front": "正面", "back": "背面", "side_right": "右侧面源图"}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def bounds_for_alpha(image: Image.Image, threshold: int) -> list[int] | None:
    alpha = image.getchannel("A")
    mask = alpha.point(lambda value: 255 if value >= threshold else 0)
    bounds = mask.getbbox()
    return list(bounds) if bounds else None


def foot_anchor(image: Image.Image) -> list[int] | None:
    """Estimate the sole anchor without altering the source pixels."""
    pixels = image.load()
    width, height = image.size
    floor_y = None
    for y in range(height - 1, -1, -1):
        if any(pixels[x, y][3] >= 128 for x in range(width)):
            floor_y = y
            break
    if floor_y is None:
        return None

    # Brown foot pixels near the lowest opaque row give a more useful X anchor
    # than the full character center, especially for asymmetric poses.
    brown_xs: list[int] = []
    for y in range(max(0, floor_y - 15), floor_y + 1):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            if alpha >= 128 and red < 155 and green < 125 and blue < 105:
                brown_xs.append(x)
    x_anchor = round(sum(brown_xs) / len(brown_xs)) if brown_xs else width // 2
    return [x_anchor, floor_y]


def render_review_html(frames: list[dict]) -> str:
    rows: list[str] = []
    pose_groups: dict[tuple[str, str], list[dict]] = {}
    for frame in frames:
        pose_groups.setdefault((frame["action"], frame["pose"]), []).append(frame)

    for (action, pose), group in pose_groups.items():
        cells = []
        for frame in group:
            rel = "../../../../SourceAssets/P4/Characters/Alpha/A1/" + frame["file"]
            cells.append(
                f'<td><div class="stage"><img src="{rel}" alt="{frame["id"]}"></div>'
                f'<div class="caption">{VIEW_NAMES[frame["view"]]} · '
                f'<a href="{rel}">{frame["file"]}</a></div></td>'
            )
        rows.append(
            f'<tr><th><strong>{ACTION_NAMES[action]}</strong><br>{POSE_NAMES[pose]}</th>'
            + "".join(cells)
            + "</tr>"
        )

    return """<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>P4 A1 动作帧组 · 视觉验收通过</title>
<style>
*{box-sizing:border-box}body{margin:0;padding:24px;background:#11151a;color:#e9edf1;font:14px/1.5 "Segoe UI","Microsoft YaHei",sans-serif}
h1{margin:0 0 8px;font-size:24px}.note{margin:0 0 20px;color:#b8c1cc;max-width:1050px}
.status{display:inline-block;padding:5px 9px;margin-bottom:16px;border:1px solid #bc9854;border-radius:4px;color:#f3d89f;background:#3b3020}
table{width:100%;border-collapse:separate;border-spacing:8px;table-layout:fixed}th{width:130px;text-align:left;vertical-align:middle;color:#f1dba8}
td{vertical-align:top;background:#1c2229;border:1px solid #303a44;border-radius:6px;padding:8px}.stage{height:210px;display:grid;place-items:center;border-radius:4px;background-color:#e7e8e9;background-image:linear-gradient(45deg,#c8cbd0 25%,transparent 25%),linear-gradient(-45deg,#c8cbd0 25%,transparent 25%),linear-gradient(45deg,transparent 75%,#c8cbd0 75%),linear-gradient(-45deg,transparent 75%,#c8cbd0 75%);background-size:20px 20px;background-position:0 0,0 10px,10px -10px,-10px 0}
img{max-width:100%;max-height:100%;object-fit:contain}.caption{padding-top:7px;color:#bdc7d1;font-size:12px;overflow-wrap:anywhere}.caption a{color:#94c8ff}
@media(max-width:820px){body{padding:10px}table{border-spacing:4px}th{width:84px;font-size:12px}.stage{height:140px}.caption{font-size:10px}}
</style></head><body>
<div class="status">视觉验收 PASS（2026-10-28）· UE 技术整合待验</div>
<h1>P4 A1 奶蛙动作帧组</h1>
<p class="note">项目负责人已通过这 33 帧的美术视觉方案，包括替换后的收臂抱肚大笑姿势；力量感稿和双臂完全张开的大笑稿已排除。每行一个姿势，列为正面、背面、右侧面。侧面运行时水平镜像复用为左侧。UE 导入、锚点校准、边缘清理和固定机位检查仍待工程交接。</p>
<table><thead><tr><th>动作 / 姿势</th><th>正面</th><th>背面</th><th>右侧面源图</th></tr></thead><tbody>
""" + "\n".join(rows) + "\n</tbody></table></body></html>"


def main() -> None:
    FRAME_DIR.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    REVIEW_PATH.parent.mkdir(parents=True, exist_ok=True)

    frames: list[dict] = []
    source_sheets: list[dict] = []
    expected_width = CELL_SIZE[0] * len(VIEWS)
    expected_height = CELL_SIZE[1]

    for action, pose, source_name in POSES:
        source_path = SOURCE_DIR / source_name
        if not source_path.is_file():
            raise FileNotFoundError(source_path)
        source_hash = sha256(source_path)
        with Image.open(source_path) as opened:
            if opened.mode != "RGBA":
                raise ValueError(f"{source_name}: expected RGBA, got {opened.mode}")
            if opened.size != (expected_width, expected_height):
                raise ValueError(
                    f"{source_name}: expected {(expected_width, expected_height)}, got {opened.size}"
                )
            sheet = opened.copy()
            alpha = sheet.getchannel("A")
            source_sheets.append(
                {
                    "file": f"SourceSheets/{source_name}",
                    "sha256": source_hash,
                    "size_px": list(sheet.size),
                    "mode": sheet.mode,
                    "alpha_extrema": list(alpha.getextrema()),
                    "cell_size_px": list(CELL_SIZE),
                    "column_order": list(VIEWS),
                    "note": "Original generator output retained unchanged; inspect stray edge pixels at in-game scale.",
                }
            )

            for view_index, view in enumerate(VIEWS):
                x0 = view_index * CELL_SIZE[0]
                x1 = x0 + CELL_SIZE[0]
                cell = sheet.crop((x0, 0, x1, CELL_SIZE[1]))
                frame_id = f"{action}_{pose}_{view}"
                output_name = f"P4_A1_{action}_{pose}_{view}.png"
                output_path = FRAME_DIR / action / output_name
                output_path.parent.mkdir(parents=True, exist_ok=True)
                cell.save(output_path, format="PNG", optimize=True)
                frames.append(
                    {
                        "id": frame_id,
                        "action": action,
                        "action_name_zh": ACTION_NAMES[action],
                        "pose": pose,
                        "pose_name_zh": POSE_NAMES[pose],
                        "view": view,
                        "view_name_zh": VIEW_NAMES[view],
                        "file": f"Frames/{action}/{output_name}",
                        "size_px": list(cell.size),
                        "source_sheet": f"SourceSheets/{source_name}",
                        "source_rect_px": [x0, 0, x1, CELL_SIZE[1]],
                        "cell_rect_px": [0, 0, CELL_SIZE[0], CELL_SIZE[1]],
                        "visible_rect_alpha16_px": bounds_for_alpha(cell, ALPHA_VISIBLE_THRESHOLD),
                        "foot_anchor_px": foot_anchor(cell),
                        "sha256": sha256(output_path),
                    }
                )

    if len(frames) != 33:
        raise AssertionError(f"Expected 33 pose cells, packaged {len(frames)}")

    actions = []
    for action in ACTION_NAMES:
        action_frames = [frame for frame in frames if frame["action"] == action]
        poses = list(dict.fromkeys(frame["pose"] for frame in action_frames))
        actions.append(
            {
                "id": action,
                "name_zh": ACTION_NAMES[action],
                "poses": poses,
                "frame_ids": [frame["id"] for frame in action_frames],
                "source_view_count": 3,
            }
        )

    manifest = {
        "schema": "naiwa.p4.alpha_animation/1",
        "asset_id": "P4_ALPHA_A1_FROG_ANIMATION",
        "status": "visual_acceptance_passed_pending_technical_integration",
        "direction": {
            "selected": "rounded cute first draft approved by project owner on 2026-10-28",
            "excluded": "the stronger/muscular alternate draft is excluded and not part of this package",
        },
        "rejected_versions": [
            {
                "id": "laugh_open_arms_wide",
                "disposition": "excluded at owner request; rejected source strip and frame exports were removed from the A1 package",
                "replacement": "laugh_open_belly_hand",
            }
        ],
        "generation": {
            "tool": "built-in image_gen",
            "format": "PNG RGBA",
            "no_pixel_retouch": True,
            "frames_are_lossless_cells_from_original_three_view_strips": True,
            "owner_visual_acceptance": "passed by project owner on 2026-10-28 for all 33 frame cells, including the replacement belly-laugh pose; technical validation not included",
            "acceptance_record": "UE5/Records/p4_alpha/evidence/W2/P4_A1_视觉验收记录.md",
        },
        "view_mapping": {
            "columns": list(VIEWS),
            "left_side": "horizontal mirror of side_right; no separate pose cell",
            "diagonal_views": "not included",
        },
        "import_recommendation": {
            "color_space": "sRGB",
            "alpha": "straight alpha retained",
            "ue_texture_group": "Character",
            "ue_mip_generation": "Sharpen0",
            "ue_compression": "Default",
            "runtime_alignment": "align each pose by foot_anchor_px; offsets are metadata only and require in-engine calibration",
        },
        "qa": {
            "source_sheet_count": len(source_sheets),
            "pose_cell_count": len(frames),
            "all_source_sheets_rgba_2172x724": True,
            "all_cells_724x724": True,
            "edge_specks": "unretouched generator output may include isolated low-alpha pixels; inspect at 720p/1080p before production import",
            "foot_anchor": "algorithmic estimate from opaque brown pixels near lowest opaque row; not approved against a rendered UE foot baseline",
            "ue_import_or_preview": "not run",
            "owner_visual_review": "passed by project owner on 2026-10-28",
            "w1_gate": "still open; A1 visual acceptance does not close runtime, performance, or DataTable checks",
        },
        "source_sheets": source_sheets,
        "actions": actions,
        "frames": frames,
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    REVIEW_PATH.write_text(render_review_html(frames), encoding="utf-8")

    print(f"frames={len(frames)}")
    print(f"source_sheets={len(source_sheets)}")
    print(f"manifest={MANIFEST_PATH.relative_to(ROOT)}")
    print(f"review={REVIEW_PATH.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
