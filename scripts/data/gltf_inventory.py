#!/usr/bin/env python3
"""Inventory of a glTF-Sample-Assets checkout: per-model size, file formats and license.

Licenses come from REUSE.toml annotations of the form path = "Models/<name>/*";
models without such an annotation are reported as "unannotated" rather than
inheriting the repository-wide default, because licenses differ per model.

Usage: gltf_inventory.py <checkout> <out_dir>
Writes models.csv, extensions.csv and summary.json into out_dir.
"""
import csv
import json
import subprocess
import sys
import tomllib
from collections import Counter, defaultdict
from pathlib import Path


def model_licenses(root: Path) -> dict[str, list[str]]:
    data = tomllib.loads((root / "REUSE.toml").read_text())
    out: dict[str, list[str]] = defaultdict(list)
    for ann in data.get("annotations", []):
        paths = ann["path"] if isinstance(ann["path"], list) else [ann["path"]]
        lic = ann.get("SPDX-License-Identifier", "")
        for p in paths:
            parts = p.split("/")
            if len(parts) >= 2 and parts[0] == "Models" and parts[1] not in ("*", "**"):
                out[parts[1]].append(lic)
    return out


def percentile(sorted_vals: list[int], q: float) -> int:
    if not sorted_vals:
        return 0
    idx = min(len(sorted_vals) - 1, round(q * (len(sorted_vals) - 1)))
    return sorted_vals[idx]


def main() -> None:
    root, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)
    commit = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"],
                            capture_output=True, text=True, check=True).stdout.strip()
    licenses = model_licenses(root)

    rows, ext_count, ext_bytes, file_sizes = [], Counter(), Counter(), []
    for model in sorted(p for p in (root / "Models").iterdir() if p.is_dir()):
        files = [f for f in model.rglob("*") if f.is_file()]
        size = sum(f.stat().st_size for f in files)
        for f in files:
            ext = f.suffix.lower() or "<none>"
            ext_count[ext] += 1
            ext_bytes[ext] += f.stat().st_size
            file_sizes.append(f.stat().st_size)
        lic = licenses.get(model.name)
        rows.append({"model": model.name, "files": len(files), "bytes": size,
                     "license": ";".join(sorted(set(lic))) if lic else "unannotated"})

    with open(out_dir / "models.csv", "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=["model", "files", "bytes", "license"])
        w.writeheader()
        w.writerows(rows)
    with open(out_dir / "extensions.csv", "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["extension", "files", "bytes"])
        for ext, n in ext_count.most_common():
            w.writerow([ext, n, ext_bytes[ext]])

    model_sizes = sorted(r["bytes"] for r in rows)
    file_sizes.sort()
    summary = {
        "commit": commit,
        "models": len(rows),
        "files": len(file_sizes),
        "total_bytes": sum(file_sizes),
        "model_bytes": {q: percentile(model_sizes, v) for q, v in
                        [("min", 0), ("p50", .5), ("p90", .9), ("max", 1)]},
        "file_bytes": {q: percentile(file_sizes, v) for q, v in
                       [("min", 0), ("p50", .5), ("p90", .9), ("p99", .99), ("max", 1)]},
        "licenses": dict(Counter(r["license"] for r in rows).most_common()),
    }
    (out_dir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
