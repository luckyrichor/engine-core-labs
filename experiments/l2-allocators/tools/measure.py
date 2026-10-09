"""Record batch allocator evidence, including clock controls and exact source."""
import argparse,csv,hashlib,io,json,os,platform,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,default=Path('build'));p.add_argument('--output',type=Path,default=Path('.local/l2-results'));a=p.parse_args()
folder=a.build/'experiments/l2-allocators';a.output.mkdir(parents=True,exist_ok=True)
before={'time':time.time(),'loadavg':os.getloadavg(),'pressure':Path('/proc/pressure/cpu').read_text()}
for name,filename in [('allocator_bench','raw.csv'),('fragmentation_demo','fragmentation.csv'),('clock_control','clock.csv')]:
 out=subprocess.check_output([str(folder/name)],text=True);(a.output/filename).write_text(out)
rows=list(csv.DictReader((a.output/'raw.csv').open()))
if len(rows)!=80:raise SystemExit('expected 80 batches of observations')
for r in rows:
 if int(r['checksum'])!=int(r['batches'])*32640 or int(r['batches'])<2000:raise SystemExit('invalid payload or sample count')
 if r['allocator'] in ('stack','arena') and r['pattern']=='odd' and int(r['internal_waste_bytes'])<=0:raise SystemExit('alignment workload did not trigger padding')
sources=[Path('CMakeLists.txt')]+[q for q in Path('experiments/l2-allocators').rglob('*') if q.is_file() and (q.suffix in {'.hpp','.cpp','.py'} or q.name=='CMakeLists.txt')]
meta={'generated_at':time.strftime('%Y-%m-%dT%H:%M:%S%z'),'head':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'uncommitted':subprocess.check_output(['git','status','--short'],text=True),'source_sha256':{str(q):hashlib.sha256(q.read_bytes()).hexdigest() for q in sources},'executable_sha256':{name:hashlib.sha256((folder/name).read_bytes()).hexdigest() for name in ('allocator_bench','fragmentation_demo','clock_control')},'machine':platform.node(),'os':platform.platform(),'logical_cpus':os.cpu_count(),'compiler':subprocess.check_output(['c++','--version'],text=True),'cache':(a.build/'CMakeCache.txt').read_text(),'before':before,'after':{'time':time.time(),'loadavg':os.getloadavg(),'pressure':Path('/proc/pressure/cpu').read_text()},'method':'256 allocations + byte writes per clock pair, 50 warmup batches; >=2000 batches and >=100ms per row; 10 rotated repeats; batch-mean percentiles, not individual latency; release semantics differ; malloc metadata unknown'}
(a.output/'device.json').write_text(json.dumps(meta,indent=2));print('Verified 80 rows and payload checksums')
