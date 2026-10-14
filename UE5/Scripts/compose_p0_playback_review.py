"""Review continuous UE SingleNode captures without changing their measured cadence.

python compose_p0_playback_review.py RUN_DIRECTORY --output OUTPUT_DIRECTORY
Add --ffmpeg PATH to encode each clip's screenshots at their actual game-time intervals.
Videos preserve the saved viewport; sheets resize whole frames without cropping.
"""
import argparse
import json
import math
import shutil
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def concat_quote(path):
    return "'" + Path(path).resolve().as_posix().replace("'", "'\\''") + "'"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--ffmpeg', help='Optional executable path/name; no download is performed.')
    parser.add_argument('--max-tiles', type=int, default=24)
    args = parser.parse_args()
    if args.max_tiles < 2:
        parser.error('--max-tiles must be at least 2')
    data = json.loads((args.run_directory / 'manifest.json').read_text(encoding='utf-8-sig'))
    if data.get('mode') != 'continuous_playback' or not data.get('capture_complete'):
        raise ValueError('A completed continuous_playback manifest is required.')
    clips = data['playback_clips']
    if len(clips) != 5 or not all(c['two_cycles_complete'] and c['observed_position_wraps'] >= 2
                                 and c['duration_game_s'] + 1e-5 >= 2 * c['clip_length_s'] for c in clips):
        raise ValueError('The five saved animations must each complete two natural cycles.')
    frames = data['playback_frames']
    for frame in frames:
        if not frame['component_space_bones'] or not math.isfinite(frame['actual_single_node_time_s']):
            raise ValueError('Missing pose or invalid animation time in a game-tick frame.')
        for bone in frame['component_space_bones']:
            values = list(bone['position_cm'].values()) + bone['quaternion_xyzw']
            if not all(math.isfinite(value) for value in values):
                raise ValueError('Invalid bone transform in playback evidence.')
    args.output.mkdir(parents=True, exist_ok=True)
    font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 16)
    encoder = None
    if args.ffmpeg:
        encoder = shutil.which(args.ffmpeg) or (str(Path(args.ffmpeg).resolve()) if Path(args.ffmpeg).is_file() else None)
        if not encoder:
            raise FileNotFoundError(args.ffmpeg)
    results = []
    for clip in clips:
        rows = [r for r in data['entries'] if r['clip'] == clip['clip']]
        if not rows or not all(r.get('saved') and Path(r['file']).is_file() for r in rows):
            raise ValueError('Missing saved screenshot for ' + clip['clip'])
        times = [r['clip_elapsed_game_s'] for r in rows]
        if any(b <= a for a, b in zip(times, times[1:])):
            raise ValueError('Screenshot game times must increase within each clip.')
        count = min(args.max_tiles, len(rows))
        indices = sorted({round(i * (len(rows) - 1) / (count - 1)) for i in range(count)})
        height = 62 + math.ceil(len(indices) / 4) * 255
        sheet = Image.new('RGB', (1600, height), '#17232d')
        draw = ImageDraw.Draw(sheet)
        draw.text((12, 8), f"{clip['clip']} | real UE PlayRate 1 | {clip['observed_position_wraps']} natural wraps | fixed gameplay camera", font=font, fill='white')
        draw.text((12, 31), 'Whole viewport, proportional resize. Selected chronological frames; raw PNGs and every game-tick pose retained.', font=font, fill='#b7d7e2')
        for tile, index in enumerate(indices):
            row = rows[index]
            with Image.open(row['file']) as source:
                image = source.convert('RGB')
            image.thumbnail((390, 216), Image.Resampling.LANCZOS)
            x, y = (tile % 4) * 400, 62 + (tile // 4) * 255
            sheet.paste(image, (x + (400 - image.width) // 2, y + (216 - image.height) // 2))
            draw.text((x + 8, y + 217), f"game={row['clip_elapsed_game_s']:.3f}s | pose={row['actual_single_node_time_s']:.3f}s", font=font, fill='white')
            draw.text((x + 8, y + 236), f"shot {index:03} | wraps={row['observed_position_wraps']}", font=font, fill='#b7d7e2')
        sheet_path = args.output / (clip['clip'] + '_playback.png')
        sheet.save(sheet_path)
        concat_path = args.output / (clip['clip'] + '_game_time.ffconcat')
        lines = ['ffconcat version 1.0']
        for index, row in enumerate(rows):
            duration = times[index + 1] - times[index] if index + 1 < len(rows) else 0.001
            lines.extend(['file ' + concat_quote(row['file']), f'duration {duration:.9f}'])
        lines.append('file ' + concat_quote(rows[-1]['file']))
        concat_path.write_text('\n'.join(lines) + '\n', encoding='utf-8')
        result = dict(clip=clip['clip'], screenshot_count=len(rows), game_tick_frames=clip['game_tick_frames'],
                      game_duration_s=clip['duration_game_s'], natural_wraps=clip['observed_position_wraps'],
                      screenshot_min_interval_game_s=min(b-a for a, b in zip(times, times[1:])),
                      screenshot_max_interval_game_s=max(b-a for a, b in zip(times, times[1:])),
                      maximum_time_continuity_error_s=clip['maximum_time_continuity_error_s'],
                      sheet=str(sheet_path.resolve()), concat=str(concat_path.resolve()))
        if encoder:
            video_path = args.output / (clip['clip'] + '_playback.mp4')
            command = [encoder, '-hide_banner', '-loglevel', 'error', '-y', '-f', 'concat', '-safe', '0',
                       '-i', str(concat_path.resolve()), '-vf', 'fps=30', '-c:v', 'libx264', '-crf', '18',
                       '-pix_fmt', 'yuv420p', '-movflags', '+faststart', str(video_path.resolve())]
            subprocess.run(command, check=True)
            result['video'] = str(video_path.resolve())
        results.append(result)
    report = dict(source=str(args.run_directory.resolve()), capture_complete=True,
                  method='Continuous saved UE animations on a stationary pawn; measured game-time screenshot intervals.',
                  limits='8 fps target screenshots, not every rendered frame. Videos repeat each captured viewport until its next measured game time; every actual game tick retains bone data. No physical input or locomotion claim.',
                  sheet_crop='None; entire original viewport resized proportionally.', clips=results)
    report_path = args.output / 'playback_review_summary.json'
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    main()
