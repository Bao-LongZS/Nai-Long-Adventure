"""Contact sheets of actual recorded UE frames, grouped by runtime test case."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser()
parser.add_argument('run_directory', type=Path)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
summary = json.loads((args.run_directory / 'summary.json').read_text(encoding='utf-8-sig'))
args.output.mkdir(parents=True, exist_ok=True)
font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 17)
frames = [f for f in summary['recorded_frames'] if f['round'] == 1]
cases = list(dict.fromkeys(f['case'] for f in frames))
manifest = {'source_run_id': summary['run_id'], 'method': '12 evenly spaced actual recorded frames per first-round case',
            'limits': 'Contact sheets show sampled motion. They do not prove physical keyboard input or replace continuous playback.',
            'sheets': []}
for case in cases:
    group = [f for f in frames if f['case'] == case]
    selected = [group[round(i * (len(group) - 1) / 11)] for i in range(12)]
    sheet = Image.new('RGB', (1440, 1580), '#16202a')
    draw = ImageDraw.Draw(sheet)
    draw.text((12, 10), f"Actual UE runtime | {summary['run_id']} | {case}", font=font, fill='white')
    for i, frame in enumerate(selected):
        source = args.run_directory / 'Frames' / frame['frame_name']
        x, y = (i % 3) * 480, 40 + (i // 3) * 382
        # Keep the gameplay character at a legible scale; crop the center half.
        original = Image.open(source).convert('RGB')
        w, h = original.size
        cropped = original.crop((w // 4, h // 4, 3 * w // 4, h))
        cropped.thumbnail((460, 350), Image.Resampling.LANCZOS)
        sheet.paste(cropped, (x + (480 - cropped.width) // 2, y))
        draw.text((x + 10, y + 354), f"{frame['frame_name']} | wall {frame['wall_time_s']:.3f}s", font=font, fill='#dde5eb')
    path = args.output / (case + '.png')
    sheet.save(path)
    manifest['sheets'].append({'case': case, 'sheet': str(path), 'frames': selected,
                               'crop': 'center half width, lower three quarters height, aspect ratio preserved within 460x350'})
(args.output / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'run_id': summary['run_id'], 'sheets': len(cases), 'output': str(args.output)}))
