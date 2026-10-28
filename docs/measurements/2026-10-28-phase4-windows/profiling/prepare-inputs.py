from pathlib import Path
import json
from bench.workload import generate
root=Path('E:/ecl-scratch/phase4-20261028/profiling')
root.mkdir(exist_ok=True)
for case,count in [('positive',10000000),('queue',1000000),('counters',300000),('stats',1000000)]:
 d=json.loads(Path('workloads/phase4/s8-'+case+'.json').read_text())
 d['name']='s8-profile-'+case;d['params']['iterations']=count;d['scale']=count
 file=root/(case+'.json');file.write_text(json.dumps(d,indent=2)+'\n')
 generate(file,root/case)
