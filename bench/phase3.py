"""Reproduce phase 3 descriptors, inputs and paired runs without replacing results."""
import argparse
import csv
from datetime import datetime, timezone, timedelta
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

from bench.device import ROOT
from bench.run import run
from bench.workload import generate, sha256

DATA = Path(os.environ.get('ECL_DATA', Path.home()/'data/engine-core-labs'))
BINARY = DATA/'build-phase3/experiments/l1-job-system/phase3/phase3_bench'
COSTS = ROOT/'docs/datasets/bistro-costs'
DESCRIPTORS = ROOT/'workloads/phase3'


def descriptor(name, params, count, dataset=None):
    return dict(name=name, kind='task_costs', dataset=dataset or dict(name='synthetic-uniform-control', version='1', checksum='synthetic'),
                filter={}, generator='ecl-v1', params=params, seed=20261025, scale=count,
                amplification=dict(factor=1, method='none'))


def prepare():
    COSTS.mkdir(parents=True, exist_ok=True)
    # Initial collection is a separate, explicitly timed tool invocation. Once
    # checked in, these input artifacts are frozen for exact regeneration.
    for name in ('histogram.json', 'collection.json', 'mesh-costs.csv'):
        target = COSTS/name
        if not target.exists():
            shutil.copy2(DATA/'phase3/costs'/name, target)
    hist = json.loads((COSTS/'histogram.json').read_text())
    relative = '../../docs/datasets/bistro-costs/histogram.json'
    dataset = dict(name='BistroInterior-world-mesh-bounds-costs', version='Bistro_v5_2-ufbx0.23.1', checksum={relative: sha256(COSTS/'histogram.json')})
    descriptions = [descriptor(f'real-{n}', dict(distribution='histogram', histogram=relative), n, dataset) for n in (10000, 100000, 1000000)]
    mean = round(sum(lo*count for lo, hi, count in hist['buckets'])/sum(count for lo, hi, count in hist['buckets']))
    descriptions.append(descriptor('uniform-100000', dict(distribution='uniform', min=mean, max=mean), 100000))
    for cost in (64, 256, 1024, 4096, 16384, 65536, 262144, 524288):
        descriptions.append(descriptor(f'grain-{cost}', dict(distribution='uniform', min=cost, max=cost), 8388608//cost))
    DESCRIPTORS.mkdir(parents=True, exist_ok=True)
    for d in descriptions:
        path = DESCRIPTORS/(d['name']+'.json')
        path.write_text(json.dumps(d, indent=2)+'\n')
        generate(path, DATA/'phase3/workloads'/d['name'])
    print(f'Prepared {len(descriptions)} fingerprints; compression is explicitly recorded in collection.json', flush=True)


def measure(suite, output_root):
    output_root = Path(output_root).resolve()
    output_root.mkdir(parents=True, exist_ok=False)
    # Archive OS activity for interpretation; no services are stopped by this script.
    (output_root/'processes-before.txt').write_text(subprocess.check_output(['ps', '-eo', 'pid,comm,pcpu,pmem', '--sort=-pcpu'], text=True))
    receipts = {p.parent.name: json.loads(p.read_text()) for p in (DATA/'third_party/install').glob('*/build-info.json') if p.parent.name.startswith(('oneTBB-', 'Taskflow-'))}
    calibration = json.loads((COSTS/'collection.json').read_text())
    groups = []
    if suite in ('S1', 'all'):
        for mode in ('prequeued', 'end_to_end'):
            groups += [(f'S1-{mode}', mode, ['real-100000', 'uniform-100000'], [1, 2, 4, 8], 1, 1,
                        ['static', 'one', 'half', 'chase', 'tbb', 'taskflow', 'async'], 'static'),
                       (f'S1-million-{mode}', mode, ['real-1000000'], [4], 1, 1,
                        ['static', 'one', 'half', 'chase', 'tbb', 'taskflow', 'async'], 'static')]
    if suite in ('S2', 'all'):
        for producers in (1, 2, 4):
            for mode in ('prequeued', 'end_to_end'):
                groups.append((f'S2-p{producers}-{mode}', mode, ['real-100000'], [4], 1, producers, ['legacy', 'sleepers'], 'legacy'))
    if suite in ('S3', 'all'):
        for block in (1, 16, 256):
            groups.append((f'S3-block{block}', 'end_to_end', [f'grain-{c}' for c in (64, 256, 1024, 4096, 16384, 65536, 262144, 524288)],
                           [1, 2, 4], block, 1, ['serial', 'static', 'chase', 'tbb', 'taskflow'], 'serial'))
    for index, (name, mode, workloads, threads, block, producers, impls, baseline) in enumerate(groups, 1):
        config = dict(story=name, baseline=baseline, rounds=10, warmup_rounds=1, mode=mode, threads=threads,
                      compiler=['c++', '--version'], compile_options=['Release', '-O3', '-DNDEBUG', 'C++20'],
                      dataset_checksums={'cost_histogram': sha256(COSTS/'histogram.json'), 'BistroInterior.fbx': calibration['source_sha256']},
                      external_libraries=receipts, timeout_seconds=90,
                      workload_files=[str(DESCRIPTORS/(w+'.json')) for w in workloads],
                      output_dir=str(output_root/name),
                      implementations=[dict(name=impl, command=[str(BINARY), '--meta', str(DATA/'phase3/workloads'/'{workload_name}'/'{workload_name}.meta.json'),
                          '--impl', impl, '--threads', '{threads}', '--mode', '{mode}', '--fingerprint', '{workload_fingerprint}',
                          '--producers', str(producers), '--block', str(block)]) for impl in impls])
        config_path = output_root/(name+'.json')
        config_path.write_text(json.dumps(config, indent=2)+'\n')
        print(f'[{index}/{len(groups)}] {name} started', flush=True)
        start = time.monotonic()
        path = run(config_path)
        print(f'[{index}/{len(groups)}] {name} completed: {path} ({time.monotonic()-start:.1f}s)', flush=True)
    (output_root/'processes-after.txt').write_text(subprocess.check_output(['ps', '-eo', 'pid,comm,pcpu,pmem', '--sort=-pcpu'], text=True))
    print('All requested groups complete; conclusions must be read from summary.csv and invalid rows retained', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('prepare', 'measure'))
    parser.add_argument('--suite', choices=('S1', 'S2', 'S3', 'all'), default='all')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.action == 'prepare':
        prepare()
    else:
        if not args.output:
            parser.error('--output must be a new directory')
        measure(args.suite, args.output)


if __name__ == '__main__':
    main()
