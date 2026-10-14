"""Extract a small P0 runtime result without running or changing Unreal.

python summarize_p0_result.py SUMMARY_JSON [SUMMARY_JSON ...] --output OUTPUT_JSON
Only positive finite CSV frame times enter median/P95; zero paused ticks are excluded.
The reported cap is a label supplied by the caller, not proof of an actual frame rate.
"""
import argparse
import csv
import json
import math
import statistics
from pathlib import Path


def frame_stats(values):
    samples = sorted(value for value in values if math.isfinite(value) and value > 0)
    if not samples:
        return {"sample_count": 0, "median_ms": None, "p95_ms": None}
    return {
        "sample_count": len(samples),
        "median_ms": round(statistics.median(samples), 4),
        "p95_ms": round(samples[math.ceil(0.95 * len(samples)) - 1], 4),
    }


def summarize(path):
    path = path.resolve()
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    csv_path = Path(data.get("csv_path") or path.with_name("frames.csv"))
    if not csv_path.is_file():
        csv_path = path.with_name("frames.csv")
    columns = {"frame_ms": [], "wall_frame_ms": []}
    if csv_path.is_file():
        with csv_path.open(encoding="utf-8-sig", newline="") as stream:
            for row in csv.DictReader(stream):
                for name, values in columns.items():
                    if row.get(name):
                        values.append(float(row[name]))
    failed = []
    for case in data.get("cases", []):
        if not case.get("pass", False):
            failed.append({
                "round": case.get("round"),
                "case": case.get("case"),
                "checks": [{key: check.get(key) for key in ("name", "expected", "actual")}
                           for check in case.get("checks", []) if not check.get("pass", False)],
            })
    return {
        "summary_path": str(path),
        "run_id": data.get("run_id"),
        "status": data.get("status"),
        "complete": data.get("complete", False),
        "planned_cases": data.get("planned_cases"),
        "completed_cases": data.get("completed_cases"),
        "screenshots_saved": data.get("all_24_screenshots_saved"),
        "output_write_failed": data.get("output_write_failed"),
        "failed_cases": failed,
        "csv_path": str(csv_path) if csv_path.is_file() else None,
        "simulation_frame_time": frame_stats(columns["frame_ms"]),
        "wall_frame_time": frame_stats(columns["wall_frame_ms"]),
        "timing_limit": "Positive finite case CSV samples; excludes zero paused simulation ticks. Includes case screenshots/CPU skin extraction and startup effects, so this is a regression cadence record, not a stable performance baseline. P95 uses nearest rank.",
        "performance_samples": data.get("performance_samples", []),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("summaries", nargs="+", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    results = [summarize(path) for path in args.summaries]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({"runs": results}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(args.output.resolve()),
                      "runs": [{"run_id": r["run_id"], "status": r["status"], "complete": r["complete"],
                                "completed_cases": r["completed_cases"], "failed_cases": len(r["failed_cases"])}
                               for r in results]}, ensure_ascii=False))


if __name__ == "__main__":
    main()
