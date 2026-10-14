"""Plot saved-mesh sections with fixed engineering scale using bundled Pillow.

Every trace comes from triangle/plane intersections, not an illustrated shape.
The same bounds and equal X/Y pixel scale are used in all comparison columns.
"""
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw
from make_spherical_eye_comparisons import font, centered, verify_sources

ROOT = Path(__file__).resolve().parent
VERSION = ROOT / "versions/20261006_spherical_eyes"
OUT = VERSION / "comparisons"
COLORS = {"green": "#398d35", "black": "#1d2427"}
WIDTH, LEFT, CELL, GUTTER = 2272, 36, 720, 20


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def body_front_f(z, body):
    crossings = []
    for segment in body["segments"]:
        p, q = segment["points"]
        if min(p[2], q[2]) <= z <= max(p[2], q[2]) and abs(q[2] - p[2]) > 1e-12:
            t = (z - p[2]) / (q[2] - p[2])
            crossings.append(-(p[1] + t * (q[1] - p[1])))
    return max(crossings) if crossings else -float("inf")


def dashed_line(draw, p, q, color, width=3):
    length = math.hypot(q[0] - p[0], q[1] - p[1])
    if length == 0:
        return
    # A stable plot-space phase keeps dashes continuous across short triangle
    # intersections, rather than restarting a dash for every tiny segment.
    intervals = max(1, math.ceil(length / 1.5))
    for index in range(intervals):
        t1, t2 = index / intervals, (index + 1) / intervals
        middle = tuple(p[i] + (q[i] - p[i]) * (t1 + t2) / 2 for i in (0, 1))
        if math.floor((middle[0] * .37 + middle[1] * .93) / 9) % 2:
            continue
        a = tuple(p[i] + (q[i] - p[i]) * t1 for i in (0, 1))
        b = tuple(p[i] + (q[i] - p[i]) * t2 for i in (0, 1))
        draw.line((a, b), fill=color, width=width)


class Plot:
    def __init__(self, canvas, x, y, bounds, pixel_scale, xticks, yticks, labels):
        self.canvas, self.x, self.y = canvas, x, y
        self.xmin, self.xmax, self.ymin, self.ymax = bounds
        self.scale = pixel_scale
        self.w = round((self.xmax - self.xmin) * self.scale)
        self.h = round((self.ymax - self.ymin) * self.scale)
        self.layer = Image.new("RGB", (self.w, self.h), "white")
        self.draw = ImageDraw.Draw(self.layer)
        base = ImageDraw.Draw(canvas)
        for tick in xticks:
            px, py = self.map((tick, self.ymin))
            self.draw.line((px, 0, px, self.h), fill="#e8edf2", width=2)
            centered(base, (x + px, y + self.h + 10), f"{tick:g}", 20, "#58697e")
        for tick in yticks:
            px, py = self.map((self.xmin, tick))
            self.draw.line((0, py, self.w, py), fill="#e8edf2", width=2)
            base.text((x - 65, y + py - 13), f"{tick:g}", font=font(20), fill="#58697e")
        centered(base, (x + self.w / 2, y + self.h + 46), labels[0], 22, "#43566d")
        base.text((x - 62, y - 36), labels[1], font=font(22), fill="#43566d")

    def map(self, xy):
        return ((xy[0] - self.xmin) * self.scale, (self.ymax - xy[1]) * self.scale)

    def line(self, points, color, width=4, dashed=False):
        coords = [self.map(point) for point in points]
        if dashed:
            for p, q in zip(coords, coords[1:]):
                dashed_line(self.draw, p, q, color, width)
        else:
            self.draw.line(coords, fill=color, width=width)

    def finish(self):
        self.draw.rectangle((0, 0, self.w - 1, self.h - 1), outline="#afbccc", width=2)
        self.canvas.paste(self.layer, (self.x, self.y))


def header(canvas, title, subtitle):
    draw = ImageDraw.Draw(canvas)
    draw.rectangle((0, 0, canvas.width, 110), fill="#17263b")
    draw.text((LEFT, 18), title, font=font(34), fill="white")
    draw.text((LEFT, 65), subtitle, font=font(22), fill="#cbd6e6")


def side_panels():
    raw = read_json(OUT / "eye_side_sections_raw.json")
    canvas = Image.new("RGB", (WIDTH, 1140), "#f0f3f7")
    header(canvas, "新旧眼体侧剖面：固定 X 平面、固定工程尺度", f"Idle 第 1 帧  |  右眼 X = {raw['original']['plane_X']:.12f}  |  独立高光已排除")
    draw = ImageDraw.Draw(canvas)
    for col, model in enumerate(("original", "A", "B")):
        data = raw[model]
        x = LEFT + col * (CELL + GUTTER)
        centered(draw, (x + CELL / 2, 135), "Original / 原版" if model == "original" else f"Candidate {model} / 候选 {model}", 28, "#17263b")
        plot = Plot(canvas, x + 65, 218, (.205, .455, 2.395, 2.605), 2520, (.25, .30, .35, .40, .45), (2.40, 2.45, 2.50, 2.55, 2.60), ("F = −Y → 前方", "Z"))
        for loop in data["body"]["loops"]:
            coords = [plot.map((-p[1], p[2])) for p in loop["points3d"]]
            plot.draw.polygon(coords, fill="#fff3ca")
        for tick in (.25, .30, .35, .40, .45):
            px, _ = plot.map((tick, 2.395))
            plot.draw.line((px, 0, px, plot.h), fill="#e8edf2", width=1)
        for tick in (2.40, 2.45, 2.50, 2.55, 2.60):
            _, py = plot.map((.205, tick))
            plot.draw.line((0, py, plot.w, py), fill="#e8edf2", width=1)
        for segment in data["body"]["segments"]:
            plot.line([(-p[1], p[2]) for p in segment["points"]], "#c19521", 5)
        for segment in data["eye"]["segments"]:
            p, q = segment["points"]
            z, f = (p[2] + q[2]) / 2, -(p[1] + q[1]) / 2
            hidden = f < body_front_f(z, data["body"]) - 1e-6
            color = COLORS.get(segment["region"], COLORS["green"])
            if hidden:
                color = "#99ba92" if segment["region"] == "green" else "#97a0a4"
            plot.line([(-p[1], p[2]), (-q[1], q[2])], color, 3 if hidden else 6, hidden)
        if model != "original":
            report = read_json(VERSION / f"candidate_{model}/spherical_eye_build_report.json")["eyes"]["R"]
            c = report["sphere_center"]
            cx, cy = plot.map((-c[1], c[2]))
            plot.draw.line((cx - 8, cy, cx + 8, cy), fill="#38567a", width=3)
            plot.draw.line((cx, cy - 8, cx, cy + 8), fill="#38567a", width=3)
            plot.draw.text((cx + 12, cy + 2), "C", font=font(24), fill="#38567a")
            draw.text((x + 70, 840), f"实际 R = {report['R']:.6f}\n实际 h(n) = {report['h']:.6f}", font=font(23), fill="#43566d", spacing=6)
            draw.text((x + 70, 916), "C 为球心投影；切片 X 保持相同。", font=font(20), fill="#68788c")
        else:
            draw.text((x + 70, 840), "旧眼后部封背埋入主体；\n它的厚度不能视作可见凸出。", font=font(23), fill="#43566d", spacing=6)
        plot.finish()
    draw.text((LEFT, 1006), "金色：黄色主体截面；实线：露出眼面；浅色虚线：被主体遮挡的后部。", font=font(25), fill="#43566d")
    draw.text((LEFT, 1055), "h 沿局部安装法线量取；剖面中的全局 F 向距离与 h 不可混用。三列使用等比例坐标轴，没有按眼径归一化。", font=font(23), fill="#43566d")
    canvas.save(OUT / "eye_side_sections.png")


def center_panels():
    canvas = Image.new("RGB", (WIDTH, 1670), "#f0f3f7")
    header(canvas, "球心正交三剖面：保存网格与拟合圆", "右眼 A / B  |  三个相互正交平面均经过各自球心  |  坐标平移至球心，保留工程单位")
    draw = ImageDraw.Draw(canvas)
    axis_names = {"X": (1, 2), "Y": (0, 2), "Z": (0, 1)}
    for row, model in enumerate(("A", "B")):
        records = read_json(VERSION / f"candidate_{model}/sphere_center_sections.json")
        for col, axis in enumerate("XYZ"):
            x = LEFT + col * (CELL + GUTTER)
            y = 218 + row * 710
            record = next(r for r in records if r["side"] == "R" and r["plane_axis"] == axis)
            a, b = axis_names[axis]
            center, radius = record["C"], record["R"]
            centered(draw, (x + CELL / 2, y - 74), f"{model}：经过 C，法向 {axis}", 28, "#17263b")
            plot = Plot(canvas, x + 75, y, (-.081, .081, -.081, .081), 3300, (-.08, -.04, 0, .04, .08), (-.08, -.04, 0, .04, .08), (f"{'XYZ'[a]} − C{'XYZ'[a]}", f"{'XYZ'[b]} − C{'XYZ'[b]}"))
            # Pillow's ellipse stroke runs inward; enlarge by half its width
            # so the gray line is centered on the actual fitted circle.
            stroke_radius = radius + 5 / plot.scale
            bbox = [plot.map((-stroke_radius, stroke_radius)), plot.map((stroke_radius, -stroke_radius))]
            plot.draw.ellipse(bbox, outline="#ccd4df", width=10)
            for segment in record["object"]["segments"]:
                coords = [(p[a] - center[a], p[b] - center[b]) for p in segment["points"]]
                plot.line(coords, COLORS.get(segment["region"], COLORS["green"]), 4)
            plot.finish()
    draw.text((LEFT, 1585), "浅灰：拟合圆；黑/绿：实际三角网格截面。所有坐标轴相同范围、相同工程尺度，没有按球半径缩放。", font=font(23), fill="#43566d")
    draw.text((LEFT, 1625), "左眼与每个剖面端点的残差见 candidate_A / candidate_B 内的 sphere_center_sections.json 和 validation_report.json。", font=font(21), fill="#68788c")
    canvas.save(OUT / "sphere_center_sections.png")


if __name__ == "__main__":
    verify_sources(read_json(OUT / "camera_protocol.json"))
    side_panels()
    center_panels()
    print("SPHERICAL_EYE_SECTION_PLOTS_OK")
