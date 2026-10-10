"""Linux cold-memory observations; one fresh process per mode/repeat, no warmup."""
import argparse,csv,hashlib,io,json,platform,statistics,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,default=Path('build'));p.add_argument('--output',type=Path,default=Path('.local/cold'));a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
exe=a.build/'experiments/l2-allocators/allocator_cold';rows=[]
for rep in range(10):
 modes=['first_touch','prefaulted','malloc_growth'];modes=modes[rep%3:]+modes[:rep%3]
 for mode in modes:
  data=list(csv.DictReader(io.StringIO(subprocess.check_output([str(exe),mode],text=True))))
  for row in data:row['repeat']=rep
  rows.extend(data)
def save(name,data):
 with (a.output/name).open('w') as f:w=csv.DictWriter(f,fieldnames=list(data[0]));w.writeheader();w.writerows(data)
save('raw.csv',rows);summary=[]
for mode in ['first_touch','prefaulted','malloc_growth']:
 for rep in range(10):
  group=[r for r in rows if r['mode']==mode and r['repeat']==rep];v=sorted(float(r['allocate_and_touch_ns']) for r in group)
  summary.append(dict(mode=mode,repeat=rep,samples=len(v),p50_ns=statistics.median(v),p99_ns=v[int(.99*(len(v)-1))],max_ns=max(v),minor_faults=sum(int(r['minor_faults']) for r in group),major_faults=sum(int(r['major_faults']) for r in group)))
save('summary.csv',summary)
(a.output/'device.json').write_text(json.dumps({'source_head':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'uncommitted':subprocess.check_output(['git','status','--short'],text=True),'host':platform.platform(),'source_sha256':{str(q):hashlib.sha256(q.read_bytes()).hexdigest() for q in [Path('experiments/l2-allocators/cold.cpp'),Path(__file__),Path('experiments/l2-allocators/allocators.hpp')]},'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'method':'fresh process per repeat/mode; 512 x 64 KiB retained live (32 MiB); touch every system page; mmap no pretouch vs prefaulted control; malloc allocate + page touch; no promise of sbrk growth, mmap use, or cold CPU caches; rusage outside timed region; ten independent process runs; cannot extrapolate to small custom allocator fast paths'},indent=2))
