"""Plot actual head/neck triangle-plane intersections and deformation masks.

Pillow is used because the bundled runtime has no matplotlib. All candidates
share engineering-coordinate bounds and equal axis scale; no shape fitting.
"""
import argparse
import csv
import json
from pathlib import Path

from PIL import Image, ImageDraw
from make_spherical_eye_comparisons import font, centered
from make_head_neck_comparisons import digest, read_json, LABELS, ORDER, ROOT, OUT
from plot_head_geometry_diagnostics import Panel

CMP = OUT / "comparisons"
HEIGHTS = (2.12, 2.22, 2.31, 2.40, 2.52, 2.60)
COLORS = {"A": "#61758b", "N1": "#248267", "N2": "#cb7c28", "C1": "#8050ae"}
MARGIN, CELL, GAP = 35, 680, 20


def title(canvas, text, subtitle):
    draw = ImageDraw.Draw(canvas)
    draw.rectangle((0, 0, canvas.width, 112), fill="#17263b")
    draw.text((MARGIN, 19), text, font=font(33), fill="white")
    draw.text((MARGIN, 65), subtitle, font=font(22), fill="#cbd6e6")
    return draw


def canvas(models, height):
    return Image.new("RGB", (2 * MARGIN + len(models) * CELL + (len(models) - 1) * GAP, height), "#f0f3f7")


def body(section):
    return section.get("body", section)


def trace(panel, section, axes, color, width=4):
    for segment in body(section)["segments"]:
        panel.line([(p[axes[0]], p[axes[1]]) for p in segment["points"]], color, width)


def midlines(models, raw):
    image = canvas(models, 970)
    draw = title(image, "下脸—颈部实际正中剖面 / X = 0", "保存网格精确三角面相交线；同工程坐标范围、同等比例轴；眼睛与高光不计入主体轮廓。")
    bounds = [-.53, 1.985, .35, 2.68]  # Y,Z, with +Y (back) to the right.
    for col, model in enumerate(models):
        x = MARGIN + col * (CELL + GAP)
        centered(draw, (x + CELL / 2, 132), LABELS[model], 25, "#17263b")
        panel = Panel(image, (x + 34, 213, x + CELL - 30, 712), bounds)
        panel.grid(.05, bounds)
        trace(panel, raw["A"]["midline"], (1, 2), "#c5ccd4", 7)
        trace(panel, raw[model]["midline"], (1, 2), COLORS[model], 4)
        for z in HEIGHTS:
            panel.line([(bounds[0], z), (bounds[2], z)], "#d3dce7", 1)
            _, y = panel.p(bounds[2], z)
            draw.text((x + CELL - 64, y - 23), f"{z:.2f}", font=font(16), fill="#68788c")
        panel.commit()
        centered(draw, (x + CELL / 2, 740), "← 前方 −Y      后方 +Y →", 23, "#43566d")
        draw.text((x + 40, 791), "灰色：固定 A\n彩色：本列实际剖面", font=font(23), fill="#43566d", spacing=6)
    draw.text((MARGIN, 917), "显示 Z = 1.985–2.680 范围。所有列保持 Y/Z 等比例；未按候选头型尺寸重新缩放。", font=font(22), fill="#43566d")
    image.save(CMP / "comparison_midline_sections.png")


def horizontal_at(record, height):
    return next(r for r in record["horizontal"] if abs(r["Z"] - height) < 1e-7)


def horizontal(models, raw):
    image = canvas(models, 3940)
    draw = title(image, "黄色头颈实际水平截面：六个固定高度", "保存网格精确相交线；各高度及候选使用同一 X/Y 工程尺度；前/后界采用 X=0 正中射线交点。")
    bounds = [-.43, -.49, .43, .33]
    for row, height in enumerate(HEIGHTS):
        y = 160 + row * 616
        for col, model in enumerate(models):
            x = MARGIN + col * (CELL + GAP)
            record = horizontal_at(raw[model], height)
            baseline = horizontal_at(raw["A"], height)
            draw.text((x + 35, y), f"{model}  Z = {height:.2f}", font=font(25), fill="#17263b")
            panel = Panel(image, (x + 45, y + 58, x + CELL - 45, y + 548), bounds)
            panel.grid(.05, bounds)
            trace(panel, baseline["body"], (0, 1), "#c5ccd4", 7)
            trace(panel, record["body"], (0, 1), COLORS[model], 4)
            panel.line([(0, bounds[1]), (0, bounds[3])], "#d1dbe7", 1)
            panel.point(0, record["center_Y"], "#405574")
            panel.commit()
            draw.text((x + 35, y + 560), f"前 {record['front_Y']:.5f}   后 {record['rear_Y']:.5f}\n深 {record['depth_Y']:.5f}   X宽 {record['width_X']:.5f}", font=font(21), fill="#43566d", spacing=4)
    draw.text((MARGIN, 3865), "每张小图向上为后方 +Y、向下为前方 −Y；灰线为固定 A，彩线为本列。蓝点为正中前后界的中心。", font=font(22), fill="#43566d")
    image.save(CMP / "comparison_horizontal_sections.png")


def measurements(models, raw, hashes):
    rows = []
    for height in HEIGHTS:
        baseline = horizontal_at(raw["A"], height)
        for model in models:
            record = horizontal_at(raw[model], height)
            row = {"model": model, "Z": height, "source_sha256": hashes[model], "units": "project world units", "pose": "raw saved mesh"}
            for key in ("front_Y", "rear_Y", "center_Y", "depth_Y", "width_X"):
                row[key] = record[key]
                row["delta_" + key + "_from_A"] = record[key] - baseline[key]
            rows.append(row)
    with (CMP / "section_measurements.csv").open("w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (CMP / "plotted_section_measurements.json").write_text(json.dumps(rows, indent=2) + "\n", encoding="utf-8")
    image = Image.new("RGB", (2480, 260 + 54 * len(rows)), "#f0f3f7")
    draw = title(image, "实际截面测量表：位置、深度与横向宽度", "固定 A 对照；前界 f、后界 r 为 X=0 射线交点，截面中心=(f+r)/2；X 宽度来自完整横截面。")
    columns = [("方案/Z", None, 30), ("前界 Y", "front_Y", 320), ("后界 Y", "rear_Y", 580), ("中心 Y", "center_Y", 840), ("深度", "depth_Y", 1100), ("X 宽度", "width_X", 1360), ("Δ前界", "delta_front_Y_from_A", 1620), ("Δ后界", "delta_rear_Y_from_A", 1880), ("Δ中心", "delta_center_Y_from_A", 2160)]
    for label, key, x in columns:
        draw.text((x, 130), label, font=font(23), fill="#43566d")
    for index, row in enumerate(rows):
        y = 178 + 54 * index
        if index % 2 == 0:
            draw.rectangle((20, y - 5, image.width - 20, y + 43), fill="white")
        for label, key, x in columns:
            value = f"{row['model']} / {row['Z']:.2f}" if key is None else (f"{row[key]:+.6f}" if key.startswith("delta_") else f"{row[key]:.6f}")
            draw.text((x, y), value, font=font(23), fill="#17263b")
    draw.text((30, image.height - 49), "完整精度及源工程 SHA256 保留在 section_measurements.csv；本表不是参考图片的三维尺寸恢复。", font=font(22), fill="#43566d")
    image.save(CMP / "section_measurements.png")


def masks(models):
    import numpy as np
    candidates = [model for model in models if model != "A"]
    image = canvas(candidates, 1550)
    draw = title(image, "实际位移与保护区掩码：源顶点侧面投影", "图示生成掩码仅解释作用范围；固定坐标是否保持由独立网格验收决定。所有候选用相同工程坐标及位移颜色量尺。")
    bounds = [-.53, 1.90, .35, 2.68]
    for col, model in enumerate(candidates):
        path = OUT / f"candidate_{model}/deformation_mask.npz"
        with np.load(path, allow_pickle=False) as data:
            points = data["points_world"]
            values = data["delta_Y"]
            region = data["region_mask"]
            guards = {key: data[key] for key in ("shoulder_guard", "eye_guard", "mouth_guard", "height_guard")}
        x = MARGIN + col * (CELL + GAP)
        centered(draw, (x + CELL / 2, 132), model, 26, "#17263b")
        selected = np.flatnonzero((points[:, 2] >= bounds[1]) & (points[:, 2] <= bounds[3]))
        selected = selected[np.argsort(points[selected, 0])]
        for mode, y in (("displacement", 206), ("guards", 876)):
            panel = Panel(image, (x + 30, y, x + CELL - 30, y + 560), bounds)
            panel.grid(.05, bounds)
            for index in selected:
                if mode == "displacement":
                    value = float(values[index])
                    t = min(abs(value) / .08, 1)
                    start, end = ((174, 185, 197), (217, 92, 26)) if value >= 0 else ((174, 185, 197), (29, 118, 193))
                    color = tuple(round(a + (b - a) * t) for a, b in zip(start, end))
                else:
                    if guards["eye_guard"][index] < 1e-8 or guards["mouth_guard"][index] < 1e-8:
                        color = "#319275"
                    elif guards["shoulder_guard"][index] < 1e-8:
                        color = "#8755a2"
                    elif guards["height_guard"][index] < 1e-8:
                        color = "#bbc4cf"
                    elif region[index] < .999:
                        color = "#3b9aba"
                    else:
                        color = "#d48c35"
                px, py = panel.lp(points[index, 1], points[index, 2])
                panel.draw.ellipse((px - 1.3, py - 1.3, px + 1.3, py + 1.3), fill=color)
            panel.commit()
            draw.text((x + 35, y - 40), "实际保存 ΔY：橙 +Y，蓝 −Y，灰 0" if mode == "displacement" else "固定与羽化：肩根 / 五官 / 高度边界", font=font(21), fill="#43566d")
    draw.text((MARGIN, 1480), "下排：紫=固定肩根，绿=固定眼窝/口周，灰=固定高度区，蓝=平滑羽化，橙=主作用区。C1 源点为所选颈部版本。", font=font(22), fill="#43566d")
    image.save(CMP / "deformation_masks.png")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--models", nargs="+", choices=ORDER, default=["A", "N1", "N2"])
    parser.add_argument("--skip-masks", action="store_true")
    args = parser.parse_args()
    models = [model for model in ORDER if model in args.models]
    raw = read_json(OUT / "section_raw.json")
    for model in models:
        source = ROOT / "versions/20261006_spherical_eyes/candidate_A/Naiwa_Character.blend" if model == "A" else OUT / f"candidate_{model}/Naiwa_Character.blend"
        if digest(source) != raw["source_hashes"][model]:
            raise ValueError(f"Sections do not match current {model} blend")
    CMP.mkdir(exist_ok=True)
    midlines(models, raw["models"])
    horizontal(models, raw["models"])
    measurements(models, raw["models"], raw["source_hashes"])
    if not args.skip_masks:
        masks(models)
    print("HEAD_NECK_SECTION_PLOTS_OK", models)


if __name__ == "__main__":
    main()
