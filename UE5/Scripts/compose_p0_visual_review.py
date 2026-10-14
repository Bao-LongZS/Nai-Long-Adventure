"""Make proportion-preserving sheets from the real UE phase review captures."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser()
parser.add_argument('run_directory', type=Path)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
data = json.loads((args.run_directory / 'manifest.json').read_text(encoding='utf-8-sig'))
assert data['capture_complete'] and len(data['entries']) == 80
args.output.mkdir(parents=True, exist_ok=True)
font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 16)
for clip in dict.fromkeys(row['clip'] for row in data['entries']):
    rows = [row for row in data['entries'] if row['clip'] == clip]
    sheet = Image.new('RGB', (1600, 1520), '#17232d')
    draw = ImageDraw.Draw(sheet)
    draw.text((12, 10), f'{clip} | held UE phases; saved gameplay camera | final 4 tiles: end/start, end/start', font=font, fill='white')
    for i, row in enumerate(rows):
        assert row['saved']
        image = Image.open(row['file']).convert('RGB')
        w, h = image.size
        image = image.crop((w // 4, h // 4, 3 * w // 4, h))
        image.thumbnail((390, 325), Image.Resampling.LANCZOS)
        x, y = (i % 4) * 400, 40 + (i // 4) * 367
        sheet.paste(image, (x + (400 - image.width) // 2, y))
        draw.text((x + 8, y + 326), f"phase {row['phase_index']:02} | t={row['sample_time_s']:.4f}s", font=font, fill='white')
        draw.text((x + 8, y + 345), f"front/rear: {row['front_extent_from_actor_cm']:.2f}/{row['rear_extent_from_actor_cm']:.2f} cm", font=font, fill='#b7d7e2')
    sheet.save(args.output / (clip + '.png'))
extents = {key: max(row[key] for row in data['entries']) for key in ('front_extent_from_actor_cm', 'rear_extent_from_actor_cm')}
result = {'source': str(args.run_directory.resolve()), 'sampled_poses': 80,
          'maximum_surface_extents_cm': extents,
          'scope': 'Held animation phases, not player movement, hardware input, or full continuous playback.',
          'sheets': [str(p) for p in args.output.glob('AS_Naiwa_*.png')]}
(args.output / 'summary.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(result, ensure_ascii=False))
