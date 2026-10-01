"""Generate CSV and SVG only after the user's core and correctness tests pass."""
import argparse
import csv
import io
import json
import hashlib
import html
import os
from pathlib import Path
import platform
import statistics
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, default=Path("build"))
parser.add_argument("--output", type=Path, default=Path(".local/l1-results"))
parser.add_argument("--tasks", type=int, default=10000)
parser.add_argument("--iterations", type=int, default=10000)
parser.add_argument("--repeats", type=int, default=3)
parser.add_argument("--implementation-label", default="user-core")
args = parser.parse_args()
if args.repeats < 2 or args.repeats > 100:
    parser.error("repeats must be 2..100")
folder = args.build / "experiments/l1-job-system"
# The contract returns 77 while TODO; never convert skipped verification into success.
contract = subprocess.run([str(folder / "steal_test")], capture_output=True, text=True)
if contract.returncode:
    raise SystemExit(f"BLOCKED: steal correctness contract exit={contract.returncode}; no output created")
subprocess.run(["ctest", "--test-dir", str(args.build), "--output-on-failure"], check=True)
exe = str(folder / "job_benchmark")
rows = []
expected_checksum = None
for distribution in ("balanced", "skewed"):
    for threads in (1, 2, 4, 8):
        for mode in ("baseline", "stealing"):
            # Unrecorded warmup.
            subprocess.run([exe, mode, str(threads), "100", str(args.iterations), distribution],
                           check=True, capture_output=True)
            for repeat in range(args.repeats):
                output = subprocess.check_output([exe, mode, str(threads), str(args.tasks),
                                                  str(args.iterations), distribution], text=True)
                row = next(csv.DictReader(io.StringIO(output)))
                if expected_checksum is None:
                    expected_checksum = row["checksum"]
                if row["checksum"] != expected_checksum:
                    raise SystemExit("checksum differs across runs; refusing charts")
                row["repeat"] = str(repeat)
                rows.append(row)
args.output.mkdir(parents=True, exist_ok=True)
metadata = {"generated_at": subprocess.check_output(["date", "-Is"], text=True).strip(),
            "implementation_label": args.implementation_label,
            "source_sha256": {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in Path("experiments/l1-job-system").rglob("*")
                if p.is_file() and p.suffix in {".hpp", ".cpp", ".py"}},
            "machine": platform.node(), "os": platform.platform(), "logical_cpus": os.cpu_count(),
            "lscpu": subprocess.check_output(["lscpu"], text=True),
            "compiler": subprocess.check_output(["c++", "--version"], text=True),
            "tasks": args.tasks, "iterations": args.iterations, "repeats": args.repeats,
            "build_cache": (args.build / "CMakeCache.txt").read_text(),
            "source_head": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
            "uncommitted": subprocess.check_output(["git", "status", "--short"], text=True),
            "method": "wall time includes submission and drain; warmup excluded; mean and sample stdev"}
(args.output / "device.json").write_text(json.dumps(metadata, indent=2))
with (args.output / "raw.csv").open("w") as file:
    writer = csv.DictWriter(file, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)
summary = []
for distribution in ("balanced", "skewed"):
    for mode in ("baseline", "stealing"):
        points = []
        for threads in (1, 2, 4, 8):
            values = [float(r["tasks_per_second"]) for r in rows if
                      r["distribution"] == distribution and r["mode"] == mode and
                      int(r["threads"]) == threads]
            mean = statistics.mean(values)
            summary.append({"distribution": distribution, "mode": mode, "threads": threads,
                            "mean_tasks_per_second": mean, "stdev": statistics.stdev(values)})
            points.append((threads, mean))
with (args.output / "summary.csv").open("w") as file:
    writer = csv.DictWriter(file, fieldnames=list(summary[0])); writer.writeheader(); writer.writerows(summary)
for distribution in ("balanced", "skewed"):
    group = [r for r in summary if r["distribution"] == distribution]
    maximum = max(float(r["mean_tasks_per_second"]) for r in group)
    parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="720" height="450">',
             '<rect width="720" height="450" fill="white"/>',
             f'<text x="60" y="25">L1 {distribution}: {html.escape(args.implementation_label)} mean tasks/sec</text>',
             '<path d="M60 50 V370 H680" fill="none" stroke="black"/>']
    for mode, color in (("baseline", "#777"), ("stealing", "#167ac6")):
        points = [(60 + (int(r["threads"])-1)/7*620,
                   370-float(r["mean_tasks_per_second"])/maximum*300)
                  for r in group if r["mode"] == mode]
        coords = " ".join(f"{x:.1f},{y:.1f}" for x, y in points)
        parts.append(f'<polyline points="{coords}" fill="none" stroke="{color}" stroke-width="2"/>')
        parts.append(f'<text x="70" y="{405 if mode == "baseline" else 430}" fill="{color}">{mode}</text>')
    for threads in (1,2,4,8):
        parts.append(f'<text x="{60+(threads-1)/7*620}" y="390">{threads}</text>')
    for fraction in (0,0.5,1):
        parts.append(f'<text x="2" y="{370-fraction*300}">{maximum*fraction:.0f}</text>')
    parts.append('</svg>')
    (args.output / f"{distribution}.svg").write_text("\n".join(parts))
print(f"Saved reproducible measurements to {args.output}")
