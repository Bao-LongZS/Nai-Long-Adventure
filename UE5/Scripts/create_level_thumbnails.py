"""Create square menu thumbnails from the existing P1/P2 gameplay captures."""

from pathlib import Path
from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "UE5/NaiwaP0/SourceAssets/MenuUI"
CAPTURES = ROOT / "UE5/NaiwaP0/SourceAssets/MenuUI/Captures"
P1_CAPTURE = CAPTURES / "P1_11_run_start.png"

# Each crop keeps the gameplay subject while excluding the in-game diagnostic HUD.
THUMBNAILS = {
    "T_UI_Thumbnail_11.png": (P1_CAPTURE, (350, 350, 890, 890)),
    "T_UI_Thumbnail_21.png": (CAPTURES / "P2_21_geo_unlit.png",
                                (760, 180, 1260, 680)),
    "T_UI_Thumbnail_22.png": (CAPTURES / "P2_22_geo_unlit.png",
                                (780, 105, 1280, 605)),
    "T_UI_Thumbnail_23.png": (CAPTURES / "P2_23_geo_unlit.png",
                                (650, 180, 1150, 680)),
}


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for filename, (source, bounds) in THUMBNAILS.items():
        if not source.is_file():
            raise FileNotFoundError(source)
        with Image.open(source) as capture:
            capture = capture.convert("RGB")
            left, top, right, bottom = bounds
            if right > capture.width or bottom > capture.height:
                raise ValueError(f"Crop {bounds} exceeds {source} size {capture.size}")
            thumbnail = capture.crop(bounds).resize((512, 512), Image.Resampling.LANCZOS)
            thumbnail.save(OUTPUT / filename, format="PNG", optimize=True)
            print(f"created {filename} from {source.name} crop={bounds}")


if __name__ == "__main__":
    main()
