"""Postprocessing only: no new verdict threshold and no hot-path modifications."""
import csv
from pathlib import Path
import statistics
ROOT=Path(__file__).resolve().parent
rows=[]
for directory in ROOT.iterdir():
    if (directory/'raw.csv').exists():
        with (directory/'raw.csv').open() as f:
            rows.extend(dict(row,group=directory.name) for row in csv.DictReader(f) if row['valid']=='1')
indexed={(r['group'],r['workload'],r['threads'],r['round'],r['impl'],r['metric']):float(r['value']) for r in rows}
estimates=[]
for r in rows:
    if not r['group'].startswith('S3-') or r['metric']!='elapsed_ns' or r['impl'] in ('serial','serial@aa'):
        continue
    serial=indexed[(r['group'],r['workload'],r['threads'],r['round'],'serial','elapsed_ns')]
    estimates.append(dict(group=r['group'],workload=r['workload'],threads=r['threads'],round=r['round'],impl=r['impl'],
        parallel_efficiency_gap=1-serial/(int(r['threads'])*float(r['value']))))
with (ROOT/'s3-efficiency-gap.csv').open('w') as f:
    w=csv.DictWriter(f,fieldnames=list(estimates[0]));w.writeheader();w.writerows(estimates)
# This is deviation from ideal serial/T time, not measured pure scheduler CPU.
# It combines enqueue, queueing, synchronization, memory effects and imbalance;
# negative values are retained rather than hidden/clipped.
print('executions:',len({(r['group'],r['workload'],r['threads'],r['round'],r['impl']) for r in rows}))
for metric in ('static_imbalance','idle_fraction','search_fraction','steal_success_rate'):
    print('S1 real-100k 4T prequeued',metric)
    for impl in ('static','one','half','chase'):
        values=[float(r['value']) for r in rows if r['group']=='S1-prequeued' and r['workload']=='real-100000' and r['threads']=='4' and r['impl']==impl and r['metric']==metric]
        print(impl,round(statistics.median(values),6))
print('S2 submit p99 per paired group:')
for path in sorted(ROOT.glob('S2-*/summary.csv')):
    with path.open() as f:
        for r in csv.DictReader(f):
            if r['impl']=='sleepers' and r['metric']=='submit_batch_p99':
                print(path.parent.name, r['ratio_median'],r['verdict'])
print('S3 large-task 4T ratios:')
with (ROOT/'s3-curve.csv').open() as f:
    for r in csv.DictReader(f):
        if r['work_units']=='524288' and r['threads']=='4' and r['impl'] in ('static','chase'):
            print(r)
