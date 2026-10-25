"""S8 paired layout/affinity experiments; Linux and Windows stay separate."""
import argparse
import csv
from datetime import datetime, timezone, timedelta
import io
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tempfile

from bench.device import ROOT, collect, source_fingerprint, swap_status, windows_info
from bench.run import parse_output
from bench.schema import RAW_FIELDS, PROGRAM_FIELDS
from bench.summarize import summarize
from bench.workload import generate, fingerprint, load, sha256

DATA = Path(os.environ.get('ECL_DATA', Path.home()/'data/engine-core-labs'))
CASES = {'positive': (2000000, 0), 'cold': (100000, 64), 'queue': (100000, 64),
         'counters': (50000, 128), 'stats': (100000, 64)}


def prepare():
    directory = ROOT/'workloads/phase4'
    directory.mkdir(parents=True, exist_ok=True)
    for case, (iterations, units) in CASES.items():
        d = dict(name='s8-'+case, kind='constructive',
                 dataset=dict(name='constructive-S8-layout', version='1', checksum='constructive'),
                 filter={}, generator='ecl-v1', params=dict(case=case, iterations=iterations, work_units=units),
                 seed=20261025, scale=iterations, amplification=dict(factor=1, method='none'))
        path = directory/(d['name']+'.json')
        # In a source snapshot, use the shipped descriptor verbatim.
        if not path.exists():
            path.write_text(json.dumps(d, indent=2)+'\n')
        generate(path, DATA/'phase4/workloads'/d['name'])
    print('S8 constructive workloads prepared', flush=True)


def execute(command, *, perf=False, timeout=100):
    with tempfile.TemporaryDirectory(prefix='ecl-s8-') as temp:
        directory = Path(temp)
        if perf:
            control, ack, counters = directory/'control', directory/'ack', directory/'perf.txt'
            os.mkfifo(control); os.mkfifo(ack)
            command = ['sudo','-n','timeout','--kill-after=5s',str(timeout-10)+'s',
                'perf','stat','--no-big-num','-x',',','-e','cpu-migrations','--delay=-1',
                '--control',f'fifo:{control},{ack}','-o',str(counters),'--',
                *command,'--perf-control',str(control),'--perf-ack',str(ack)]
        result = subprocess.run(command, capture_output=True, text=True, errors="replace", timeout=timeout,
                                env=os.environ | {'LC_ALL':'C'})
        rows = parse_output(result.stdout)
        metadata = None
        for line in result.stderr.splitlines():
            if line.startswith('S8_META:'):
                metadata = json.loads(line[8:])
        if result.returncode or metadata is None:
            raise RuntimeError(f'experiment exit={result.returncode}; {result.stderr[-2000:]}')
        if perf:
            try:
                text = counters.read_text()
            except PermissionError:
                text = subprocess.check_output(['sudo','-n','cat',str(counters)],text=True)
            values = [row[0].strip() for row in csv.reader(io.StringIO(text))
                      if len(row)>2 and row[2].strip()=='cpu-migrations']
            if len(values)!=1 or not re.fullmatch(r'[0-9]+',values[0]):
                raise RuntimeError('cpu-migrations not counted: '+text)
            rows.append(dict(metric='cpu_migrations',value=values[0],unit='count',better='lower',valid='1',invalid_reason='',stub='0'))
            metadata['perf_interval']='enabled before release; disabled before validation; acknowledgement gaps included'
        return rows, metadata


def power_scheme():
    text=subprocess.check_output(['powercfg','/getactivescheme'],text=True,errors='replace')
    match=re.search(r'[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}',text)
    if not match:raise RuntimeError('active power scheme UUID unavailable')
    return match.group(0)


def measure(binary, output, *, case_names, threads, use_perf=True):
    output=Path(output).resolve();output.mkdir(parents=True,exist_ok=False)
    original_plan=None
    if sys.platform=='win32':
        original_plan=power_scheme()
        (output/'original-power-plan.txt').write_text(original_plan+'\n')
    status='failed'
    try:
        if original_plan is not None:
            subprocess.run(['powercfg','/setactive','8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c'],check=True)
            if power_scheme().lower()!='8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c':
                raise RuntimeError('high-performance scheme did not take effect')
        compiler=['c++','--version'] if sys.platform!='win32' else ['cmd','/d','/c','cl 2>&1 & exit /b 0']
        device=collect(compiler=compiler,compile_options=['Release','C++20','/O2' if sys.platform=='win32' else '-O3','NDEBUG'],threads=threads,
                       dataset_checksums={case:sha256(ROOT/'workloads/phase4'/('s8-'+case+'.json')) for case in case_names})
        layout=json.loads(subprocess.check_output([str(binary),'--inspect','1'],text=True))
        if layout['cache_line']!=64:raise RuntimeError('unconfirmed cache line')
        (output/'layout-audit.json').write_text(json.dumps(layout,indent=2)+'\n')
        if sys.platform=='win32':
            cores=device['processor_cores'];best=max(c['efficiency_class'] for c in cores)
            eligible=[min(c['cpus']) for c in cores if c['efficiency_class']==best]
            if len(eligible)<max(threads):raise RuntimeError('main S8 matrix must fit one thread per performance core')
            device['affinity_policy']='both modes limited to one logical CPU per physical performance core; pinned mode additionally fixes each worker'
            device['ac_power_required']=True
        else:
            eligible=sorted(os.sched_getaffinity(0))
            device['affinity_policy']='inherited eligible CPUs in both modes; pinned worker i uses eligible[i % count]'
        device['s8_eligible_cpus']=eligible
        device['s8_cache_line']=layout
        device['migration_collection']={'enabled':bool(use_perf and sys.platform.startswith('linux')),
            'event':'cpu-migrations only','process_identity':'sudo/root' if use_perf and sys.platform.startswith('linux') else 'current SSH user',
            'scope':'all benchmark threads in controlled interval, including the unpinned producer/main thread'}
        snapshot=ROOT/'source-snapshot.json'
        if snapshot.exists():
            provenance=json.loads(snapshot.read_text())
            if provenance['source']['sha256']!=device['source']['sha256']:raise RuntimeError('source snapshot checksum mismatch')
            device['git']=provenance['git']|{'source_snapshot':True}
        if sys.platform.startswith('linux'):
            (output/'processes-before.txt').write_text(subprocess.check_output(['ps','-eo','pid,comm,pcpu,pmem','--sort=-pcpu'],text=True))
        group_count=len(case_names)
        for group_index,case in enumerate(case_names,1):
            descriptor_path=ROOT/'workloads/phase4'/('s8-'+case+'.json')
            description=load(descriptor_path);fp=fingerprint(description)
            variants=['packed','pending','next','wake'] if case=='counters' else ['packed','separated']
            directory=output/case;directory.mkdir()
            (directory/'device.json').write_text(json.dumps(device,indent=2)+'\n')
            entries=variants+[v+'-pinned' for v in variants]+['packed@aa','packed-pinned@aa']
            config=dict(case=case,threads=threads,entries=entries,rounds=10,warmup_rounds=1,
                        descriptor=description,workload_fingerprint=fp,eligible_cpus=eligible,
                        binary=str(binary),perf=bool(use_perf and sys.platform.startswith('linux')),
                        design='all layout and worker-binding cells interleaved in each round; two baseline A/A groups')
            (directory/'config.json').write_text(json.dumps(config,indent=2)+'\n')
            readme=f'# S8 {case}\n\n10 rounds + 1 warmup. All binding/layout cells interleave and rotate in the same round. packed@aa and packed-pinned@aa are independent noise baselines.\n\nsummary.csv uses packed (unbound) baseline: compare unbound layouts and packed-pinned for one-factor effects. summary-pinned.csv uses packed-pinned baseline: compare pinned layouts only. Joint layout+binding changes are not a one-factor result.\n\nSource/topology in device.json; actual affinity/readback in executions.jsonl. Windows unbound has no per-worker pin but retains the same P-core eligible mask. Producer/main is not pinned.\n\nReproduce with python -m bench.phase4 measure --binary <binary> --output <new directory> --cases {case}.\n\nStatus: running.\n'
            (directory/'README.md').write_text(readme)
            print(f'[{group_index}/{group_count}] {case} started',flush=True)
            with (directory/'raw.csv').open('w',newline='') as stream,(directory/'executions.jsonl').open('w') as evidence:
                writer=csv.DictWriter(stream,fieldnames=RAW_FIELDS);writer.writeheader()
                for count in threads:
                    for index in range(-1,10):
                        rotation=index%len(entries);ordered=entries[rotation:]+entries[:rotation]
                        for order,label in enumerate(ordered,1):
                            selected=label.removesuffix('@aa')
                            pinned=selected.endswith('-pinned')
                            impl=selected.removesuffix('-pinned')
                            command=[str(binary),'--meta',str(DATA/'phase4/workloads'/description['name']/(description['name']+'.meta.json')),
                                '--impl',impl,'--threads',str(count),'--pin',str(int(pinned)),'--cpus',','.join(map(str,eligible)),
                                '--fingerprint',fp,'--require-ac',str(int(sys.platform=='win32'))]
                            before=swap_status()['used_kb']
                            rows,metadata=execute(command,perf=bool(use_perf and sys.platform.startswith('linux')))
                            after=swap_status()['used_kb']
                            reasons=[]
                            if sys.platform.startswith('linux') and before!=after:reasons.append('swap_usage_changed')
                            if metadata['per_worker_binding_verified']!=pinned or metadata['eligible_cpus']!=eligible:
                                reasons.append('affinity_verification_failed')
                            if not metadata['eligibility_verified']:reasons.append('eligibility_verification_failed')
                            if index<0:continue
                            evidence.write(json.dumps(dict(round=index+1,order=order,impl=label,threads=count,**metadata))+'\n')
                            for row in rows:
                                if reasons:row.update(valid='0',invalid_reason=';'.join(reasons))
                                row.update(round=index+1,order=order,impl=label,workload=description['name'],workload_fingerprint=fp,
                                    threads=count,mode='na',swap_used_before_kb=before,swap_used_after_kb=after,
                                    pinned=int(pinned),pinned_cpus=';'.join(str(eligible[t%len(eligible)]) for t in range(count)) if pinned else '')
                                writer.writerow(row)
                            stream.flush();evidence.flush()
            summarize(directory,'packed')
            original_summary=(directory/'summary.csv').read_bytes()
            summarize(directory,'packed-pinned')
            (directory/'summary-pinned.csv').write_bytes((directory/'summary.csv').read_bytes())
            (directory/'summary.csv').write_bytes(original_summary)
            (directory/'README.md').write_text(readme.replace('Status: running.','Status: complete. stub=0.'))
            print(f'[{group_index}/{group_count}] {case} completed',flush=True)
        status='complete'
    finally:
        if original_plan is not None:
            subprocess.run(['powercfg','/setactive',original_plan],check=True)
            restored=power_scheme()
            (output/'restored-power-plan.txt').write_text(restored+'\n')
            if restored.lower()!=original_plan.lower():raise RuntimeError('power plan restoration verification failed')
        (output/'STATUS.json').write_text(json.dumps({'status':status,'power_plan_restored':original_plan is not None})+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('prepare','measure'))
    parser.add_argument('--binary',type=Path)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--cases',nargs='+',choices=list(CASES),default=list(CASES))
    parser.add_argument('--threads',nargs='+',type=int,default=[1,2,4,8])
    parser.add_argument('--no-perf',action='store_true')
    args=parser.parse_args()
    if args.action=='prepare':prepare()
    else:
        if not args.binary or not args.output:parser.error('--binary and --output required')
        measure(args.binary.resolve(),args.output,case_names=args.cases,threads=args.threads,use_perf=not args.no_perf)

if __name__=='__main__':main()
