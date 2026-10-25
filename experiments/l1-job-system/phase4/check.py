"""Layout, binding and payload contracts without performance assertions."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from bench.run import parse_output
from bench.workload import generate

binary = sys.argv[1]
layout = json.loads(subprocess.check_output([binary, '--inspect', '1'], text=True))
assert layout['cache_line'] == 64
assert layout['positive_stride'] == {'packed': 8, 'separated': 64}
assert layout['queue_metadata_bytes']['separated'] > layout['queue_metadata_bytes']['packed']
for variant in ('pending', 'next', 'wake'):
    assert layout['counter_offsets'][variant][0] > layout['counter_offsets']['packed'][0]
with tempfile.TemporaryDirectory() as temp:
    root = Path(temp)
    for scenario, variants in (('positive', ['packed', 'separated']),
                               ('cold', ['packed', 'separated']),
                               ('queue', ['packed', 'separated']),
                               ('counters', ['packed', 'pending', 'next', 'wake']),
                               ('stats', ['packed', 'separated'])):
        descriptor = dict(name='test-'+scenario, kind='constructive',
            dataset=dict(name='constructive-s8-contract',version='1',checksum='constructive'),
            filter={},generator='ecl-v1',params=dict(case=scenario,iterations=2000,work_units=4),
            seed=0,scale=2000,amplification=dict(factor=1,method='none'))
        file = root/(scenario+'.json');file.write_text(json.dumps(descriptor))
        meta = generate(file, root/scenario)
        path = root/scenario/(descriptor['name']+'.meta.json')
        for variant in variants:
            for threads in (1, 4):
                for pinned in (0, 1):
                    command = [binary, '--meta',str(path),'--impl',variant,'--threads',str(threads),
                               '--pin',str(pinned),'--fingerprint',meta['workload_fingerprint']]
                    result = subprocess.run(command,text=True,capture_output=True,timeout=15)
                    assert result.returncode == 0, (command,result.stderr)
                    rows = parse_output(result.stdout)
                    assert all(row['valid']=='1' and row['stub']=='0' for row in rows)
                    evidence = json.loads(next(line[8:] for line in result.stderr.splitlines() if line.startswith('S8_META:')))
                    assert evidence['eligibility_verified']
                    assert evidence['per_worker_binding_verified'] == bool(pinned)
        bad = subprocess.run([binary,'--meta',str(path),'--cpus','999999'],text=True,capture_output=True)
        assert bad.returncode != 0 and 'requested CPU not eligible' in bad.stderr
print('S8: 5 cases/12 variants, one/four workers, binding readback, exact payloads, invalid CPU rejected')
