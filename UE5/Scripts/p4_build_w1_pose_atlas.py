"""Pack two existing static P3 poses into a P4-only W1 atlas probe.

This is a rendering/anchor/material input, not production animation art.
"""
from pathlib import Path
import json

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = ROOT / "UE5/SourceAssets/P3/Portraits/Processed"
OUTPUT_DIR = ROOT / "UE5/SourceAssets/P4/Portraits/W1"
OUTPUT_PNG = OUTPUT_DIR / "P4_Player_W1_StaticPoseAtlas.png"
OUTPUT_JSON = OUTPUT_DIR / "P4_Player_W1_StaticPoseAtlas.json"
BOTTOM_ANCHOR_MARGIN = 24
CELL_GAP = 2

POSES = [
    ("idle_neutral", "SPR_Player_Neutral_A.png"),
    ("hit_reaction", "SPR_Player_Hit_A.png"),
]


def load_pose(source_name: str) -> tuple[Image.Image, tuple[int, int, int, int]]:
    image = Image.open(SOURCE_DIR / source_name).convert("RGBA")
    alpha = image.getchannel("A")
    # P3 processed exports contain a second sprite row below the character.
    # Find the largest connected opaque component first, then keep all pixels
    # above its foot line so separate hit VFX remain while neighboring sprites
    # are excluded. This only changes the P4-derived probe, never the P3 source.
    mask = alpha.point(lambda value: 255 if value >= 8 else 0)
    bounds = mask.getbbox()
    if not bounds:
        raise RuntimeError(f"No visible pixels in {source_name}")

    width, height = image.size
    pixels = mask.load()
    visited = bytearray(width * height)
    components: list[tuple[int, tuple[int, int, int, int]]] = []
    for y in range(height):
        for x in range(width):
            index = y * width + x
            if visited[index] or pixels[x, y] == 0:
                continue
            visited[index] = 1
            stack = [index]
            area = 0
            min_x = max_x = x
            min_y = max_y = y
            while stack:
                current = stack.pop()
                cy, cx = divmod(current, width)
                area += 1
                min_x, max_x = min(min_x, cx), max(max_x, cx)
                min_y, max_y = min(min_y, cy), max(max_y, cy)
                for nx, ny in ((cx - 1, cy), (cx + 1, cy), (cx, cy - 1), (cx, cy + 1)):
                    if 0 <= nx < width and 0 <= ny < height:
                        neighbor = ny * width + nx
                        if not visited[neighbor] and pixels[nx, ny] != 0:
                            visited[neighbor] = 1
                            stack.append(neighbor)
            components.append((area, (min_x, min_y, max_x + 1, max_y + 1)))

    _, character_bounds = max(components, key=lambda item: item[0])
    # Keep separate VFX that sit alongside the character, but crop at the
    # dominant character's bottom edge to remove the next sprite row.
    crop_mask = Image.new("RGBA", image.size, (0, 0, 0, 0))
    crop_mask.paste(image.crop((0, 0, width, character_bounds[3])), (0, 0))
    pose_bounds = crop_mask.getchannel("A").point(lambda value: 255 if value >= 8 else 0).getbbox()
    return image.crop(pose_bounds), pose_bounds


def main() -> None:
    poses = []
    for pose_id, source_name in POSES:
        image, bounds = load_pose(source_name)
        poses.append({"id": pose_id, "source": source_name, "image": image, "source_bounds": bounds})

    cell_width = max(pose["image"].width for pose in poses) + 48
    cell_height = max(pose["image"].height for pose in poses) + BOTTOM_ANCHOR_MARGIN
    atlas = Image.new("RGBA", (cell_width * len(poses) + CELL_GAP * (len(poses) - 1), cell_height), (0, 0, 0, 0))
    frame_records = []
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    for index, pose in enumerate(poses):
        image = pose["image"]
        x = index * (cell_width + CELL_GAP) + (cell_width - image.width) // 2
        y = cell_height - BOTTOM_ANCHOR_MARGIN - image.height
        cell = Image.new("RGBA", (cell_width, cell_height), (0, 0, 0, 0))
        cell.alpha_composite(image, ((cell_width - image.width) // 2, y))
        atlas.alpha_composite(cell, (index * (cell_width + CELL_GAP), 0))
        frame_texture = OUTPUT_DIR / f"P4_Player_W1_{pose['id']}.png"
        cell.save(frame_texture, format="PNG", optimize=True)
        frame_records.append({
            "id": pose["id"],
            "source": f"UE5/SourceAssets/P3/Portraits/Processed/{pose['source']}",
            "source_alpha_bounds": list(pose["source_bounds"]),
            "atlas_rect": [index * (cell_width + CELL_GAP), 0, cell_width, cell_height],
            "frame_texture": str(frame_texture.relative_to(ROOT)).replace("\\", "/"),
            "visible_rect": [x, y, image.width, image.height],
            "foot_anchor": [index * (cell_width + CELL_GAP) + cell_width // 2, cell_height - BOTTOM_ANCHOR_MARGIN],
            "production_animation_frame": False,
        })

    atlas.save(OUTPUT_PNG, format="PNG", optimize=True)
    manifest = {
        "status": "static_pose_render_probe",
        "atlas": str(OUTPUT_PNG.relative_to(ROOT)).replace("\\", "/"),
        "size": [atlas.width, atlas.height],
        "layout": "2 cells, horizontal; transparent RGBA; baseline-aligned foot anchors",
        "cell_size": [cell_width, cell_height],
        "frames": frame_records,
        "limitation": "Two static poses only; does not satisfy idle/move/attack/dash/defend/laugh animation production or three-direction coverage.",
    }
    OUTPUT_JSON.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps(manifest, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
