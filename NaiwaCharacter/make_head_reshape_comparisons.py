"""Assemble existing head-reshape PNGs; no Blender or model operations.

Usage: python make_head_reshape_comparisons.py --output comparisons
"""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


VIEWS = (("front", "Front"), ("side", "Side"), ("three_quarter", "45deg"))
PREFIXES = (("before", "Before"), ("after", "After"))
IMAGE_SIZE = (900, 1000)
MARGIN = 24
GAP = 20
TITLE_HEIGHT = 70
LABEL_HEIGHT = 52
BACKGROUND = (27, 30, 34)
TEXT_COLOR = (242, 243, 245)


def arial_font(size):
    for font_path in (Path("C:/Windows/Fonts/arial.ttf"), Path("arial.ttf")):
        try:
            return ImageFont.truetype(str(font_path), size)
        except OSError:
            continue
    return ImageFont.load_default()


def make_sheet(output, group, views, title, destination):
    width = MARGIN * 2 + len(views) * IMAGE_SIZE[0] + (len(views) - 1) * GAP
    row_height = LABEL_HEIGHT + IMAGE_SIZE[1]
    height = MARGIN * 2 + TITLE_HEIGHT + 2 * row_height + GAP
    canvas = Image.new("RGB", (width, height), BACKGROUND)
    draw = ImageDraw.Draw(canvas)
    draw.text((MARGIN, MARGIN), title, font=arial_font(34), fill=TEXT_COLOR)
    font = arial_font(28)
    for row, (prefix, label) in enumerate(PREFIXES):
        y = MARGIN + TITLE_HEIGHT + row * (row_height + GAP)
        for column, (view, view_label) in enumerate(views):
            x = MARGIN + column * (IMAGE_SIZE[0] + GAP)
            draw.text((x, y), f"{label} - {view_label}", font=font, fill=TEXT_COLOR)
            source = output / f"{prefix}_{group}_{view}.png"
            with Image.open(source) as image:
                if image.size != IMAGE_SIZE:
                    raise ValueError(f"Expected {IMAGE_SIZE}: {source} has {image.size}")
                # The original render is pasted at native size, without resampling.
                canvas.paste(image.convert("RGB"), (x, y + LABEL_HEIGHT))
    destination_path = output / destination
    canvas.save(destination_path)
    return destination_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path,
                        help="Directory containing the 14 Blender PNG renders")
    parser.add_argument("--head-only", action="store_true")
    args = parser.parse_args()
    output = args.output.expanduser().resolve()
    groups = (("head", VIEWS), ("head_gray", VIEWS))
    if not args.head_only:
        groups += (("full", VIEWS[:1]),)
    required = [output / f"{prefix}_{group}_{view}.png"
                for prefix, _ in PREFIXES
                for group, views in groups
                for view, _ in views]
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise FileNotFoundError("Missing comparison renders:\n" + "\n".join(missing))
    sheets = (
        ("head", VIEWS, "Head color - identical camera and lighting", "comparison_head.png"),
        ("head_gray", VIEWS, "Head gray - identical camera and lighting", "comparison_head_gray.png"),
        ("full", VIEWS[:1], "Full body - identical camera and lighting", "comparison_full.png"),
    )
    for group, views, title, destination in sheets:
        if args.head_only and group == "full":
            continue
        path = make_sheet(output, group, views, title, destination)
        print("HEAD_RESHAPE_CONTACT_SHEET_OK", str(path))


if __name__ == "__main__":
    main()
