"""Prepare source-preserving P1 feedback assets for Unreal import.

The WAV normalizer only repairs RIFF chunk lengths in local copies; it does not
change PCM sample data. Death-animation frames come from the selected looping
video and have only its plain gray background keyed out.
"""

import hashlib
import json
import struct
import wave
from datetime import datetime, timezone
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "UE5/SourceAssets/P1_Feedback"
AUDIO = SOURCE / "Audio"
IMAGES = SOURCE / "Images"
FRAMES = SOURCE / "DeathLaughFrames"
VIDEO_FRAMES = SOURCE / "DeathLaughVideoFramesRaw"
MANIFEST = SOURCE / "feedback_source_manifest.json"
VIDEO_URL = "https://www.bilibili.com/video/BV1xsRSBQENA/"
VIDEO_START_SECONDS = 2.0
VIDEO_FPS = 24
VIDEO_FRAME_COUNT = 96
VIDEO_LOOP_DURATION_MS = 4000


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def normalize_wave(path: Path) -> dict:
    data = bytearray(path.read_bytes())
    if len(data) < 44 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"Not a RIFF/WAVE file: {path.name}")

    struct.pack_into("<I", data, 4, len(data) - 8)
    position = 12
    fmt = None
    data_offset = None
    while position + 8 <= len(data):
        chunk_id = bytes(data[position : position + 4])
        chunk_size = struct.unpack_from("<I", data, position + 4)[0]
        payload_start = position + 8
        payload_end = payload_start + chunk_size
        if chunk_id == b"fmt ":
            fmt = bytes(data[payload_start : payload_start + min(chunk_size, 16)])
        if chunk_id == b"data":
            data_offset = payload_start
            # The supplied BGM files use the streaming sentinel 0xffffffff.
            # Their data chunk is last, so the exact payload length is the rest
            # of this file. Other clips are normalized to the same invariant.
            actual_size = len(data) - payload_start
            struct.pack_into("<I", data, position + 4, actual_size)
            break
        if payload_end > len(data):
            raise ValueError(f"Truncated chunk {chunk_id!r} in {path.name}")
        position = payload_end + (chunk_size & 1)

    if data_offset is None or fmt is None:
        raise ValueError(f"Missing fmt/data chunk in {path.name}")
    format_tag, channels, sample_rate, byte_rate, block_align, bits = struct.unpack(
        "<HHIIHH", fmt
    )
    if format_tag != 1 or not byte_rate or not channels or not sample_rate:
        raise ValueError(f"Unsupported WAV format in {path.name}")
    payload_size = len(data) - data_offset
    if payload_size % block_align:
        raise ValueError(f"WAV payload is not frame aligned in {path.name}")

    original_hash = sha256(path)
    path.write_bytes(data)
    with wave.open(str(path), "rb") as stream:
        duration = stream.getnframes() / stream.getframerate()
    return {
        "name": path.name,
        "source_copy_sha256_before_header_repair": original_hash,
        "normalized_copy_sha256": sha256(path),
        "pcm_payload_sha256": hashlib.sha256(data[data_offset:]).hexdigest(),
        "pcm_payload_bytes": payload_size,
        "sample_rate_hz": sample_rate,
        "channels": channels,
        "bits_per_sample": bits,
        "duration_seconds": round(duration, 3),
        "header_repair": "RIFF_size_and_data_chunk_size_only",
    }


def prepare_video_frames() -> dict:
    source_frames = sorted(VIDEO_FRAMES.glob("frame_*.png"))
    if len(source_frames) != VIDEO_FRAME_COUNT:
        raise AssertionError(
            f"Expected {VIDEO_FRAME_COUNT} sampled video frames in {VIDEO_FRAMES}, found {len(source_frames)}"
        )
    FRAMES.mkdir(parents=True, exist_ok=True)
    frames = []
    size = None
    for index, source_path in enumerate(source_frames):
        original = Image.open(source_path).convert("RGB")
        keyed = original.copy()
        ImageDraw.floodfill(keyed, (0, 0), (0, 0, 0), thresh=35)
        changed = ImageChops.difference(keyed, original).convert("L")
        alpha = changed.point(lambda value: 0 if value else 255)
        result = original.convert("RGBA")
        result.putalpha(alpha)
        frame_path = FRAMES / f"T_P1_DeathLaugh_{index:02d}.png"
        result.save(frame_path, format="PNG")
        if size is None:
            size = result.size
        frames.append({
            "index": index,
            "path": str(frame_path.relative_to(ROOT)).replace("\\", "/"),
            "duration_ms": round(1000 / VIDEO_FPS),
            "source_sha256": sha256(source_path),
            "sha256": sha256(frame_path),
        })
    return {
        "source": str(VIDEO_FRAMES.relative_to(ROOT)).replace("\\", "/"),
        "source_url": VIDEO_URL,
        "source_start_seconds": VIDEO_START_SECONDS,
        "source_fps": VIDEO_FPS,
        "width": size[0],
        "height": size[1],
        "frame_count": len(frames),
        "duration_ms": VIDEO_LOOP_DURATION_MS,
        "looping": True,
        "background_key": "connected neutral-gray background from top-left; floodfill tolerance 35",
        "frames": frames,
    }


def main() -> None:
    audio_names = (
        "Background_Music_1.wav", "Background_Music_2.wav",
        "Collection.wav", "Collection_1.wav", "Collection_2.wav",
        "Die.wav", "Die_1.wav", "Die_3.wav", "Jump.wav",
        "哎哟我去.wav", "哟嚯嚯嚯.wav",
    )
    audio = [normalize_wave(AUDIO / name) for name in audio_names]
    frames = prepare_video_frames()
    manifest = {
        "schema": "NaiwaP1FeedbackSourceV1",
        "prepared_utc": datetime.now(timezone.utc).isoformat(),
        "source_audio_directory": "UE5/SourceAssets/P1_Feedback/Audio",
        "audio": audio,
        "collection_images": [
            {"name": name, "sha256": sha256(IMAGES / name)}
            for name in ("collection1.png", "collection2.png")
        ],
        "death_laugh": frames,
    }
    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({
        "status": "prepared",
        "audio_count": len(audio),
        "video_frames": frames["frame_count"],
        "video_loop_duration_ms": frames["duration_ms"],
        "manifest": str(MANIFEST),
    }, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
