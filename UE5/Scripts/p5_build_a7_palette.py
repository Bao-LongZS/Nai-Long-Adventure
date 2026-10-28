from __future__ import annotations

import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = ROOT / "UE5" / "SourceAssets" / "P5" / "Manifests" / "P5_A7_Palette_L4_manifest_v01.json"
EXPORT_PATH = ROOT / "UE5" / "SourceAssets" / "P5" / "Materials" / "Exports" / "Palette_L4_v01_WIP.png"


def load_font(size: int) -> ImageFont.ImageFont:
    for candidate in (r"C:\Windows\Fonts\segoeui.ttf", r"C:\Windows\Fonts\arial.ttf"):
        if Path(candidate).exists():
            return ImageFont.truetype(candidate, size)
    return ImageFont.load_default()


def main() -> None:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    colors = manifest["colors"]
    columns = 4
    card_w, card_h, gap = 278, 132, 16
    margin, header_h = 32, 104
    rows = (len(colors) + columns - 1) // columns
    width = margin * 2 + columns * card_w + (columns - 1) * gap
    height = header_h + margin + rows * card_h + (rows - 1) * gap + margin
    canvas = Image.new("RGB", (width, height), "#E9ECE9")
    draw = ImageDraw.Draw(canvas)
    title_font = load_font(27)
    body_font = load_font(18)
    small_font = load_font(15)
    draw.text((margin, 22), "PALETTE_L4  /  WIP CANDIDATE", font=title_font, fill="#334352")
    draw.text((margin, 61), "Natural-tone snow scene · sampled from P2/P3 palette families · pending in-scene review", font=small_font, fill="#657586")

    for index, item in enumerate(colors):
        row, column = divmod(index, columns)
        x = margin + column * (card_w + gap)
        y = header_h + margin + row * (card_h + gap)
        draw.rounded_rectangle((x, y, x + card_w, y + card_h), radius=9, fill="#FAFBF9", outline="#C9D0D2", width=1)
        swatch = item["hex"]
        draw.rounded_rectangle((x + 10, y + 10, x + card_w - 10, y + 72), radius=6, fill=swatch)
        draw.text((x + 12, y + 82), item["label"], font=body_font, fill="#27333A")
        draw.text((x + 12, y + 107), f"{swatch}   {item['id']}", font=small_font, fill="#657586")

    EXPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(EXPORT_PATH, format="PNG", optimize=True)
    print(f"Wrote {len(colors)} color swatches: {EXPORT_PATH}")


if __name__ == "__main__":
    main()
