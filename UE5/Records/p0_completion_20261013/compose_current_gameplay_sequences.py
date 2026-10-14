"""At most eight chronological original UE viewports per first-round case."""
import json
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'UE5/NaiwaP0/Saved/NaiwaTests/20261012T064857Z_282460'
OUTPUT = Path(__file__).resolve().parent / 'gameplay_sequences'
OUTPUT.mkdir(parents=True, exist_ok=True)
data = json.loads((SOURCE / 'summary.json').read_text(encoding='utf-8-sig'))
frames = [row for row in data['recorded_frames'] if row['round'] == 1]
font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 18)
results = []
for case in dict.fromkeys(row['case'] for row in frames):
    rows = [row for row in frames if row['case'] == case]
    count = min(8, len(rows))
    indices = sorted({round(index * (len(rows) - 1) / (count - 1)) for index in range(count)}) if count > 1 else [0]
    selected = [rows[index] for index in indices]
    sheet = Image.new('RGB', (1280, 72 + math.ceil(count / 2) * 390), '#17232d')
    draw = ImageDraw.Draw(sheet)
    draw.text((12, 8), f'Actual UE | {data["run_id"]} | round 1 | {case}', font=font, fill='white')
    draw.text((12, 34), 'Whole viewport, proportional resize. Chronological original frames; no crop or interpolation.', font=font, fill='#b7d7e2')
    for tile, row in enumerate(selected):
        source = SOURCE / 'Frames' / row['frame_name']
        with Image.open(source) as original:
            image = original.convert('RGB')
        image.thumbnail((620, 349), Image.Resampling.LANCZOS)
        x, y = (tile % 2) * 640, 72 + (tile // 2) * 390
        sheet.paste(image, (x + (640 - image.width) // 2, y + (349 - image.height) // 2))
        draw.text((x + 10, y + 353), f'{row["frame_name"]} | wall={row["wall_time_s"]:.3f}s', font=font, fill='white')
    path = OUTPUT / (case + '.png')
    sheet.save(path)
    results.append(dict(case=case, source_image_count=len(rows), selected_count=count, sheet=str(path),
                        selected_frames=selected, crop='None; full viewport'))
report = dict(source_run_id=data['run_id'], round=1, method='At most eight evenly distributed chronological actual screenshot viewports per case.',
              limits='Selected screenshots provide sampled sequence review; no claim that the MP4 was played or every rendered frame was viewed.', sheets=results)
(OUTPUT / 'manifest.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(dict(cases=len(results), selected_frames=sum(row['selected_count'] for row in results))))
