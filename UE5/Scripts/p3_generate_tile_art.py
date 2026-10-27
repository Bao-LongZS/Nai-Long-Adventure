"""Generate crisp, palette-matched P3 board textures at the exact game-cell scale."""
from pathlib import Path
import math
import random

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "UE5/SourceAssets/P3/Tiles"
SIZE = 256


def textured_surface(base, seed, strength=5):
    rng = random.Random(seed)
    pixels = []
    for y in range(SIZE):
        vertical = 1.0 - 0.045 * (y / (SIZE - 1))
        for x in range(SIZE):
            grain = rng.randint(-strength, strength)
            pixels.append(tuple(max(0, min(255, round(channel * vertical) + grain)) for channel in base))
    return Image.frombytes("RGB", (SIZE, SIZE), bytes(channel for pixel in pixels for channel in pixel))


def chamfered(points, inset):
    out = []
    for index, (x, y) in enumerate(points):
        px, py = points[index - 1]
        nx, ny = points[(index + 1) % len(points)]
        def toward(tx, ty):
            dx, dy = tx - x, ty - y
            distance = math.hypot(dx, dy)
            return (x + dx * inset / distance, y + dy * inset / distance)
        out.extend((toward(px, py), toward(nx, ny)))
    return out


def beveled_tile(image, edge, face, highlight, shadow, inset=13):
    draw = ImageDraw.Draw(image)
    square = [(0, 0), (SIZE - 1, 0), (SIZE - 1, SIZE - 1), (0, SIZE - 1)]
    inner = chamfered(square, inset)
    draw.polygon(square, fill=edge)
    draw.line([inner[0], inner[1], inner[2]], fill=highlight, width=5)
    draw.line([inner[4], inner[5], inner[6]], fill=shadow, width=5)
    draw.polygon(inner, fill=face)
    return draw


def make_floor(seed, base):
    image = textured_surface(base, seed, 3)
    draw = beveled_tile(image, (20, 33, 44), (42, 61, 75), (86, 110, 127), (27, 43, 55), 14)
    draw.rounded_rectangle((22, 22, 233, 233), radius=12, outline=(56, 77, 91), width=2)
    draw.line((36, 46, 74, 46), fill=(67, 87, 101), width=2)
    draw.line((182, 209, 221, 209), fill=(34, 50, 62), width=2)
    draw.line((205, 32, 220, 32), fill=(74, 95, 110), width=2)
    rng = random.Random(seed + 19)
    for _ in range(90):
        x = rng.randint(28, 228)
        y = rng.randint(28, 228)
        c = rng.choice(((51, 69, 83), (61, 79, 92), (34, 50, 63)))
        draw.point((x, y), fill=c)
    # Four tiny rivets make the cell boundary readable at gameplay scale.
    for x, y in ((19, 19), (236, 19), (19, 236), (236, 236)):
        draw.ellipse((x - 3, y - 3, x + 3, y + 3), fill=(101, 121, 134))
        draw.point((x - 1, y - 1), fill=(160, 174, 181))
    return image


def make_stone():
    image = textured_surface((78, 94, 110), 29, 8)
    draw = beveled_tile(image, (28, 41, 53), (82, 101, 119), (143, 159, 170), (39, 55, 68), 17)
    draw.polygon(((58, 62), (88, 53), (117, 66), (103, 83), (77, 79)), fill=(91, 109, 124))
    draw.line(((62, 62), (77, 91), (68, 111), (84, 129), (78, 151)), fill=(45, 61, 74), width=5)
    draw.line(((77, 91), (104, 89), (121, 103)), fill=(51, 66, 79), width=4)
    draw.line(((78, 151), (60, 173), (67, 194)), fill=(48, 63, 76), width=4)
    draw.line(((80, 94), (75, 105), (83, 112)), fill=(127, 142, 153), width=2)
    draw.line(((132, 165), (160, 153), (176, 163)), fill=(61, 78, 91), width=3)
    for x, y in ((36, 39), (214, 47), (205, 218), (39, 210)):
        draw.ellipse((x - 3, y - 3, x + 3, y + 3), fill=(48, 64, 77))
    return image


def make_crate(item=False):
    image = textured_surface((93, 65, 43), 47 if not item else 53, 8)
    draw = beveled_tile(image, (48, 39, 36), (151, 107, 67), (220, 177, 118), (81, 55, 43), 12)
    # Planked top, dark seams and fine wood grain.
    for y in (55, 101, 147, 193):
        draw.rectangle((18, y - 19, 237, y + 19), fill=(157, 111, 70), outline=(93, 63, 45), width=3)
        draw.line((31, y - 11, 222, y - 11), fill=(188, 140, 91), width=2)
        draw.line((29, y + 10, 220, y + 10), fill=(119, 79, 53), width=2)
        draw.line((47, y + 2, 83, y + 2), fill=(126, 84, 55), width=2)
        draw.line((153, y - 2, 193, y - 2), fill=(201, 151, 98), width=2)
    # Thick crossed braces echo the approved map's distinctive crate mark.
    draw.line(((31, 31), (225, 225)), fill=(68, 49, 41), width=31)
    draw.line(((225, 31), (31, 225)), fill=(68, 49, 41), width=31)
    draw.line(((31, 27), (226, 222)), fill=(213, 166, 107), width=18)
    draw.line(((221, 28), (27, 222)), fill=(213, 166, 107), width=18)
    draw.line(((35, 33), (222, 220)), fill=(157, 109, 67), width=8)
    draw.line(((218, 34), (34, 220)), fill=(157, 109, 67), width=8)
    for x, y in ((31, 31), (225, 31), (31, 225), (225, 225)):
        draw.ellipse((x - 6, y - 6, x + 6, y + 6), fill=(61, 49, 42), outline=(225, 190, 132), width=2)
    if item:
        draw.rounded_rectangle((103, 91, 153, 166), radius=9, fill=(62, 50, 39), outline=(244, 195, 91), width=5)
        draw.ellipse((116, 104, 140, 128), fill=(241, 194, 88), outline=(255, 223, 145), width=3)
        draw.rectangle((115, 121, 141, 151), fill=(218, 161, 64), outline=(255, 219, 126), width=2)
        draw.ellipse((125, 130, 131, 136), fill=(84, 63, 41))
    return image


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    textures = {
        "T_P3Floor_A.png": make_floor(11, (39, 57, 71)),
        "T_P3Floor_B.png": make_floor(17, (43, 61, 75)),
        "T_P3HardStone.png": make_stone(),
        "T_P3WoodCrate.png": make_crate(False),
        "T_P3ItemCrate.png": make_crate(True),
    }
    for name, image in textures.items():
        path = OUTPUT / name
        image.save(path, format="PNG", optimize=True)
        print(f"{path.name}: {image.width}x{image.height}, {path.stat().st_size:,} bytes")


if __name__ == "__main__":
    main()
