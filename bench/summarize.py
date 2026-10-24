"""Summarize raw.csv using paired rounds; never measure elapsed time here."""
import argparse
import csv
import json
import math
import os
from pathlib import Path
import statistics
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from bench.schema import RAW_FIELDS, SUMMARY_FIELDS, MODES
from bench.verdict import MIN_P99_SAMPLES, decide


def summarize(directory, baseline):
    directory = Path(directory)
    groups, identities, descriptors = {}, {}, {}
    with (directory / 'raw.csv').open(newline='', encoding='utf-8') as source:
        reader = csv.DictReader(source)
        if reader.fieldnames != RAW_FIELDS:
            raise ValueError('raw.csv header does not match fixed schema')
        for row in reader:
            if None in row or any(value is None for value in row.values()):
                raise ValueError('malformed raw.csv row')
            number, threads = int(row['round']), int(row['threads'])
            if number < 1 or threads < 1 or row['mode'] not in MODES:
                raise ValueError('invalid round, threads or mode')
            if row['better'] not in ('higher', 'lower') or row['valid'] not in ('0', '1') or row['stub'] not in ('0', '1'):
                raise ValueError('invalid better, valid or stub')
            workload = row['workload']
            fingerprint = row['workload_fingerprint']
            if workload in identities and identities[workload] != fingerprint:
                raise ValueError('one workload name has multiple fingerprints')
            identities[workload] = fingerprint
            key = (workload, threads, row['mode'], row['metric'], row['impl'])
            descriptor = (row['better'], row['unit'], row['stub'])
            if key in descriptors and descriptors[key] != descriptor:
                raise ValueError('mixed units, directions or stub status in a group')
            descriptors[key] = descriptor
            group = groups.setdefault(key, {})
            # Keep invalid-only groups visible, but never use their values.
            if row['valid'] == '1':
                value = float(row['value'])
                if not math.isfinite(value):
                    raise ValueError('non-finite valid measurement')
                if number in group:
                    raise ValueError('duplicate valid round in a metric group')
                group[number] = value

    device_path = directory / 'device.json'
    device = json.loads(device_path.read_text(encoding='utf-8')) if device_path.exists() else {}
    # Plan section A: threads beyond physical cores are oversubscribed. Section B:
    # on hybrid CPUs, threads beyond the performance-core threads run on mixed cores.
    cores = device.get('physical_cores') or device.get('logical_threads') or os.cpu_count()
    performance = device.get('performance_cpus')
    hybrid_limit = len(performance) if performance and device.get('efficiency_cpus') else None

    def paired(left, right):
        # A zero denominator has no defined ratio and cannot support a verdict.
        return [value / right[r] for r, value in sorted(left.items())
                if r in right and right[r] != 0 and math.isfinite(value / right[r])]

    output = []
    for key, group in sorted(groups.items()):
        workload, threads, mode, metric, impl = key
        prefix = key[:4]
        base = groups.get((*prefix, baseline), {})
        aa = groups.get((*prefix, baseline + '@aa'), {})
        better, unit, stub = descriptors[key]
        for name in (baseline, baseline + '@aa'):
            other = descriptors.get((*prefix, name))
            if other is not None and other != (better, unit, stub):
                raise ValueError('comparison has mixed units, directions or stub status')
        values = list(group.values())
        ratios, noise = paired(group, base), paired(aa, base)
        samples_ok = True
        if metric.endswith('_p99'):
            sample_metric = metric[:-4] + '_samples'
            # Check each contributing impl, baseline and A/A round separately.
            for name, measurements in ((impl, group), (baseline, base), (baseline + '@aa', aa)):
                samples = groups.get((workload, threads, mode, sample_metric, name), {})
                if any(samples.get(r, 0) < MIN_P99_SAMPLES for r in measurements):
                    samples_ok = False
        verdict, flags = decide(values, ratios, noise, better, samples_ok=samples_ok,
                                oversubscribed=bool(cores and threads > cores),
                                hybrid_cores=bool(hybrid_limit and threads > hybrid_limit), stub=stub == '1')
        row = dict(zip(SUMMARY_FIELDS[:5], key))
        row.update(rounds_valid=len(values), median=statistics.median(values) if values else '',
                   min=min(values) if values else '', max=max(values) if values else '',
                   ratio_median=statistics.median(ratios) if ratios else '',
                   ratio_min=min(ratios) if ratios else '', ratio_max=max(ratios) if ratios else '',
                   aa_min=min(noise) if noise else '', aa_max=max(noise) if noise else '',
                   verdict=verdict, flags=';'.join(flags))
        output.append(row)
    with (directory / 'summary.csv').open('w', newline='', encoding='utf-8') as target:
        writer = csv.DictWriter(target, fieldnames=SUMMARY_FIELDS)
        writer.writeheader()
        writer.writerows(output)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--baseline', required=True)
    args = parser.parse_args()
    summarize(args.directory, args.baseline)


if __name__ == '__main__':
    main()
