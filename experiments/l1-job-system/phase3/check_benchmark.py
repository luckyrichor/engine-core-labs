"""Bounded correctness checks for every configured implementation and mode."""
import csv
import io
import subprocess
import sys
import tempfile
from pathlib import Path
from bench.workload import generate
from bench.run import parse_output

with tempfile.TemporaryDirectory() as temp:
    generate('workloads/tasks-small.json', temp)
    meta = str(Path(temp)/'tasks-small.meta.json')
    for impl in sys.argv[2:]:
        for threads in (1, 4):
            for mode in ('prequeued', 'end_to_end'):
                command = [sys.argv[1], '--meta', meta, '--impl', impl,
                           '--threads', str(threads), '--mode', mode,
                           '--producers', '4', '--block', '1']
                result = subprocess.run(command, text=True, capture_output=True, timeout=15)
                assert result.returncode == 0, (command, result.stderr)
                rows = parse_output(result.stdout)
                assert all(r['valid'] == '1' and r['stub'] == '0' for r in rows)
                assert next(float(r['value']) for r in rows if r['metric'] == 'submitted_tasks') == 2000
                if impl in ('legacy', 'sleepers'):
                    assert next(float(r['value']) for r in rows if r['metric'] == 'submit_batch_samples') >= 60
        # Last partial block and whole-range single block, including taskflow's interface.
        for block in (3, 4096):
            result = subprocess.run([sys.argv[1], '--meta', meta, '--impl', impl,
                                     '--block', str(block), '--threads', '2'],
                                    text=True, capture_output=True, timeout=15)
            assert result.returncode == 0, (impl, block, result.stderr)
            parse_output(result.stdout)
    result = subprocess.run([sys.argv[1], '--meta', meta, '--fingerprint', 'wrong'],
                            text=True, capture_output=True)
    assert result.returncode != 0 and 'fingerprint mismatch' in result.stderr
print('All configured schedulers, start modes, producer stress and partial blocks validated')
