"""Paired four-worker prequeued scan; fixed seed shared by both policies."""
import argparse,csv,hashlib,io,json,platform,statistics,subprocess,time,os
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,default=Path('build'));p.add_argument('--output',type=Path,default=Path('.local/scan'));p.add_argument('--repeats',type=int,default=10);a=p.parse_args()
if not 2<=a.repeats<=100:p.error('repeats 2..100')
a.output.mkdir(parents=True,exist_ok=True);exe=a.build/'experiments/l1-job-system/job_benchmark'
configs=[('heterogeneous_random',r,m) for r in (1,10,50) for m in (10,100)]+[('uniform_cost',0,10),('uniform_cost',0,100)]
rows=[];ratios=[]
for rep in range(a.repeats):
 for d,r,m in configs[rep%len(configs):]+configs[:rep%len(configs)]:
  seed=20261009+rep;base=round(100000/(m+1)) if d=='uniform_cost' else 50000;args=['10000',str(base),d,str(seed),str(r),str(m)]
  expected=subprocess.check_output([str(exe),'reference',*args],text=True).strip();pair={}
  for mode in (('baseline','stealing') if rep%2==0 else ('stealing','baseline')):
   row=next(csv.DictReader(io.StringIO(subprocess.check_output([str(exe),mode,'4',*args[:3],'prequeued',*args[3:]],text=True))))
   if row['checksum']!=expected or row['exactly_once']!='true':raise RuntimeError('correctness')
   row.update(repeat=rep,heavy_permille=r,max_multiplier=m,loadavg=str(os.getloadavg()));rows.append(row);pair[mode]=row
  if pair['baseline']['shape_hash']!=pair['stealing']['shape_hash']:raise RuntimeError('pair shape')
  ratios.append(dict(distribution=d,heavy_permille=r,max_multiplier=m,repeat=rep,ratio=float(pair['stealing']['tasks_per_second'])/float(pair['baseline']['tasks_per_second'])))
 print(f'completed scan {rep+1}/{a.repeats}',flush=True)
def save(name,data):
 with (a.output/name).open('w') as f:w=csv.DictWriter(f,fieldnames=list(data[0]));w.writeheader();w.writerows(data)
save('raw.csv',rows);save('paired.csv',ratios)
summary=[]
for d,r,m in configs:
 v=[x['ratio'] for x in ratios if (x['distribution'],x['heavy_permille'],x['max_multiplier'])==(d,r,m)]
 summary.append(dict(distribution=d,heavy_permille=r,max_multiplier=m,median_ratio=statistics.median(v),min_ratio=min(v),max_ratio=max(v),slower=sum(x<1 for x in v),runs=len(v)))
save('summary.csv',summary)
meta={'source_head':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'uncommitted':subprocess.check_output(['git','status','--short'],text=True),'host':platform.platform(),'cpu_count':os.cpu_count(),'generated_at':time.strftime('%Y-%m-%dT%H:%M:%S%z'),'source_sha256':{str(q):hashlib.sha256(q.read_bytes()).hexdigest() for q in Path('experiments').rglob('*') if q.suffix in ('.cpp','.hpp','.py')},'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'method':'10000 tasks, 50000 base iterations for two-point shapes; uniform base rounded from 100000/(multiplier+1) to hold expected mean cost near 50000, four workers, prequeued; ten distinct seeds shared per pair, alternating policy order; full warmup; costs vary across configurations, compare paired ratios only'}
(a.output/'device.json').write_text(json.dumps(meta,indent=2))
