"""Paired controls; real warm files, synthetic decode and paced frames."""
import argparse,csv,io,json,subprocess,hashlib,statistics,platform,time,os
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--output',type=Path,default=Path('.local/l3-review'))
p.add_argument('--build',type=Path,default=Path('build'))
p.add_argument('--repeats',type=int,default=10)
p.add_argument('--iterations',type=int,default=500000)
a=p.parse_args()
if not 2<=a.repeats<=100 or not 0<=a.iterations<=10000000:p.error('measurement bounds')
output=a.output;output.mkdir(parents=True,exist_ok=True)
assets=Path('.local/l3-assets');assets.mkdir(parents=True,exist_ok=True)
for i in range(32):(assets/f'{i}.bin').write_bytes(bytes([i])*65536)
exe=a.build/'experiments/l3-resource-loading/resource_bench'
configs=[('sync_a','sync',4,'vector'),('sync_b','sync',4,'vector'),
         ('async_a','async',4,'vector'),('async_b','async',4,'vector'),
         ('async_3','async',3,'vector'),('arena_4','async',4,'arena'),
         ('arena_3','async',3,'arena'),('preload','preload',4,'vector')]
def host():
 stat=Path('/proc/stat');pressure=Path('/proc/pressure/cpu')
 return dict(time=time.time(),loadavg=os.getloadavg(),cpu_ticks=[int(v) for v in stat.read_text().splitlines()[0].split()[1:9]] if stat.exists() else None,
             cpu_pressure=pressure.read_text() if pressure.exists() else None)
def interval(before,after):
 if before['cpu_ticks'] is None:return dict(steal_ticks=None,steal_ms=None,steal_pct=None)
 delta=[b-a for a,b in zip(before['cpu_ticks'],after['cpu_ticks'])]
 return dict(steal_ticks=delta[7],steal_ms=delta[7]*1000/os.sysconf('SC_CLK_TCK'),steal_pct=100*delta[7]/max(1,sum(delta)))
def save(name,rows):
 with (output/name).open('w') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
def pct(v,q):return sorted(v)[int(q*(len(v)-1))]
rows=[];summary=[];observations=[]
for repeat in range(a.repeats):
 order=configs[repeat%len(configs):]+configs[:repeat%len(configs)]
 if repeat%2:order=list(reversed(order))
 checks=[]
 for label,mode,workers,backend in order:
  before=host()
  result=list(csv.DictReader(io.StringIO(subprocess.check_output([str(exe),mode,str(assets),str(a.iterations),str(workers),backend],text=True))))
  after=host();evidence=interval(before,after)
  for row in result:row.update(repeat=repeat,label=label)
  frame=[r for r in result if int(r['frame'])>=0];latency=[r for r in result if r['frame']=='-2'];total=result[-1]
  assert len(frame)==96 and len(latency)==32 and len({r['asset_id'] for r in latency})==32
  checks.append(total['checksum']);values=[float(r['frame_ms']) for r in frame]
  summary.append(dict(label=label,mode=mode,workers=workers,backend=backend,repeat=repeat,p50_ms=statistics.median(values),p99_ms=pct(values,.99),max_ms=max(values),setup_ms=total['setup_ms'],startup_ms=total['startup_ms'],drain_ms=total['drain_ms'],missing_asset_frames=total['missing'],demand_p50_ms=statistics.median(float(r['demand_to_install_ms']) for r in latency),demand_p99_ms=pct([float(r['demand_to_install_ms']) for r in latency],.99),demand_max_ms=max(float(r['demand_to_install_ms']) for r in latency),request_p99_ms=pct([float(r['request_to_install_ms']) for r in latency],.99),checksum=total['checksum'],**evidence))
  observations.append(dict(label=label,repeat=repeat,before=before,after=after,**evidence));rows.extend(result)
 assert len(set(checks))==1
 print(f'completed L3 {repeat+1}/{a.repeats}',flush=True)
save('raw.csv',rows);save('summary.csv',summary)
pairs=[]
for repeat in range(a.repeats):
 group={r['label']:r for r in summary if r['repeat']==repeat}
 for first,second in [('sync_a','sync_b'),('async_a','async_b'),('async_a','async_3'),('async_a','arena_4'),('async_3','arena_3')]:
  pairs.append(dict(repeat=repeat,first=first,second=second,p99_ratio=float(group[first]['p99_ms'])/float(group[second]['p99_ms']),demand_p99_ratio=group[first]['demand_p99_ms']/max(1e-9,group[second]['demand_p99_ms'])))
save('paired.csv',pairs)
(output/'device.json').write_text(json.dumps(dict(source_head=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),dirty=subprocess.check_output(['git','status','--short'],text=True),host=platform.platform(),logical_cpus=os.cpu_count(),generated_at=time.strftime('%Y-%m-%dT%H:%M:%S%z'),source_sha256={str(q):hashlib.sha256(q.read_bytes()).hexdigest() for q in Path('experiments').rglob('*') if q.suffix in ('.hpp','.cpp','.py')},executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),observations=observations,iterations=a.iterations,method='32x64KiB warm files; 96 frames paced 5ms, sleep200us base work; demand4/8frames, max8 pending and2 installs/frame; include pool/thread creation in setup/startup; preload latencies occur BEFORE gameplay; frame_ms excludes pacing sleep. 32 per-asset timestamps include main-thread installation and deferred commits, demand includes capacity retries, accepted request excludes them. Two identical sync and async controls, rotated/reversed order. Linux aggregate CPU steal is guest-reported vCPU stolen ticks (not per-thread attribution); zero does not prove no contention. Ratios based on 96-frame order statistic are noisy, not causal proof.'),indent=2))
