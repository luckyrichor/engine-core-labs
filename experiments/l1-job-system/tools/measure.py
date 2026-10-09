"""Measure the same executor with/without stealing; preserve source and host evidence."""
import argparse, csv, hashlib, io, itertools, json, os, platform, statistics, subprocess, time, resource
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--build',type=Path,default=Path('build'))
p.add_argument('--output',type=Path,default=Path('.local/l1-results'))
p.add_argument('--tasks',type=int,default=10000)
p.add_argument('--iterations',type=int,default=50000)
p.add_argument('--repeats',type=int,default=10)
p.add_argument('--min-seconds',type=float,default=.2)
p.add_argument('--implementation-label',default='codex-core')
p.add_argument('--distributions',nargs='+',choices=['balanced','skewed','heterogeneous','heterogeneous_random'],default=['balanced','skewed','heterogeneous'])
p.add_argument('--seed',type=int,default=20261009)
a=p.parse_args()
if not 2<=a.repeats<=100 or not 1<=a.tasks<=10000000 or not 1<=a.iterations<=100000000 or not 0<=a.min_seconds<=10:p.error('invalid measurement bounds')
if not 0<=a.seed<=2**64-a.repeats:p.error('seed range')
max_iterations=1000000 if any(d.startswith('heterogeneous') for d in a.distributions) else 100000000
if a.iterations>max_iterations:p.error('iterations exceed distribution bounds')
exe=str(a.build/'experiments/l1-job-system/job_benchmark')
subprocess.run(['ctest','--test-dir',str(a.build),'--output-on-failure'],check=True)
def run(mode,threads,distribution,phase,iterations,seed=0):
 out=subprocess.check_output([exe,mode,str(threads),str(a.tasks),str(iterations),distribution,phase,str(seed)],text=True)
 return next(csv.DictReader(io.StringIO(out)))
def host():
 pressure=Path('/proc/pressure/cpu')
 return {'time':time.time(),'loadavg':os.getloadavg(),'cpu_pressure':pressure.read_text() if pressure.exists() else None,'proc_stat':Path('/proc/stat').read_text().splitlines()[0], 'children_cpu_s': resource.getrusage(resource.RUSAGE_CHILDREN).ru_utime+resource.getrusage(resource.RUSAGE_CHILDREN).ru_stime}
iterations=a.iterations
calibration=[]
# Calibrate the fastest observed balanced prequeued run, including the 8-worker case.
if a.min_seconds:
 for attempt in range(5):
  samples=[run('stealing',n,'balanced','prequeued',iterations) for n in (1,2,4,8)]
  fastest=min(float(r['seconds']) for r in samples)
  calibration.append({'iterations':iterations,'runs':samples})
  if fastest>=a.min_seconds:break
  iterations=min(max_iterations,max(iterations+1,int(iterations*a.min_seconds/max(fastest,.000001)*1.15)))
 else:raise SystemExit('calibration did not reach minimum duration')
expected={(d,seed):subprocess.check_output([exe,'reference',str(a.tasks),str(iterations),d,str(seed)],text=True).strip() for d in a.distributions for seed in (range(a.seed,a.seed+a.repeats) if d=='heterogeneous_random' else [0])}
a.output.mkdir(parents=True,exist_ok=True)
configs=list(itertools.product(a.distributions,('baseline','stealing'),(1,2,4,8),('prequeued','end_to_end')))
rows=[];observations=[]
with (a.output/'raw.csv').open('w') as f:
 writer=None
 for repeat in range(a.repeats):
  order=configs[repeat:]+configs[:repeat]
  if repeat%2:order=list(reversed(order))
  for distribution,mode,threads,phase in order:
   seed=a.seed+repeat if distribution=='heterogeneous_random' else 0
   before=host();row=run(mode,threads,distribution,phase,iterations,seed);after=host()
   if row['checksum']!=expected[(distribution,seed)] or row['exactly_once']!='true':raise SystemExit('independent checksum / task identity check failed')
   row['repeat']=repeat;rows.append(row)
   observations.append({'config':[distribution,mode,threads,phase,repeat],'before':before,'after':after})
   if writer is None:writer=csv.DictWriter(f,fieldnames=list(row));writer.writeheader()
   writer.writerow(row);f.flush()
  print(f'completed repeat {repeat+1}/{a.repeats}',flush=True)
summary=[]
for distribution,mode,threads,phase in configs:
 group=[r for r in rows if (r['distribution'],r['mode'],int(r['threads']),r['phase'])==(distribution,mode,threads,phase)]
 values=[float(r['tasks_per_second']) for r in group];mean=statistics.mean(values);sd=statistics.stdev(values)
 summary.append(dict(distribution=distribution,mode=mode,threads=threads,phase=phase,runs=len(values),median_tasks_per_second=statistics.median(values),mean_tasks_per_second=mean,stdev=sd,cv=sd/mean,min_seconds=min(float(r['seconds']) for r in group),max_seconds=max(float(r['seconds']) for r in group)))
with (a.output/'summary.csv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=list(summary[0]));w.writeheader();w.writerows(summary)
for distribution,phase in itertools.product(a.distributions,('prequeued','end_to_end')):
 group=[r for r in summary if r['distribution']==distribution and r['phase']==phase]
 maximum=max(r['mean_tasks_per_second']+r['stdev'] for r in group)
 parts=['<svg xmlns="http://www.w3.org/2000/svg" width="760" height="460"><rect width="760" height="460" fill="white"/>',f'<text x="55" y="25">{distribution} / {phase}: mean +/- sample SD, {a.repeats} runs</text>','<path d="M70 50V370H720" fill="none" stroke="black"/>']
 for mode,color in [('baseline','#777'),('stealing','#167ac6')]:
  points=[]
  for r in group:
   if r['mode']!=mode:continue
   x=70+(r['threads']-1)/7*640;y=370-r['mean_tasks_per_second']/maximum*310;dy=r['stdev']/maximum*310
   points.append(f'{x},{y}');parts.append(f'<path d="M{x} {y-dy}V{y+dy}" stroke="{color}"/><circle cx="{x}" cy="{y}" r="3" fill="{color}"/>')
  parts.append(f'<polyline points="{" ".join(points)}" fill="none" stroke="{color}"/><text x="80" y="{420 if mode=="baseline" else 445}" fill="{color}">{mode}</text>')
 for n in (1,2,4,8):parts.append(f'<text x="{70+(n-1)/7*640}" y="395">{n}</text>')
 for frac in (0,.5,1):parts.append(f'<text x="2" y="{370-frac*310}">{maximum*frac:.0f}</text>')
 parts.append('</svg>');(a.output/f'{distribution}-{phase}.svg').write_text('\n'.join(parts))
sources=[Path('CMakeLists.txt')]+[q for q in Path('experiments').rglob('*') if q.is_file() and (q.suffix in {'.hpp','.cpp','.py'} or q.name=='CMakeLists.txt')]
metadata={'generated_at':time.strftime('%Y-%m-%dT%H:%M:%S%z'),'machine':platform.node(),'os':platform.platform(),'logical_cpus':os.cpu_count(),'lscpu':subprocess.check_output(['lscpu'],text=True),'compiler':subprocess.check_output(['c++','--version'],text=True),'requested':vars(a)|{'build':str(a.build),'output':str(a.output)},'actual_tasks':a.tasks,'actual_iterations':iterations,'independent_checksum':{f'{d}/{seed}':v for (d,seed),v in expected.items()},'calibration':calibration,'observations':observations,'source_head':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'uncommitted':subprocess.check_output(['git','status','--short'],text=True),'load_interpretation':'loadavg is a lagging host average, not a measurement of background work; proc_stat and children CPU are interval evidence, not per-thread attribution', 'source_sha256':{str(q):hashlib.sha256(q.read_bytes()).hexdigest() for q in sources},'executable_sha256':hashlib.sha256(Path(exe).read_bytes()).hexdigest(),'build_cache':(a.build/'CMakeCache.txt').read_text(),'method':'full workload warmup on same pool; rotated/reversed order; prequeued excludes submission, includes resume/work/future drain; end_to_end includes submission; atomic per-ID exactly-once counts; independently computed sequential sum; SD is not a confidence interval'}
(a.output/'device.json').write_text(json.dumps(metadata,indent=2))
print(f'Saved {len(rows)} runs to {a.output}')
