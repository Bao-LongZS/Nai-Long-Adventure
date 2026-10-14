"""Place the saved R1 front and side renders together without cropping them."""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parent
OUT = ROOT / "versions" / "20261007_N2_round_refinement" / "showcase"
DESTINATION = OUT / "奶蛙_R1_全身正面与侧面.png"


def centered_text(draw, text, center_x, top, font, color):
    box = draw.textbbox((0, 0), text, font=font)
    draw.text((center_x - (box[2] - box[0]) / 2, top - box[1]), text,
              font=font, fill=color)


def main():
    renders = []
    for name in ("R1_full_front.png", "R1_full_side.png"):
        with Image.open(OUT / name) as source:
            renders.append(source.convert("RGB"))
    assert renders[0].size == renders[1].size == (1100, 1500)
    tile_w, tile_h = renders[0].size
    header, footer = 160, 28
    background = renders[0].getpixel((0, 0))
    canvas = Image.new("RGB", (tile_w * 2, header + tile_h + footer), background)
    draw = ImageDraw.Draw(canvas)
    title_font = ImageFont.truetype("C:/Windows/Fonts/msyhbd.ttc", 44)
    label_font = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 30)
    centered_text(draw, "奶蛙 R1 · 全身效果", tile_w, 40, title_font, "#fff3bb")
    for index, (label, render) in enumerate(zip(("正面", "侧面"), renders)):
        centered_text(draw, label, tile_w * (index + 0.5), 115,
                      label_font, "#eeeeee")
        position = (index * tile_w, header)
        canvas.paste(render, position)
        check = canvas.crop((position[0], position[1],
                             position[0] + tile_w, position[1] + tile_h))
        assert check.tobytes() == render.tobytes(), "Render pixels were changed"
    canvas.save(DESTINATION, optimize=True)
    print("R1_FULLBODY_SHOWCASE_PANEL_OK")
    print(DESTINATION)
    print(f"Size: {canvas.width} x {canvas.height}")


if __name__ == "__main__":
    main()
