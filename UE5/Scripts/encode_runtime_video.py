"""Encode actual UE screenshot PNGs; this script does not render a character.

"D:/STEAM/steamapps/common/Blender/blender.exe" --background --factory-startup
  --python-exit-code 1 --python encode_runtime_video.py --
  --frames <recording-directory> --output <review.mp4> [--fps 8]
  [--timestamps <CSV with frame_name,wall_time_s>]

Keep the original frame_000000.png sequence. Without screenshot timestamps,
playback uses a nominal frame rate and cannot establish runtime timing or FPS.
"""
from __future__ import annotations

import argparse
from bisect import bisect_right
import csv
from fractions import Fraction
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import sys
import traceback

import bpy


FFPROBE = Path(r"C:\Program Files\Tencent\QQBrowser\22.0.7003.400\ffmpeg\ffprobe.exe")
FRAME_NAME = re.compile(r"frame_(\d{6})\.png")


def png_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"Invalid PNG header: {path}")
    return struct.unpack(">II", header[16:24])


def read_frames(directory: Path) -> tuple[list[Path], tuple[int, int]]:
    if not directory.is_dir():
        raise FileNotFoundError(directory)
    pngs = list(directory.glob("*.png"))
    if not pngs:
        raise ValueError(f"No screenshot PNGs in {directory}")
    unexpected = [path.name for path in pngs if not FRAME_NAME.fullmatch(path.name)]
    if unexpected:
        raise ValueError(f"Unexpected PNG filenames: {unexpected[:10]}")
    frames = sorted(pngs, key=lambda path: int(FRAME_NAME.fullmatch(path.name).group(1)))
    for index, frame in enumerate(frames):
        expected = f"frame_{index:06d}.png"
        if frame.name != expected:
            raise ValueError(f"Sequence must be complete from frame_000000.png; expected {expected}, found {frame.name}")
    dimensions = png_dimensions(frames[0])
    if min(dimensions) < 2 or any(size % 2 for size in dimensions):
        raise ValueError(f"H.264 yuv420p requires positive even dimensions: {dimensions}")
    for frame in frames[1:]:
        if png_dimensions(frame) != dimensions:
            raise ValueError(f"Inconsistent PNG dimensions: {frame}")
    return frames, dimensions


def playback_frames(frames: list[Path], args) -> tuple[list[Path], dict]:
    if args.timestamps is None:
        return frames, {
            "mode": "nominal_frame_rate", "source_timestamp_csv": None,
            "limit": "No screenshot wall timestamps supplied. Playback FPS is nominal; it does not establish game FPS, frame pacing, or actual elapsed duration.",
        }
    timestamps_path = args.timestamps.resolve()
    values = {}
    with timestamps_path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        fields = reader.fieldnames or []
        if args.frame_column not in fields or args.timestamp_column not in fields:
            raise ValueError(f"Timestamp CSV requires columns {args.frame_column!r}, {args.timestamp_column!r}")
        for row in reader:
            # A basename or full screenshot path is accepted, including Windows paths.
            name = row[args.frame_column].strip().replace("\\", "/").rsplit("/", 1)[-1]
            if name in values:
                raise ValueError(f"Duplicate timestamp row for {name}")
            value = float(row[args.timestamp_column])
            if not math.isfinite(value):
                raise ValueError(f"Non-finite screenshot timestamp: {name}")
            values[name] = value
    expected_names = {frame.name for frame in frames}
    if set(values) != expected_names:
        raise ValueError(f"Timestamp rows must exactly match screenshot files; missing={sorted(expected_names - set(values))[:10]}, unexpected={sorted(set(values) - expected_names)[:10]}")
    times = [values[frame.name] for frame in frames]
    if any(later <= earlier for earlier, later in zip(times, times[1:])):
        raise ValueError("Screenshot wall timestamps must strictly increase in frame-number order")
    relative_times = [time - times[0] for time in times]
    # Include the final source screenshot, then show it for one output frame.
    count = math.ceil(relative_times[-1] * args.fps - 1e-9) + 1
    if count > 1_000_000:
        raise ValueError("Timestamp range would produce over 1,000,000 playback frames; check seconds units")
    selected = [frames[max(0, bisect_right(relative_times, index / args.fps + 1e-9) - 1)]
                for index in range(count)]
    return selected, {
        "mode": "wall_timestamp_resampling", "source_timestamp_csv": str(timestamps_path),
        "frame_column": args.frame_column, "timestamp_column": args.timestamp_column,
        "first_wall_timestamp_s": times[0], "last_wall_timestamp_s": times[-1],
        "source_capture_span_s": relative_times[-1],
        "resampling": "Hold the most recent actual screenshot at each output tick; no interpolated images.",
        "tail_assumption": "Round the final screenshot time up to an output tick and show it for one output frame; no later capture timestamp is available.",
        "limit": "Screenshot request wall timestamps approximate capture timing. Dropped frames, render latency, screenshot overhead, and test fixture teleports remain visible limitations; this video does not measure runtime FPS.",
    }


def encode(frames: list[Path], output: Path, fps: int, dimensions: tuple[int, int]):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.frame_start, scene.frame_end = 1, len(frames)
    scene.render.fps, scene.render.fps_base = fps, 1.0
    scene.render.resolution_x, scene.render.resolution_y = dimensions
    scene.render.resolution_percentage = 100
    scene.render.pixel_aspect_x = scene.render.pixel_aspect_y = 1.0
    scene.render.use_sequencer = True
    scene.render.use_compositing = False
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.view_settings.exposure, scene.view_settings.gamma = 0.0, 1.0
    editor = scene.sequence_editor_create()
    strips = editor.strips if hasattr(editor, "strips") else editor.sequences
    strip = strips.new_image("Actual UE runtime screenshots", str(frames[0]), channel=1, frame_start=1)
    for frame in frames[1:]:
        strip.elements.append(frame.name)
    strip.blend_type = "REPLACE"
    strip.frame_final_duration = len(frames)
    strip.colorspace_settings.name = "sRGB"
    if hasattr(scene.render.image_settings, "media_type"):
        scene.render.image_settings.media_type = "VIDEO"
    scene.render.image_settings.file_format = "FFMPEG"
    scene.render.image_settings.color_mode = "RGB"
    scene.render.ffmpeg.format = "MPEG4"
    scene.render.ffmpeg.codec = "H264"
    scene.render.ffmpeg.constant_rate_factor = "HIGH"
    scene.render.ffmpeg.ffmpeg_preset = "GOOD"
    scene.render.ffmpeg.gopsize = fps * 2
    scene.render.ffmpeg.use_max_b_frames = False
    scene.render.ffmpeg.audio_codec = "NONE"
    scene.render.filepath = str(output)
    scene.render.use_file_extension = True
    bpy.ops.render.render(animation=True)


def validate_video(output: Path, fps: int, count: int, dimensions: tuple[int, int]) -> dict:
    if not output.is_file() or output.stat().st_size < 1000:
        raise RuntimeError(f"Expected MP4 was not created: {output}")
    result = subprocess.run([
        str(FFPROBE), "-v", "error", "-count_frames", "-show_streams", "-show_format",
        "-of", "json", str(output),
    ], capture_output=True, text=True, encoding="utf-8", check=True)
    metadata = json.loads(result.stdout)
    videos = [stream for stream in metadata["streams"] if stream.get("codec_type") == "video"]
    audios = [stream for stream in metadata["streams"] if stream.get("codec_type") == "audio"]
    if len(videos) != 1 or audios:
        raise RuntimeError("Expected exactly one video stream and no audio streams")
    video = videos[0]
    if video.get("codec_name") != "h264" or video.get("pix_fmt") != "yuv420p":
        raise RuntimeError("Expected H.264 yuv420p output")
    if (video.get("width"), video.get("height")) != dimensions:
        raise RuntimeError("Encoded resolution differs from source screenshot dimensions")
    if int(video.get("nb_read_frames", 0)) != count:
        raise RuntimeError("Decoded video frame count differs from planned playback frame count")
    if "nb_frames" in video and int(video["nb_frames"]) != count:
        raise RuntimeError("Container frame count differs from planned playback frame count")
    if Fraction(video["avg_frame_rate"]) != fps:
        raise RuntimeError("Encoded frame rate differs from requested playback frame rate")
    duration = float(metadata["format"]["duration"])
    if abs(duration - count / fps) > 0.002:
        raise RuntimeError(f"Encoded duration differs from planned playback duration: {duration}")
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frames", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fps", type=int, default=8)
    parser.add_argument("--timestamps", type=Path)
    parser.add_argument("--frame-column", default="frame_name")
    parser.add_argument("--timestamp-column", default="wall_time_s")
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    output = args.output.resolve()
    if not 1 <= args.fps <= 120:
        raise ValueError("Playback fps must be an integer from 1 to 120")
    if output.suffix.lower() != ".mp4":
        raise ValueError("Output filename must end in .mp4")
    output.parent.mkdir(parents=True, exist_ok=True)
    report_path = output.with_name(output.stem + "_encoding_report.json")
    report = {"status": "pending", "output": str(output), "frames_directory": str(args.frames.resolve()),
              "encoder": "Blender built-in FFmpeg H.264", "blender_version": bpy.app.version_string,
              "source": "Actual UE FScreenshotRequest PNG sequence; original frames are preserved.",
              "audio_streams": 0, "playback_fps": args.fps, "report_path": str(report_path)}
    try:
        if not FFPROBE.is_file():
            raise FileNotFoundError(f"Required output validation tool is missing: {FFPROBE}")
        frames, dimensions = read_frames(args.frames.resolve())
        playback, timing = playback_frames(frames, args)
        report.update(source_frames=len(frames), playback_frames=len(playback), width=dimensions[0],
                      height=dimensions[1], planned_duration_s=len(playback) / args.fps, timing=timing,
                      source_frame_names=[frame.name for frame in frames],
                      playback_source_frame_names=[frame.name for frame in playback])
        encode(playback, output, args.fps, dimensions)
        metadata = validate_video(output, args.fps, len(playback), dimensions)
        report.update(status="passed", bytes=output.stat().st_size, codec="h264", pixel_format="yuv420p",
                      duration_s=float(metadata["format"]["duration"]), ffprobe=metadata)
    except Exception:
        report.update(status="failed", error=traceback.format_exc())
        raise
    finally:
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("NAIWA_RUNTIME_VIDEO_ENCODE_OK", str(output), flush=True)


if __name__ == "__main__":
    main()
