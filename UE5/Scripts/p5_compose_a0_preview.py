"""Compose the provisional P5 A0 art-direction sample from source images."""

from pathlib import Path
import argparse

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
REFERENCE_DIR = ROOT / "UE5" / "SourceAssets" / "P5" / "References"


def load_font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    name = "arialbd.ttf" if bold else "arial.ttf"
    return ImageFont.truetype(Path("C:/Windows/Fonts") / name, size=size)


def draw_panel(
    draw: ImageDraw.ImageDraw,
    bounds: tuple[int, int, int, int],
    color: tuple[int, int, int, int],
    outline: tuple[int, int, int, int],
) -> None:
    draw.rounded_rectangle(bounds, radius=17, fill=color, outline=outline, width=3)
    left, top, right, bottom = bounds
    draw.line((left + 18, top + 5, left + 92, top + 5), fill=(255, 255, 255, 65), width=3)
    draw.line((right - 73, bottom - 5, right - 18, bottom - 5), fill=(8, 25, 48, 85), width=3)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", choices=("v01", "v02"), default="v01")
    version = parser.parse_args().version
    source = REFERENCE_DIR / f"P5_A0_SkiScene_Concept_{version}.png"
    overlay_path = REFERENCE_DIR / f"P5_A0_HUDPlaceholder_{version}.png"
    composite_path = REFERENCE_DIR / f"P5_A0_SkiScene_WithHUD_{version}.png"

    scene = Image.open(source).convert("RGBA")
    width, height = scene.size
    overlay = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    navy = (17, 39, 69, 236)
    navy_outline = (120, 165, 211, 205)
    pale = (237, 245, 255, 255)
    muted = (186, 211, 238, 255)

    draw_panel(draw, (38, 32, 304, 142), navy, navy_outline)
    draw.text((58, 43), "DISTANCE", font=load_font(18, True), fill=muted)
    draw.text((55, 67), "842 m", font=load_font(52, True), fill=pale, stroke_width=1, stroke_fill=(8, 25, 48, 190))

    gap_bounds = (1125, 32, 1375, 142)
    draw_panel(draw, gap_bounds, (241, 174, 52, 246), (255, 228, 152, 245))
    draw.polygon([(1145, 79), (1163, 49), (1181, 79)], fill=(28, 47, 70, 255))
    draw.rounded_rectangle((1160, 60, 1166, 71), radius=2, fill=(255, 224, 145, 255))
    draw.ellipse((1160, 74, 1166, 80), fill=(255, 224, 145, 255))
    draw.text((1190, 43), "AVALANCHE GAP", font=load_font(16, True), fill=(39, 50, 60, 255))
    draw.text((1190, 67), "48 m", font=load_font(50, True), fill=(24, 39, 58, 255))

    draw_panel(draw, (1390, 32, 1515, 142), navy, navy_outline)
    draw.ellipse((1407, 61, 1455, 109), fill=(231, 155, 28, 255), outline=(255, 224, 132, 255), width=4)
    draw.ellipse((1415, 69, 1447, 101), fill=(255, 210, 79, 255), outline=(179, 111, 15, 255), width=2)
    draw.text((1461, 69), "12", font=load_font(34, True), fill=pale, stroke_width=1, stroke_fill=(8, 25, 48, 190))
    draw.text((1408, 115), "COINS", font=load_font(14, True), fill=muted)

    draw_panel(draw, (1528, 32, 1634, 142), navy, navy_outline)
    draw.polygon(
        [(1550, 64), (1556, 78), (1571, 79), (1560, 88), (1564, 102), (1550, 94), (1538, 102), (1541, 88), (1530, 79), (1545, 78)],
        fill=(246, 190, 70, 255),
        outline=(255, 229, 159, 255),
    )
    draw.text((1573, 68), "x2", font=load_font(30, True), fill=pale, stroke_width=1, stroke_fill=(8, 25, 48, 190))
    draw.text((1541, 115), "COMBO", font=load_font(14, True), fill=muted)

    overlay.save(overlay_path, optimize=True)
    Image.alpha_composite(scene, overlay).convert("RGB").save(composite_path, quality=96, optimize=True)
    print(f"Canvas: {width}x{height}")
    print(f"Overlay: {overlay_path}")
    print(f"Composite: {composite_path}")


if __name__ == "__main__":
    main()
