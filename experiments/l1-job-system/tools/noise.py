"""A/A baseline controls: identical input, executable and policy, alternating order."""
import argparse,csv,hashlib,io,json,os,platform,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--build',type=Path,default=Path('build'))
p.add_argument('--output',type=Path,default=Path('.local/l1-noise'))
p.add_argument('--threads',type=int,nargs='+',default=[1,2,3,4,8])
p.add_argument('--tasks',type=int,default=10000)
p.add_argument('--iterations',type=int,default=50000)
p.add_argument('--repeats',type=int,default=10)
a=p.parse_args()
if not 2<=a.repeats<=100 or not all(1<=n<=256 for n in a.threads) or not 1<=a.tasks<=1000000 or not 1<=a.iterations<=100000000:p.error('bounds')
a.output.mkdir(parents=True,exist_ok=True);exe=a.build/'experiments/l1-job-system/job_benchmark'
expected=subprocess.check_output([str(exe),'reference',str(a.tasks),str(a.iterations),'balanced','0'],text=True).strip()
def ticks():return [int(v) for v in Path('/proc/stat').read_text().splitlines()[0].split()[1:9]]
rows=[];pairs=[]
for rep in range(a.repeats):
 for threads in a.threads:
  for phase in ['prequeued','end_to_end']:
   group={}
   for label in (['a','b'] if rep%2==0 else ['b','a']):
    before=ticks();start=time.time()
    row=next(csv.DictReader(io.StringIO(subprocess.check_output([str(exe),'baseline',str(threads),str(a.tasks),str(a.iterations),'balanced',phase,'0'],text=True))))
    after=ticks();delta=[b-a for a,b in zip(before,after)]
    assert row['checksum']==expected and row['exactly_once']=='true'
    row.update(label=label,repeat=rep,observed_at=start,steal_ticks=delta[7],steal_ms=delta[7]*1000/os.sysconf('SC_CLK_TCK'),steal_pct=100*delta[7]/max(1,sum(delta)))
    rows.append(row);group[label]=row
   pairs.append(dict(repeat=rep,threads=threads,phase=phase,a_over_b=float(group['a']['seconds'])/float(group['b']['seconds'])))
 print(f'completed A/A {rep+1}/{a.repeats}',flush=True)
for name,data in [('raw.csv',rows),('paired.csv',pairs)]:
 with (a.output/name).open('w') as f:w=csv.DictWriter(f,fieldnames=list(data[0]));w.writeheader();w.writerows(data)
(a.output/'device.json').write_text(json.dumps(dict(source_head=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),dirty=subprocess.check_output(['git','status','--short'],text=True),host=platform.platform(),logical_cpus=os.cpu_count(),args=vars(a)|dict(build=str(a.build),output=str(a.output)),source_sha256={str(q):hashlib.sha256(q.read_bytes()).hexdigest() for q in Path('experiments/l1-job-system').rglob('*') if q.suffix in ('.cpp','.hpp','.py')},executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),method='balanced identical baseline A/A, fixed input; alternating label order, same full-workload warmup as benchmark; independent reference and exactly-once; Linux aggregate steal ticks not per-thread evidence; pair ratios are noise observations, not confidence intervals'),indent=2))
