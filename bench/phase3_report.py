"""Summarize already-classified phase-3 rows; draw S3 with matplotlib (optional)."""
import argparse
import csv
import json
from pathlib import Path
import statistics

from bench.device import ROOT, source_fingerprint


def load(root):
    rows = []
    raw = []
    for directory in sorted(root.iterdir()):
        if (directory/'summary.csv').exists():
            with (directory/'summary.csv').open() as stream:
                for row in csv.DictReader(stream):
                    row['group'] = directory.name
                    rows.append(row)
            with (directory/'raw.csv').open() as stream:
                for row in csv.DictReader(stream):
                    row['group'] = directory.name
                    raw.append(row)
    return rows, raw


def report(root, plots=False):
    rows, raw = load(root)
    through = [r for r in rows if r['metric'] == 'throughput' and '@aa' not in r['impl']]
    # Never recompute significance here: bench.verdict is the only policy.
    with (root/'throughput-comparisons.csv').open('w') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(through[0]))
        writer.writeheader(); writer.writerows(through)
    invalid = [r for r in raw if r['valid'] != '1']
    groups = {r['group'] for r in rows}
    fingerprints = {r['workload']: r['workload_fingerprint'] for r in raw}
    hashes = {json.loads((root/g/'device.json').read_text())['source']['sha256'] for g in groups}
    validity = {'groups': len(groups), 'raw_rows': len(raw), 'invalid_rows': len(invalid),
                'invalid_executions': len({(r['group'], r['workload'], r['threads'], r['round'], r['impl']) for r in invalid}),
                'invalid_reasons': sorted({r['invalid_reason'] for r in invalid}),
                'source_hashes': sorted(hashes), 'current_source_hash': source_fingerprint()['sha256'],
                'workload_fingerprints': fingerprints,
                'minimum_valid_throughput_rounds': min(int(r['rounds_valid']) for r in through),
                'stub_rows': sum(r['stub'] != '0' for r in raw)}
    (root/'audit.json').write_text(json.dumps(validity, indent=2)+'\n')
    calibration = json.loads((ROOT/'docs/datasets/bistro-costs/collection.json').read_text())
    curve = []
    for r in through:
        if r['group'].startswith('S3-'):
            cost = int(r['workload'].split('-')[1])
            curve.append(dict(block=int(r['group'].replace('S3-block','')), work_units=cost,
                              calibrated_ns=cost*calibration['ns_per_work_unit'], threads=int(r['threads']),
                              impl=r['impl'], speedup=r['ratio_median'], speedup_min=r['ratio_min'],
                              speedup_max=r['ratio_max'], verdict=r['verdict'], flags=r['flags']))
    with (root/'s3-curve.csv').open('w') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(curve[0]))
        writer.writeheader();writer.writerows(curve)
    if plots:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        fig, axes = plt.subplots(1, 3, figsize=(15, 4), sharey=True)
        for axis, block in zip(axes, (1, 16, 256)):
            for impl in ('static','chase','tbb','taskflow'):
                selected = sorted([r for r in curve if r['block']==block and r['threads']==4 and r['impl']==impl], key=lambda r:r['calibrated_ns'])
                if not selected:
                    continue
                axis.plot([r['calibrated_ns'] for r in selected], [float(r['speedup']) for r in selected], 'o-', label=impl)
            axis.axhline(1, color='black', linewidth=.8)
            axis.set_xscale('log');axis.set_yscale('log')
            axis.set_title(f'block={block}, workers=4')
            axis.set_xlabel('calibrated work per element (ns)');axis.grid(alpha=.25)
        axes[0].set_ylabel('paired throughput / serial (median)')
        axes[-1].legend();fig.suptitle('S3: fixed total integer work; significance is in summary.csv')
        fig.tight_layout();fig.savefig(root/'s3-speedup.svg');plt.close(fig)
        (root/'plot-version.json').write_text(json.dumps({'matplotlib':matplotlib.__version__})+'\n')
    def table(story, predicate=lambda r: True):
        selected = [r for r in through if r['group'].startswith(story) and predicate(r)]
        text = '| group | workload | T | impl | ratio median [min,max] | verdict | flags |\n|---|---|---|---|---|---|---|\n'
        for r in selected:
            ratio = f"{float(r['ratio_median']):.3f} [{float(r['ratio_min']):.3f},{float(r['ratio_max']):.3f}]" if r['ratio_median'] else ''
            text += f"| {r['group']} | {r['workload']} | {r['threads']} | {r['impl']} | {ratio} | {r['verdict']} | {r['flags']} |\n"
        return text
    text = '# Phase 3 paired results\n\n'
    text += 'Raw rows, including invalid runs, are retained. Classifications come only from bench.verdict. Work costs are deterministic CPU proxies resampled from measured Bistro mesh bounds with 16x time compression; these are not actual engine mesh tasks.\n\n'
    text += f"Audit: {len(groups)} groups; {len(raw)} rows; {len(invalid)} invalid metric rows; minimum valid throughput rounds={validity['minimum_valid_throughput_rounds']}; stub rows={validity['stub_rows']}.\n\n"
    text += '## S1: real 100k tasks, four workers\n\n'+table('S1-', lambda r:r['workload']=='real-100000' and r['threads']=='4')
    text += '\n## S1: uniform control, four workers\n\n'+table('S1-', lambda r:r['workload']=='uniform-100000' and r['threads']=='4')
    text += '\n## S2: conditional notification\n\n'+table('S2-',lambda r:r['impl']=='sleepers')
    text += '\n## S3: crossover evidence\n\n'
    for block in (1,16,256):
        for impl in ('static','chase','tbb','taskflow'):
            points=sorted([r for r in curve if r['threads']==4 and r['block']==block and r['impl']==impl and r['verdict']=='significant_improvement'],key=lambda r:r['calibrated_ns'])
            text += f"- block={block}, {impl}: "+(f"first sampled significant improvement near {points[0]['calibrated_ns']:.0f} ns/element (a sampled point, not an exact hardware threshold)." if points else 'no sampled point met the significant-improvement rule.')+'\n'
    text += '\nAll thread counts, modes and raw ratio ranges are in throughput-comparisons.csv and s3-curve.csv. Do not infer significance from a line chart alone.\n'
    (root/'RESULTS.md').write_text(text)
    print(json.dumps(validity, indent=2))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path);parser.add_argument('--plots',action='store_true')
    args=parser.parse_args();report(args.directory, args.plots)

if __name__=='__main__':
    main()
