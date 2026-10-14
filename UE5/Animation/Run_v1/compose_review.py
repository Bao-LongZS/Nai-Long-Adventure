"""Compose existing native Blender renders into a compact pose review sheet."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 18)
small = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 15)
sheet = Image.new('RGB', (1024, 1190), (45, 45, 45))
draw = ImageDraw.Draw(sheet)
draw.text((16, 12), 'Naiwa E2 / Big-arm cute run + local belly bounce / 24 fps / 0.667 s', font=font, fill='white')
poses = [(1, 'L contact'), (3, 'L compression'), (5, 'flight / recoil'), (7, 'recovery'),
         (9, 'R contact'), (11, 'R compression'), (13, 'flight / recoil'), (15, 'recovery')]
for view_index, view in enumerate(['side', 'three_quarter']):
    for index, (frame, label) in enumerate(poses):
        col, row = index % 4, index // 4 + view_index * 2
        x, y = col * 256, 42 + row * 282
        image = Image.open(HERE / view / f'pose_{frame:04d}.png').convert('RGB').resize((256, 256), Image.Resampling.LANCZOS)
        sheet.paste(image, (x, y))
        draw.text((x + 5, y + 260), f'{view}  f{frame:02}  {label}', font=small, fill=(235, 235, 235))
sheet.save(HERE / 'Naiwa_Run_pose_sheet.png')
print(HERE / 'Naiwa_Run_pose_sheet.png')
