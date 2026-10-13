"""Real file reads, synthetic CPU decode, paced synthetic frames; no cold-I/O claim."""
import csv,io,json,subprocess,hashlib,statistics,platform,time
from pathlib import Path
output=Path('docs/measurements/2026-10-13-l3');output.mkdir(parents=True,exist_ok=True)
assets=Path('.local/l3-assets');assets.mkdir(parents=True,exist_ok=True)
for i in range(32):(assets/f'{i}.bin').write_bytes(bytes([i])*65536)
exe=Path('build/experiments/l3-resource-loading/resource_bench');rows=[];totals=[]
for repeat in range(10):
 modes=['sync','async','preload'];modes=modes[repeat%3:]+modes[:repeat%3]
 for mode in modes:
  result=list(csv.DictReader(io.StringIO(subprocess.check_output([str(exe),mode,str(assets),'500000'],text=True))))
  for row in result:row['repeat']=repeat
  rows.extend(result);totals.append(result[-1])
 assert len(set(r['checksum'] for r in totals[-3:]))==1
 print(f'completed L3 {repeat+1}/10',flush=True)
with (output/'frames.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
summary=[]
for r in totals:
 values=sorted(float(x['frame_ms']) for x in rows if x['mode']==r['mode'] and x['repeat']==r['repeat'] and int(x['frame'])>=0)
 summary.append(dict(mode=r['mode'],repeat=r['repeat'],p50_ms=statistics.median(values),p99_ms=values[int(.99*(len(values)-1))],max_ms=max(values),startup_ms=r['startup_ms'],drain_ms=r['drain_ms'],missing_asset_frames=r['missing'],checksum=r['checksum']))
with (output/'summary.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=list(summary[0]));w.writeheader();w.writerows(summary)
(output/'device.json').write_text(json.dumps({'source_head':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'dirty':subprocess.check_output(['git','status','--short'],text=True),'host':platform.platform(),'generated_at':time.strftime('%Y-%m-%dT%H:%M:%S%z'),'source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in Path('experiments').rglob('*') if p.suffix in ('.hpp','.cpp','.py')},'method':'32x64KiB files, synthetic decode 500000 iterations; 96 frames paced at 5ms, 200us sleep as simulated base frame work; demand four more assets every eight frames; async max eight in flight, two commits/frame; preload startup excluded from frames but reported; drains separately reported; real file reads with warm OS cache (newly generated fixtures), not image/mesh codec or GPU uploads; ten rotated-order runs'},indent=2))
