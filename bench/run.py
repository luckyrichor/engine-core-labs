"""Run independent experiment processes; programs own all metric timing."""
import argparse
import csv
from datetime import datetime
import hashlib
import io
import json
import math
from pathlib import Path
import re
import shlex
import subprocess
import sys
from datetime import timedelta, timezone

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from bench.device import ROOT, collect, swap_status
from bench.schema import RAW_FIELDS, PROGRAM_FIELDS, MODES
from bench.summarize import summarize
from bench.verdict import MIN_ROUNDS, RECOMMENDED_ROUNDS


def parse_output(stdout):
    reader = csv.DictReader(io.StringIO(stdout))
    if reader.fieldnames != PROGRAM_FIELDS:
        raise ValueError('experiment stdout header must be ' + ','.join(PROGRAM_FIELDS))
    rows, metrics = [], set()
    for row in reader:
        if None in row or any(v is None for v in row.values()):
            raise ValueError('malformed experiment CSV row')
        if not row['metric'] or row['metric'] in metrics:
            raise ValueError('empty or duplicate metric')
        metrics.add(row['metric'])
        if row['valid'] not in ('0', '1') or row['stub'] not in ('0', '1') or row['better'] not in ('higher', 'lower'):
            raise ValueError('invalid valid, stub or better field')
        if not math.isfinite(float(row['value'])):
            raise ValueError('experiment value must be finite')
        rows.append(row)
    if not rows:
        raise ValueError('experiment emitted no metrics')
    return rows


def run(config_path):
    config_path = Path(config_path).resolve()
    config = json.loads(config_path.read_text(encoding='utf-8'))
    repeats = config.get('rounds', RECOMMENDED_ROUNDS)
    warmups = config.get('warmup_rounds', 1)
    if type(repeats) is not int or repeats < MIN_ROUNDS or type(warmups) is not int or warmups < 0:
        raise ValueError('rounds must be >= 5 and warmup_rounds >= 0 (integers)')
    threads_list = config['threads']
    if not threads_list or any(type(t) is not int or t < 1 for t in threads_list) or len(set(threads_list)) != len(threads_list):
        raise ValueError('threads must be a nonempty list of distinct positive integers')
    implementations = config['implementations']
    names = [item['name'] for item in implementations]
    baseline = config['baseline']
    if not names or len(set(names)) != len(names) or baseline not in names or any('@aa' in name for name in names):
        raise ValueError('implementation names must be unique, contain baseline and reserve @aa')
    for impl in implementations:
        if not isinstance(impl['command'], list) or not impl['command'] or not all(isinstance(x, str) for x in impl['command']):
            raise ValueError('command templates must be nonempty argument lists (no shell)')
    mode = config.get('mode', 'na')
    if mode not in MODES:
        raise ValueError('unknown mode')
    # Affinity is intentionally not changed. Record observed inherited process affinity.
    import os
    cpus = sorted(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else None
    workloads = []
    for filename in config['workload_files']:
        path = (config_path.parent / filename).resolve()
        description = json.loads(path.read_text(encoding='utf-8'))
        canonical = json.dumps(description, sort_keys=True, separators=(',', ':'), ensure_ascii=False).encode('utf-8')
        workloads.append((path, description['name'], hashlib.sha256(canonical).hexdigest()[:16]))
    if not workloads or len({name for _, name, _ in workloads}) != len(workloads):
        raise ValueError('workloads must have distinct names and cannot be empty')
    device = collect(compiler=config.get('compiler'), compile_options=config.get('compile_options'),
                     dataset_checksums=config.get('dataset_checksums'),
                     external_libraries=config.get('external_libraries'), threads=threads_list)
    machine = device['machine']
    story = config['story']
    if not re.fullmatch(r'[A-Za-z0-9_.-]+', story) or not re.fullmatch(r'[A-Za-z0-9_.-]+', machine):
        raise ValueError('story and machine must be safe directory components')
    date = datetime.now(timezone(timedelta(hours=8))).strftime('%Y-%m-%d')
    destination = config.get('output_dir')
    directory = (config_path.parent / destination).resolve() if destination else ROOT / 'docs' / 'measurements' / f'{date}-{story}-{machine}'
    directory.mkdir(parents=True, exist_ok=False)  # Never replace an earlier run.
    (directory / 'device.json').write_text(json.dumps(device, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    (directory / 'config.json').write_text(json.dumps(config, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    readme = (f'# {story}: reproduction\n\nWorking directory: `{ROOT}`\n\n'
              f'Configuration used: `{config_path}` (snapshot: config.json).\n\n'
              f'```sh\n{shlex.join([sys.executable, str(ROOT / "bench/run.py"), str(config_path)])}\n```\n\n'
              'Select a new output_dir before rerunning; existing results are never overwritten.\n'
              'Workload paths are relative to the original configuration directory. Commands run from the repository root.\n'
              'No runner timing or affinity changes. pinned_cpus records inherited Linux affinity; '
              'pinned=0 means no binding requested by this runner.\n\n'
              'Status: running.\n')
    (directory / 'README.md').write_text(readme, encoding='utf-8')
    aa_command = next(item['command'] for item in implementations if item['name'] == baseline)
    entries = [(item['name'], item['command']) for item in implementations] + [(baseline + '@aa', aa_command)]
    any_stub = False
    try:
        with (directory / 'raw.csv').open('w', newline='', encoding='utf-8') as target:
            writer = csv.DictWriter(target, fieldnames=RAW_FIELDS)
            writer.writeheader()
            for workload_path, workload, fingerprint in workloads:
                for threads in threads_list:
                    for index in range(-warmups, repeats):
                        # Each measured round rotates by one entry; warmup does not shift round 1.
                        rotation = index % len(entries)
                        ordered = entries[rotation:] + entries[:rotation]
                        for order, (name, template) in enumerate(ordered, 1):
                            variables = dict(threads=threads, workload=str(workload_path),
                                             workload_name=workload, workload_fingerprint=fingerprint,
                                             round=index + 1, mode=mode, impl=baseline if name == baseline + '@aa' else name,
                                             python=sys.executable, repo=str(ROOT))
                            command = [part.format_map(variables) for part in template]
                            before = swap_status()['used_kb']
                            result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True,
                                                    timeout=config.get('timeout_seconds'))
                            after = swap_status()['used_kb']
                            rows = parse_output(result.stdout)
                            reasons = [row['invalid_reason'] or 'validation_failed' for row in rows if row['valid'] == '0']
                            if result.returncode:
                                reasons.append(f'exit_code={result.returncode}')
                            if sys.platform.startswith('linux') and before is not None and after is not None and before != after:
                                reasons.append('swap_usage_changed')
                            for row in rows:
                                any_stub |= row['stub'] == '1'
                                if index < 0:
                                    continue
                                if reasons:
                                    row.update(valid='0', invalid_reason=';'.join(dict.fromkeys(reasons)))
                                row.update(round=index + 1, order=order, impl=name, workload=workload,
                                           workload_fingerprint=fingerprint, threads=threads, mode=mode,
                                           swap_used_before_kb=before, swap_used_after_kb=after,
                                           pinned=0, pinned_cpus=';'.join(map(str, cpus)) if cpus is not None else '')
                                writer.writerow(row)
                            target.flush()
        summarize(directory, baseline)
    except Exception as error:
        (directory / 'README.md').write_text(readme.replace('Status: running.', f'Status: failed ({type(error).__name__}). Partial output is not a completed experiment.'), encoding='utf-8')
        raise
    (directory / 'README.md').write_text(readme.replace('Status: running.',
        f'Status: complete. stub={int(any_stub)}; stub data is synthetic and cannot support performance claims.' if any_stub else 'Status: complete. stub=0.'), encoding='utf-8')
    return directory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('config', type=Path)
    args = parser.parse_args()
    print(run(args.config))


if __name__ == '__main__':
    main()
