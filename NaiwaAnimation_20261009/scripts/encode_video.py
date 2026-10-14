"""Encode a rendered PNG sequence with Blender's built-in H.264 encoder.

Blender --background --factory-startup --python-exit-code 1 --python
  encode_video.py -- --frames ../frames --output ../Naiwa_10s.mp4
"""
from __future__ import annotations

import argparse
from fractions import Fraction
import json
import math
from pathlib import Path
import struct
import subprocess
import sys

import bpy


WIDTH, HEIGHT = 1920, 1080
FFPROBE = Path(r"C:\Program Files\Tencent\QQBrowser\22.0.7003.400\ffmpeg\ffprobe.exe")


def png_dimensions(path):
    with path.open("rb") as handle:
        header = handle.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"Not a valid PNG header: {path}")
    return struct.unpack(">II", header[16:24])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frames", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fps", type=int, default=24)
    parser.add_argument("--duration", type=float, default=10.0)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    if args.fps <= 0 or not math.isfinite(args.duration) or args.duration <= 0:
        raise ValueError("fps and duration must be positive finite values")
    frame_count = round(args.fps * args.duration)
    if frame_count < 1 or not math.isclose(frame_count / args.fps, args.duration, abs_tol=1e-9):
        raise ValueError("duration must be an exact whole number of frames")
    frame_dir = args.frames.resolve()
    output = args.output.resolve()
    if not frame_dir.is_dir():
        raise FileNotFoundError(frame_dir)
    if output.suffix.lower() != ".mp4":
        raise ValueError("output must end in .mp4")
    frames = [frame_dir / f"{frame:04d}.png" for frame in range(1, frame_count + 1)]
    actual_frames = {path.name for path in frame_dir.glob("*.png")}
    expected_frames = {path.name for path in frames}
    if actual_frames != expected_frames:
        missing = sorted(expected_frames - actual_frames)
        unexpected = sorted(actual_frames - expected_frames)
        raise ValueError(f"Expected exactly {frame_count} numbered PNG frames; "
                         f"missing={missing[:10]}, unexpected={unexpected[:10]}")
    for frame in frames:
        if png_dimensions(frame) != (WIDTH, HEIGHT):
            raise ValueError(f"Frame must be {WIDTH}x{HEIGHT}: {frame}")
    if not FFPROBE.is_file():
        raise FileNotFoundError(f"Required validation tool is missing: {FFPROBE}")
    output.parent.mkdir(parents=True, exist_ok=True)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.frame_start, scene.frame_end = 1, frame_count
    scene.render.fps = args.fps
    scene.render.fps_base = 1.0
    scene.render.resolution_x, scene.render.resolution_y = WIDTH, HEIGHT
    scene.render.resolution_percentage = 100
    scene.render.pixel_aspect_x = scene.render.pixel_aspect_y = 1.0
    scene.render.use_sequencer = True
    scene.render.use_compositing = False
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0
    editor = scene.sequence_editor_create()
    strips = editor.strips if hasattr(editor, "strips") else editor.sequences
    strip = strips.new_image("Naiwa animation frames", str(frames[0]), channel=1, frame_start=1)
    for frame in frames[1:]:
        strip.elements.append(frame.name)
    strip.blend_type = "REPLACE"
    strip.frame_final_duration = frame_count
    strip.colorspace_settings.name = "sRGB"

    if hasattr(scene.render.image_settings, "media_type"):
        scene.render.image_settings.media_type = "VIDEO"
    scene.render.image_settings.file_format = "FFMPEG"
    scene.render.image_settings.color_mode = "RGB"
    scene.render.ffmpeg.format = "MPEG4"
    scene.render.ffmpeg.codec = "H264"
    scene.render.ffmpeg.constant_rate_factor = "HIGH"
    scene.render.ffmpeg.ffmpeg_preset = "GOOD"
    scene.render.ffmpeg.gopsize = args.fps * 2
    scene.render.ffmpeg.use_max_b_frames = False
    scene.render.ffmpeg.audio_codec = "NONE"
    scene.render.filepath = str(output)
    scene.render.use_file_extension = True
    bpy.ops.render.render(animation=True)
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
        raise RuntimeError("Expected H.264 video with yuv420p pixels")
    if (video.get("width"), video.get("height")) != (WIDTH, HEIGHT):
        raise RuntimeError("Encoded dimensions differ from rendered frames")
    if int(video.get("nb_read_frames", 0)) != frame_count:
        raise RuntimeError("Decoded frame count differs from source PNG count")
    if int(video.get("nb_frames", frame_count)) != frame_count:
        raise RuntimeError("Container frame count differs from source PNG count")
    if Fraction(video["avg_frame_rate"]) != args.fps:
        raise RuntimeError("Encoded frame rate differs from requested frame rate")
    actual_duration = float(metadata["format"]["duration"])
    if abs(actual_duration - args.duration) > 0.001:
        raise RuntimeError(f"Encoded duration differs from requested duration: {actual_duration}")
    report = {
        "status": "passed", "output": str(output), "frames_directory": str(frame_dir),
        "frames": frame_count, "fps": args.fps, "duration_seconds": actual_duration,
        "width": WIDTH, "height": HEIGHT, "codec": "h264", "pixel_format": "yuv420p",
        "audio_streams": 0, "bytes": output.stat().st_size,
        "encoder": "Blender built-in FFmpeg H.264", "ffprobe": metadata,
    }
    records = output.parent / "records"
    records.mkdir(parents=True, exist_ok=True)
    (records / "video_validation.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("NAIWA_VIDEO_ENCODE_OK", str(output), flush=True)


if __name__ == "__main__":
    main()
