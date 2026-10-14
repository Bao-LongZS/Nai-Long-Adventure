"""Assemble fixed-camera renders and export measured eye data.

Run with ordinary Python (Pillow); never crops or normalizes by eye diameter.
Original render files are inputs and are not changed.
"""
import csv
import hashlib
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
VERSION = ROOT / "versions/20261006_spherical_eyes"
OUT = VERSION / "comparisons"
MODELS = ("original", "A", "B")
LABELS = {"original": "Original / 原版", "A": "Candidate A / 候选 A", "B": "Candidate B / 候选 B"}
SOURCE = {
    "original": ROOT / "versions/20261005_head_side_continuity/Naiwa_Character.blend",
    "A": VERSION / "candidate_A/Naiwa_Character.blend",
    "B": VERSION / "candidate_B/Naiwa_Character.blend",
}
D0 = 0.11724481274201151
S0 = 0.23462580179330159
Z0 = 2.505265415494


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def font(size):
    for path in ("C:/Windows/Fonts/msyh.ttc", "C:/Windows/Fonts/arial.ttf"):
        if Path(path).exists():
            return ImageFont.truetype(path, size)
    return ImageFont.load_default(size=size)


def centered(draw, xy, text, size, fill):
    f = font(size)
    bounds = draw.textbbox((0, 0), text, font=f)
    draw.text((xy[0] - (bounds[2] - bounds[0]) / 2, xy[1]), text, font=f, fill=fill)


def verify_sources(protocol):
    hashes = {}
    for label, path in SOURCE.items():
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != protocol["source_hashes"][label]:
            raise ValueError(f"{label} render is stale: saved blend hash no longer matches")
        hashes[label] = actual
    # Camera, light, exposure come from one rendering protocol; enforce the
    # camera equality across columns for every image category and view.
    grouped = {}
    camera_keys = ("yaw", "pitch", "target", "location", "ortho_scale", "resolution")
    for record in protocol["files"]:
        key = (record["kind"], record["gray"], record["yaw"])
        camera = {k: record[k] for k in camera_keys}
        if key in grouped and camera != grouped[key]:
            raise ValueError(f"Camera differs across models for {key}")
        grouped[key] = camera
    return hashes


def assemble(kind, gray, title, destination, protocol):
    views = ("front",) if kind == "full" else ("front", "three_quarter", "side")
    row_labels = ("0° 正面",) if kind == "full" else ("0° 正面", "45° 斜侧", "90° 侧面")
    prefix = kind + ("_gray" if gray else "")
    raw_size = (800, 800) if kind == "eye" else (900, 1000)
    cell_w = 720
    cell_h = round(raw_size[1] * cell_w / raw_size[0])
    left, gutter, header, row_bar, footer = 36, 20, 170, 46, 100
    width = left * 2 + 3 * cell_w + 2 * gutter
    height = header + len(views) * (row_bar + cell_h + gutter) + footer
    canvas = Image.new("RGB", (width, height), "#f0f3f7")
    draw = ImageDraw.Draw(canvas)
    draw.rectangle((0, 0, width, 105), fill="#17263b")
    draw.text((left, 19), title, font=font(34), fill="white")
    scale = 0.295 if kind == "eye" else (3.45 if kind == "full" else 0.85)
    draw.text((left, 64), f"同相机 / 同灯光 / 同曝光  |  正交尺度 {scale:g}  |  Idle 第 1 帧", font=font(22), fill="#cbd6e6")
    for col, model in enumerate(MODELS):
        x = left + col * (cell_w + gutter)
        centered(draw, (x + cell_w / 2, 123), LABELS[model], 27, "#17263b")
    used = []
    for row, (view, row_label) in enumerate(zip(views, row_labels)):
        top = header + row * (row_bar + cell_h + gutter)
        draw.text((left, top + 5), row_label, font=font(25), fill="#43566d")
        for col, model in enumerate(MODELS):
            filename = f"{model}_{prefix}_{view}.png"
            with Image.open(OUT / filename) as raw:
                if raw.size != raw_size:
                    raise ValueError(f"Unexpected size: {filename} = {raw.size}")
                tile = raw.convert("RGB").resize((cell_w, cell_h), Image.Resampling.LANCZOS)
            x = left + col * (cell_w + gutter)
            canvas.paste(tile, (x, top + row_bar))
            used.append(filename)
    bottom = height - footer + 9
    draw.text((left, bottom), "完整画幅统一缩放；保留工程尺度和原始相机，没有按各候选眼径重新放大。", font=font(22), fill="#43566d")
    draw.text((left, bottom + 35), "眼部近景、头部图和全身图分别使用固定尺度；仅比较同一张表内的三列。", font=font(20), fill="#68788c")
    canvas.save(OUT / destination)
    return {"output": destination, "inputs": used, "raw_size": raw_size, "tile_size": [cell_w, cell_h], "uniform_resizing": True, "crop": None}


def measurement_rows():
    validation = read_json(VERSION / "spherical_eyes_validation.json")
    rows = []
    for model in MODELS:
        build = None if model == "original" else read_json(VERSION / f"candidate_{model}/spherical_eye_build_report.json")
        report = None if model == "original" else read_json(VERSION / f"candidate_{model}/validation_report.json")
        measurements = validation["source_measurements"] if model == "original" else {s: report["eyes"][s]["frontal_metrics"] for s in ("L", "R")}
        spacing = measurements["R"]["aperture"]["center"][0] - measurements["L"]["aperture"]["center"][0]
        for side in ("L", "R"):
            metrics = measurements[side]
            center = metrics["aperture"]["center"]
            diameter = 2 * math.sqrt(metrics["aperture"]["area"] / math.pi)
            pupil_ratio = math.sqrt(metrics["black"]["area"] / metrics["aperture"]["area"])
            row = {
                "model": model, "side": side, "D0": D0, "S0": S0,
                "visible_area_green_including_pupil": metrics["aperture"]["area"],
                "visible_D_equivalent": diameter,
                "D_error_percent": (diameter / D0 - 1) * 100,
                "visible_centroid_X": center[0], "visible_centroid_Z": center[1],
                "Z_shift_from_fixed_Z0": center[1] - Z0,
                "Z_shift_over_D0": (center[1] - Z0) / D0,
                "horizontal_center_spacing": spacing,
                "spacing_error_percent": (spacing / S0 - 1) * 100,
                "visible_pupil_area": metrics["black"]["area"],
                "visible_pupil_D_over_visible_eye_D": pupil_ratio,
                "frontal_outline_axis_ratio": metrics["aperture"]["axis_ratio"],
                "maximum_visible_gap_in_global_F": metrics["visible_surface_gap"]["maximum"],
                "raw_sampling_step": metrics["sampling"]["step"],
                "R_actual": "", "h_local_normal_actual": "", "R_over_D0": "", "h_over_D0": "",
                "C_X": "", "C_Y": "", "C_Z": "",
                "P_X": "", "P_Y": "", "P_Z": "",
                "normal_X": "", "normal_Y": "", "normal_Z": "",
                "maximum_relative_radius_residual_raw": "",
                "maximum_relative_radius_residual_Idle_frame1": "",
                "source_sha256": hashlib.sha256(SOURCE[model].read_bytes()).hexdigest(),
                "measurement_pose": "Idle frame 1", "units": "project units",
            }
            if build:
                eye = build["eyes"][side]
                fit = report["eyes"][side]
                row.update({"R_actual": eye["R"], "h_local_normal_actual": eye["h"], "R_over_D0": eye["R"] / D0, "h_over_D0": eye["h"] / D0})
                for prefix, key in (("C", "sphere_center"), ("P", "install_P"), ("normal", "install_normal")):
                    row.update({f"{prefix}_{axis}": value for axis, value in zip("XYZ", eye[key])})
                row["maximum_relative_radius_residual_raw"] = fit["raw_world_fit"]["maximum_relative_radius_residual"]
                row["maximum_relative_radius_residual_Idle_frame1"] = fit["Idle_frame1_evaluated_world_fit"]["maximum_relative_radius_residual"]
            rows.append(row)
    return rows


def render_measurement_table(rows):
    width, height = 2232, 660
    canvas = Image.new("RGB", (width, height), "#f0f3f7")
    draw = ImageDraw.Draw(canvas)
    draw.rectangle((0, 0, width, 95), fill="#17263b")
    draw.text((35, 22), "实际测量 / Actual measurements", font=font(32), fill="white")
    columns = (("方案/侧", "model_side", 40), ("眼径 D", "visible_D_equivalent", 270), ("D 误差", "D_error_percent", 550), ("瞳径 / 眼径", "visible_pupil_D_over_visible_eye_D", 795), ("眼心 Z", "visible_centroid_Z", 1110), ("实际 R", "R_actual", 1400), ("实际 h (n)", "h_local_normal_actual", 1690), ("球面残差", "maximum_relative_radius_residual_raw", 1980))
    for label, key, x in columns:
        draw.text((x, 115), label, font=font(24), fill="#43566d")
    for index, row in enumerate(rows):
        y = 168 + index * 58
        if index % 2 == 0:
            draw.rectangle((26, y - 7, width - 26, y + 46), fill="white")
        for label, key, x in columns:
            value = f"{row['model']} {row['side']}" if key == "model_side" else row[key]
            if key != "model_side":
                value = "—" if value == "" else (f"{value:+.3f}%" if key == "D_error_percent" else (f"{value:.2e}" if "residual" in key else f"{value:.6f}"))
            draw.text((x, y), value, font=font(23), fill="#17263b")
    draw.text((35, 545), "D = 2√(可见外缘面积/π)，包含黑瞳、排除高光；按保存工程在 Idle 第 1 帧重测。", font=font(22), fill="#43566d")
    draw.text((35, 583), "h 沿安装法线 n 量取；原版不具备单一球心与 R / h。完整参数与中心距见 eye_measurements.csv。", font=font(22), fill="#43566d")
    canvas.save(OUT / "eye_measurements.png")


def main():
    protocol = read_json(OUT / "camera_protocol.json")
    hashes = verify_sources(protocol)
    panels = []
    for kind, gray, title, filename in (
        ("head", False, "头部彩色：固定尺度对比", "comparison_head.png"),
        ("head", True, "头部灰模：固定尺度对比", "comparison_head_gray.png"),
        ("eye", False, "眼部近景：固定尺度对比", "comparison_eye.png"),
        ("eye", True, "眼部灰模：固定尺度对比", "comparison_eye_gray.png"),
        ("full", False, "全身正面：固定尺度对比", "comparison_full.png"),
    ):
        panels.append(assemble(kind, gray, title, filename, protocol))
    rows = measurement_rows()
    with (OUT / "eye_measurements.csv").open("w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    render_measurement_table(rows)
    manifest = {"source_hashes_verified": hashes, "camera_equality_verified": True, "per_candidate_eye_normalization": False, "panels": panels, "measurement_definition": "2*sqrt(visible aperture area/pi); aperture includes black pupil and excludes independent glints", "sampling_step": 0.0003}
    (OUT / "comparison_assembly_protocol.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print("SPHERICAL_EYE_COMPARISONS_OK")


if __name__ == "__main__":
    main()
