"""Prepare high quality transparent P3 actor portraits from the approved source art."""
from collections import deque
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = ROOT / "UE5/SourceAssets/P3/Portraits/Raw"
OUTPUT_DIR = ROOT / "UE5/SourceAssets/P3/Portraits/Processed"
TARGET_HEIGHT = 1024
BACKGROUND_THRESHOLD = 235


def remove_connected_white_background(source: Path) -> Image.Image:
    image = Image.open(source).convert("RGBA")
    width = max(1, round(image.width * TARGET_HEIGHT / image.height))
    image = image.resize((width, TARGET_HEIGHT), Image.Resampling.LANCZOS)
    pixels = image.load()
    width, height = image.size
    seen = bytearray(width * height)
    queue = deque()

    def is_background(x: int, y: int) -> bool:
        red, green, blue, alpha = pixels[x, y]
        return alpha > 0 and min(red, green, blue) >= BACKGROUND_THRESHOLD

    for x in range(width):
        for y in (0, height - 1):
            index = y * width + x
            if not seen[index] and is_background(x, y):
                seen[index] = 1
                queue.append((x, y))
    for y in range(height):
        for x in (0, width - 1):
            index = y * width + x
            if not seen[index] and is_background(x, y):
                seen[index] = 1
                queue.append((x, y))

    while queue:
        x, y = queue.popleft()
        red, green, blue, _ = pixels[x, y]
        pixels[x, y] = (red, green, blue, 0)
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < width and 0 <= ny < height:
                index = ny * width + nx
                if not seen[index] and is_background(nx, ny):
                    seen[index] = 1
                    queue.append((nx, ny))

    bounds = image.getbbox()
    if bounds:
        image = image.crop(bounds)
    return image


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    sources = sorted(SOURCE_DIR.glob("SPR_*.png"))
    if len(sources) != 4:
        raise RuntimeError(f"Expected 4 approved P3 portraits in {SOURCE_DIR}, found {len(sources)}")

    for source in sources:
        image = remove_connected_white_background(source)
        target = OUTPUT_DIR / source.name
        image.save(target, format="PNG", optimize=True)
        print(f"{source.name}: {image.width}x{image.height}, {target.stat().st_size:,} bytes")


if __name__ == "__main__":
    main()
