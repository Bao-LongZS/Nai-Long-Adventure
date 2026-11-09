#!/usr/bin/env python3
"""Build versioned P5 A4/A5 art candidates from generated source sheets.

This tool only reads the WIP source sheets under SourceAssets/P5 and writes
candidate PNG atlases, previews, OpenRaster layer files, and manifests. It does
not modify Unreal Content or any approved source art.
"""
from __future__ import annotations

import hashlib
import json
import zipfile
from collections import deque
from pathlib import Path
from xml.etree.ElementTree import Element, SubElement, tostring

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
CELL = 512
ATLAS_COLS = 4
ATLAS_ROWS = 6
BODY_BASELINE_Y = 470
BODY_SAFE_EDGE = 28

BODY_SOURCE = ROOT / "UE5/SourceAssets/P5/Characters/Source/A4_Animation_v03_WIP"
BODY_EXPORT = ROOT / "UE5/SourceAssets/P5/Characters/Exports"
BODY_EVIDENCE = ROOT / "UE5/Records/p5_p6_plan/evidence/P5_A0"
ENV_SOURCE = ROOT / "UE5/SourceAssets/P5/Environment/Source/A5_Mount_v02_WIP"
ENV_EXPORT = ROOT / "UE5/SourceAssets/P5/Environment/Exports"
MANIFESTS = ROOT / "UE5/SourceAssets/P5/Manifests"

SEQUENCES = [
    {"name": "Glide", "file": "C_Naiwa_Glide_v03_WIP.png", "cols": 2, "rows": 2,
     "frames": ["Glide_01", "Glide_02", "Glide_03", "Glide_04"], "duration": [100] * 4, "loop": True},
    {"name": "PreloadTakeoff", "file": "C_Naiwa_Takeoff_v02_WIP.png", "cols": 3, "rows": 1,
     "frames": ["Preload_01", "Preload_02", "Takeoff_Release"], "duration": [80, 80, 100], "loop": False},
    {"name": "AirTuckExtend", "file": "C_Naiwa_AirTuck_v02_WIP.png", "cols": 3, "rows": 1,
     "frames": ["AirTuck_01", "AirTuck_02", "AirExtend_01"], "duration": [90, 90, 110], "loop": False},
    {"name": "AirTurnBalance", "file": "C_Naiwa_AirTurn_v02_WIP.png", "cols": 3, "rows": 1,
     "frames": ["AirTurn_01", "AirTurn_02", "AirTurn_03"], "duration": [100] * 3, "loop": True},
    {"name": "LandRecover", "file": "C_Naiwa_Land_v02_WIP.png", "cols": 3, "rows": 1,
     "frames": ["Land_Contact", "Land_Compress", "Land_ResumeGlide"], "duration": [80, 100, 160], "loop": False},
    {"name": "FallRecovery", "file": "C_Naiwa_Fall_v02_WIP.png", "cols": 2, "rows": 2,
     "frames": ["Fall_01", "Fall_02", "Recovery_01", "Recovery_02"], "duration": [100, 120, 120, 180], "loop": False},
    {"name": "Ride", "file": "C_Naiwa_Ride_v02_WIP.png", "cols": 3, "rows": 1,
     "frames": ["Ride_01", "Ride_02", "Ride_03"], "duration": [120] * 3, "loop": True},
]

HAND_ANCHOR_CANDIDATES = {
    "Glide": [[0.22, 0.40], [0.77, 0.43]],
    "PreloadTakeoff": [[0.29, 0.57], [0.72, 0.56]],
    "AirTuckExtend": [[0.25, 0.47], [0.72, 0.48]],
    "AirTurnBalance": [[0.22, 0.42], [0.78, 0.43]],
    "LandRecover": [[0.23, 0.48], [0.77, 0.48]],
    "FallRecovery": [[0.30, 0.60], [0.70, 0.60]],
    "Ride": [[0.43, 0.57], [0.60, 0.57]],
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def largest_component_bbox(alpha: Image.Image) -> tuple[int, int, int, int] | None:
    """Find the dominant opaque silhouette at 1/4 resolution to remove stray marks."""
    threshold = alpha.point(lambda value: 255 if value >= 24 else 0)
    small_w = max(1, threshold.width // 4)
    small_h = max(1, threshold.height // 4)
    small = threshold.resize((small_w, small_h), Image.Resampling.NEAREST)
    mask = bytearray(1 if value else 0 for value in small.getdata())
    seen = bytearray(len(mask))
    largest: list[int] = []
    for start, occupied in enumerate(mask):
        if not occupied or seen[start]:
            continue
        queue = deque([start])
        seen[start] = 1
        component: list[int] = []
        while queue:
            index = queue.popleft()
            component.append(index)
            x = index % small_w
            y = index // small_w
            for neighbor in (index - 1 if x else -1, index + 1 if x + 1 < small_w else -1,
                             index - small_w if y else -1, index + small_w if y + 1 < small_h else -1):
                if neighbor >= 0 and mask[neighbor] and not seen[neighbor]:
                    seen[neighbor] = 1
                    queue.append(neighbor)
        if len(component) > len(largest):
            largest = component
    if not largest:
        return None
    xs = [index % small_w for index in largest]
    ys = [index // small_w for index in largest]
    left = max(0, min(xs) * 4 - 12)
    top = max(0, min(ys) * 4 - 12)
    right = min(alpha.width, (max(xs) + 1) * 4 + 12)
    bottom = min(alpha.height, (max(ys) + 1) * 4 + 12)
    return left, top, right, bottom


def clean_cell(sheet: Image.Image, col: int, row: int, cols: int, rows: int,
               remove_blue_reaction: bool = False) -> tuple[Image.Image, list[int]]:
    left = round(col * sheet.width / cols)
    right = round((col + 1) * sheet.width / cols)
    top = round(row * sheet.height / rows)
    bottom = round((row + 1) * sheet.height / rows)
    cell = sheet.crop((left, top, right, bottom)).convert("RGBA")
    alpha = cell.getchannel("A")
    bbox = largest_component_bbox(alpha)
    if bbox:
        cell = cell.crop(bbox)
        # Keep the generated anti-aliased edge; isolated blue impact strokes are
        # omitted from this mount-only WIP so FX can remain a separate layer.
        if remove_blue_reaction:
            pixels = cell.load()
            for y in range(cell.height):
                for x in range(cell.width):
                    r, g, b, a = pixels[x, y]
                    if a and b > 125 and b > g * 1.20 and r < 115:
                        pixels[x, y] = (r, g, b, 0)
        bbox2 = cell.getchannel("A").getbbox()
        if bbox2:
            cell = cell.crop(bbox2)
    return cell, [left, top, right - left, bottom - top]


def resize_straight_alpha(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    image = image.convert("RGBA")
    r, g, b, a = image.split()
    alpha = a
    channels = []
    for channel in (r, g, b):
        premultiplied = Image.new("L", image.size)
        premultiplied_data = [round(c * av / 255) for c, av in zip(channel.getdata(), alpha.getdata())]
        premultiplied.putdata(premultiplied_data)
        channels.append(premultiplied.resize(size, Image.Resampling.LANCZOS))
    new_alpha = alpha.resize(size, Image.Resampling.LANCZOS)
    restored = []
    for channel in channels:
        out = []
        for value, av in zip(channel.getdata(), new_alpha.getdata()):
            out.append(0 if av == 0 else min(255, round(value * 255 / av)))
        layer = Image.new("L", size)
        layer.putdata(out)
        restored.append(layer)
    return Image.merge("RGBA", (*restored, new_alpha))


def place_in_cell(image: Image.Image, cell: tuple[int, int], bottom_y: int | None) -> tuple[Image.Image, list[int]]:
    width, height = cell
    image = image.convert("RGBA")
    scale = min((width - BODY_SAFE_EDGE * 2) / max(1, image.width),
                (height - BODY_SAFE_EDGE * 2) / max(1, image.height))
    scaled_size = (max(1, round(image.width * scale)), max(1, round(image.height * scale)))
    resized = resize_straight_alpha(image, scaled_size)
    x = (width - resized.width) // 2
    y = (height - resized.height) // 2 if bottom_y is None else bottom_y - resized.height
    y = max(BODY_SAFE_EDGE, min(height - resized.height - BODY_SAFE_EDGE, y))
    canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    canvas.alpha_composite(resized, (x, y))
    return canvas, [x, y, resized.width, resized.height]


def make_ora(path: Path, width: int, height: int, layers: list[tuple[str, Image.Image, int, int]]) -> None:
    root = Element("image", {"version": "0.0.1", "w": str(width), "h": str(height), "name": path.stem})
    stack = SubElement(root, "stack")
    merged = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    encoded_layers: list[tuple[str, bytes, int, int]] = []
    for index, (name, image, x, y) in enumerate(layers):
        layer_path = f"data/layer{index:03d}.png"
        from io import BytesIO
        buffer = BytesIO()
        image.save(buffer, format="PNG")
        encoded_layers.append((layer_path, buffer.getvalue(), x, y))
        SubElement(stack, "layer", {"name": name, "src": layer_path, "x": str(x), "y": str(y),
                                     "opacity": "1.0", "visibility": "visible", "composite-op": "svg:src-over"})
        merged.alpha_composite(image, (x, y))
    from io import BytesIO
    merged_buffer = BytesIO()
    merged.save(merged_buffer, format="PNG")
    stack_xml = tostring(root, encoding="utf-8", xml_declaration=True)
    path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr("mimetype", "image/openraster", compress_type=zipfile.ZIP_STORED)
        archive.writestr("stack.xml", stack_xml, compress_type=zipfile.ZIP_DEFLATED)
        archive.writestr("mergedimage.png", merged_buffer.getvalue(), compress_type=zipfile.ZIP_DEFLATED)
        for layer_path, content, _, _ in encoded_layers:
            archive.writestr(layer_path, content, compress_type=zipfile.ZIP_DEFLATED)


def make_gif(frames: list[Image.Image], path: Path, duration_ms: list[int], loop: bool) -> None:
    preview_size = (256, 256)
    bg = Image.new("RGBA", preview_size, (225, 236, 245, 255))
    rendered = []
    for frame in frames:
        thumb = frame.copy()
        thumb.thumbnail((224, 224), Image.Resampling.LANCZOS)
        x = (256 - thumb.width) // 2
        y = (256 - thumb.height) // 2
        canvas = bg.copy()
        canvas.alpha_composite(thumb, (x, y))
        rendered.append(canvas.convert("RGB"))
    rendered[0].save(path, save_all=True, append_images=rendered[1:], duration=duration_ms,
                     loop=0 if loop else 1, disposal=2, optimize=False)


def write_body_assets() -> tuple[list[dict], list[Image.Image]]:
    BODY_EXPORT.mkdir(parents=True, exist_ok=True)
    BODY_EVIDENCE.mkdir(parents=True, exist_ok=True)
    atlas = Image.new("RGBA", (ATLAS_COLS * CELL, ATLAS_ROWS * CELL), (0, 0, 0, 0))
    all_frames: list[dict] = []
    frame_images: list[Image.Image] = []
    ora_layers: list[tuple[str, Image.Image, int, int]] = []
    for sequence in SEQUENCES:
        source_path = BODY_SOURCE / sequence["file"]
        sheet = Image.open(source_path).convert("RGBA")
        start_index = len(all_frames)
        segment_frames: list[Image.Image] = []
        row_offset = sequence.get("source_row", 0)
        for local_index, frame_name in enumerate(sequence["frames"]):
            col = local_index % sequence["cols"]
            row = (local_index // sequence["cols"]) + row_offset
            cell, source_cell_rect = clean_cell(sheet, col, row, sequence["cols"], sequence["rows"])
            if cell.getchannel("A").getbbox() is None:
                raise RuntimeError(f"Empty art cell in {source_path.name}: {frame_name}")
            frame, sprite_rect = place_in_cell(cell, (CELL, CELL), BODY_BASELINE_Y)
            index = len(all_frames)
            atlas_col = index % ATLAS_COLS
            atlas_row = index // ATLAS_COLS
            x, y = atlas_col * CELL, atlas_row * CELL
            atlas.alpha_composite(frame, (x, y))
            ora_layers.append((frame_name, frame, x, y))
            frame_images.append(frame)
            segment_frames.append(frame)
            all_frames.append({
                "index": index,
                "name": frame_name,
                "sequence": sequence["name"],
                "sourceSheet": f"UE5/SourceAssets/P5/Characters/Source/A4_Animation_v03_WIP/{source_path.name}",
                "sourceCell": source_cell_rect,
                "atlasRect": [x, y, CELL, CELL],
                "spriteRectInCell": sprite_rect,
                "durationMs": sequence["duration"][local_index],
                "loop": sequence["loop"],
                "anchors": {
                    "footContactUV": [0.5, BODY_BASELINE_Y / CELL],
                    "bodyRootUV": [0.5, 0.70],
                    "handAnchorsUV": HAND_ANCHOR_CANDIDATES[sequence["name"]],
                    "rideSeatUV": [0.5, 0.64] if sequence["name"] == "Ride" else None,
                    "status": "provisional; hand points are pose estimates; calibrate against slope, board and mount in UE Editor"
                }
            })
        gif_path = BODY_EVIDENCE / f"P5_A4_{sequence['name']}_v05_WIP.gif"
        make_gif(segment_frames, gif_path, sequence["duration"], sequence["loop"])
    atlas_path = BODY_EXPORT / "P5_A4_NaiwaBodyAtlas_v05_WIP.png"
    atlas.save(atlas_path, format="PNG", optimize=True)
    ora_path = BODY_SOURCE / "P5_A4_NaiwaBodyAtlas_v05_layers_WIP.ora"
    make_ora(ora_path, atlas.width, atlas.height, ora_layers)
    return all_frames, frame_images


def write_board() -> dict:
    source_path = BODY_SOURCE / "P5_A4_Snowboard_v01_Source_WIP.png"
    image = Image.open(source_path).convert("RGBA")
    bbox = image.getchannel("A").getbbox()
    if not bbox:
        raise RuntimeError("Snowboard source is transparent")
    sprite = image.crop(bbox)
    # Fit the board inside the 4:1 game texture while preserving a clean pivot gutter.
    canvas = Image.new("RGBA", (1024, 256), (0, 0, 0, 0))
    size = (920, 190)
    scale = min(size[0] / sprite.width, size[1] / sprite.height)
    out_size = (max(1, round(sprite.width * scale)), max(1, round(sprite.height * scale)))
    sprite = resize_straight_alpha(sprite, out_size)
    x, y = (1024 - sprite.width) // 2, (256 - sprite.height) // 2
    canvas.alpha_composite(sprite, (x, y))
    output = BODY_EXPORT / "P5_A4_Snowboard_v01_WIP.png"
    canvas.save(output, format="PNG", optimize=True)
    ora = BODY_SOURCE / "P5_A4_Snowboard_v01_layers_WIP.ora"
    make_ora(ora, canvas.width, canvas.height, [("Snowboard", canvas, 0, 0)])
    return {"source": source_path, "output": output, "ora": ora, "spriteRect": [x, y, sprite.width, sprite.height]}


def write_kangaroo() -> tuple[list[dict], Path]:
    source_path = ENV_SOURCE / "P5_A5_KangarooMount_v02_Source_WIP.png"
    sheet = Image.open(source_path).convert("RGBA")
    names = ["AvailableIdle", "ReadyToCarry", "DownhillGlide", "ImpactReact", "Bounce", "RecoveredIdle"]
    atlas = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    frames = []
    layers = []
    x_edges = [0, round(1024 / 3), round(2048 / 3), 1024]
    for index, name in enumerate(names):
        source_col = index % 3
        source_row = index // 3
        cell, source_cell_rect = clean_cell(sheet, source_col, source_row, 3, 2, remove_blue_reaction=True)
        x, y = x_edges[source_col], source_row * 512
        width, height = x_edges[source_col + 1] - x, 512
        frame, sprite_rect = place_in_cell(cell, (width, height), 494)
        atlas.alpha_composite(frame, (x, y))
        layers.append((name, frame, x, y))
        frames.append({
            "index": index,
            "name": name,
            "sourceCell": source_cell_rect,
            "atlasRect": [x, y, width, height],
            "spriteRectInCell": sprite_rect,
            "anchors": {
                "footAnchorUV": [0.5, 0.965],
                "saddleCenterUV": [0.50, 0.48],
                "saddleForwardUV": [0.68, 0.48],
                "status": "provisional; align with player Ride frames in UE Editor"
            }
        })
    output = ENV_EXPORT / "P5_A5_KangarooMount_v02_WIP.png"
    output.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(output, format="PNG", optimize=True)
    ora_path = ENV_SOURCE / "P5_A5_KangarooMount_v02_layers_WIP.ora"
    make_ora(ora_path, atlas.width, atlas.height, layers)
    return frames, output


def main() -> None:
    for directory in (BODY_SOURCE, BODY_EXPORT, BODY_EVIDENCE, ENV_SOURCE, ENV_EXPORT, MANIFESTS):
        directory.mkdir(parents=True, exist_ok=True)
    body_frames, _ = write_body_assets()
    board = write_board()
    kangaroo_frames, kangaroo_output = write_kangaroo()

    anchors = {
        "schemaVersion": 1,
        "status": "WIP; all anchors are provisional and require UE slope/mount calibration",
        "coordinateSystem": {"origin": "top-left of each sprite cell", "u": "left-to-right", "v": "top-to-bottom", "range": "0..1"},
        "bodyAtlas": {"file": "UE5/SourceAssets/P5/Characters/Exports/P5_A4_NaiwaBodyAtlas_v05_WIP.png", "columns": 4, "rows": 6, "cellSize": [CELL, CELL], "frames": body_frames},
        "snowboard": {
            "file": str(board["output"].relative_to(ROOT)).replace("\\", "/"),
            "size": [1024, 256],
            "rotationPivotUV": [0.5, 0.5],
            "contactLineUV": [[0.08, 0.76], [0.92, 0.76]],
            "status": "provisional; verify board bottom against feet at -30,-15,0,+15,+30 degrees"
        },
        "kangarooAtlas": {
            "file": str(kangaroo_output.relative_to(ROOT)).replace("\\", "/"),
            "columns": 3, "rows": 2, "size": [1024, 1024], "frames": kangaroo_frames,
            "status": "saddle anchors are approximate; calibrate against the separate player Ride pose"
        }
    }
    anchors_path = MANIFESTS / "P5_A4_A5_anchors_v03_WIP.json"
    anchors_path.write_text(json.dumps(anchors, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    manifest = {
        "manifestId": "P5_A4_A5_ContinuousMotionCandidates",
        "version": "v03",
        "date": "2026-10-29",
        "status": "WIP candidate package; not runtime accepted; identity, timing, 1080p/720p, UE alpha and anchors pending review",
        "reviewNotes": [
            "Glide v03 visual pose direction approved by project owner on 2026-10-29; runtime integration and in-game validation are still pending.",
            "Project owner approved A4 v05 art direction for continued preparation on 2026-10-29; A1/UE runtime validation remains pending."
        ],
        "generator": "built-in image_gen; raw generation sheets retained under Characters/Source/A4_Animation_v03_WIP and Environment/Source/A5_Mount_v02_WIP",
        "rebuildScript": "UE5/Scripts/p5_build_a4a5_candidate_assets.py",
        "bodyAtlas": {
            "file": "UE5/SourceAssets/P5/Characters/Exports/P5_A4_NaiwaBodyAtlas_v05_WIP.png",
            "format": "PNG RGBA straight alpha; WIP",
            "dimensions": [ATLAS_COLS * CELL, ATLAS_ROWS * CELL],
            "frameCount": len(body_frames), "grid": [ATLAS_COLS, ATLAS_ROWS], "cell": [CELL, CELL],
            "frames": body_frames,
            "sourceSheets": [{"file": f"UE5/SourceAssets/P5/Characters/Source/A4_Animation_v03_WIP/{seq['file']}", "sha256": sha256(BODY_SOURCE / seq["file"])} for seq in SEQUENCES],
            "editableSource": "UE5/SourceAssets/P5/Characters/Source/A4_Animation_v03_WIP/P5_A4_NaiwaBodyAtlas_v05_layers_WIP.ora",
            "previews": [f"UE5/Records/p5_p6_plan/evidence/P5_A0/P5_A4_{seq['name']}_v05_WIP.gif" for seq in SEQUENCES]
        },
        "snowboard": {
            "file": "UE5/SourceAssets/P5/Characters/Exports/P5_A4_Snowboard_v01_WIP.png",
            "source": "UE5/SourceAssets/P5/Characters/Source/A4_Animation_v03_WIP/P5_A4_Snowboard_v01_Source_WIP.png",
            "dimensions": [1024, 256], "rotationPivotUV": [0.5, 0.5],
            "editableSource": str(board["ora"].relative_to(ROOT)).replace("\\", "/")
        },
        "kangaroo": {
            "file": "UE5/SourceAssets/P5/Environment/Exports/P5_A5_KangarooMount_v02_WIP.png",
            "source": "UE5/SourceAssets/P5/Environment/Source/A5_Mount_v02_WIP/P5_A5_KangarooMount_v02_Source_WIP.png",
            "dimensions": [1024, 1024], "grid": [3, 2], "frameCount": len(kangaroo_frames),
            "editableSource": "UE5/SourceAssets/P5/Environment/Source/A5_Mount_v02_WIP/P5_A5_KangarooMount_v02_layers_WIP.ora",
            "frames": kangaroo_frames
        },
        "anchors": "UE5/SourceAssets/P5/Manifests/P5_A4_A5_anchors_v03_WIP.json",
        "sourceHash": {"snowboard": sha256(board["source"]), "kangaroo": sha256(ENV_SOURCE / "P5_A5_KangarooMount_v02_Source_WIP.png")}
    }
    manifest_path = MANIFESTS / "P5_A4_A5_ContinuousMotion_candidates_v03_WIP.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"bodyAtlas": manifest["bodyAtlas"]["file"], "bodyFrames": len(body_frames), "snowboard": manifest["snowboard"]["file"], "kangaroo": manifest["kangaroo"]["file"], "kangarooFrames": len(kangaroo_frames), "anchors": str(anchors_path.relative_to(ROOT)).replace("\\", "/"), "manifest": str(manifest_path.relative_to(ROOT)).replace("\\", "/")}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
