"""CTest correctness harness. Each test uses freshly generated, verified data."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from bench.workload import generate, sha256


def main():
    if sys.argv[1] == 'skip':
        print('SKIP: external baseline not built: ' + sys.argv[2], flush=True)
        return 77
    kind, executable, *args = sys.argv[1:]
    root = Path(__file__).resolve().parents[3]
    name = 'tasks-small' if kind == 'scheduler' else 'trace-small'
    with tempfile.TemporaryDirectory(prefix='ecl-adapter-') as temp:
        out = Path(temp)
        meta = generate(root/'workloads'/f'{name}.json', out)
        path = out/f'{name}.meta.json'
        command = [executable, *args, str(path)]
        result = subprocess.run(command, capture_output=True, text=True, check=True)
        print(result.stderr, end='')
        value = json.loads(result.stdout)
        if kind == 'scheduler':
            assert value['tasks'] == meta['answer']['task_count']
            assert value['checksum'] == meta['answer']['cost_sum']
            assert value['exactly_once'] and value['work_verified']
        else:
            assert value['identity_verified'] and value['all_released']
            for field in ('allocations', 'frees', 'peak_live_bytes'):
                assert value[field] == meta['answer'][field]
        # Matching hashes are necessary but not enough: reject a wrong answer.
        field = 'cost_sum' if kind == 'scheduler' else 'peak_live_bytes'
        meta['answer'][field] += 1
        path.write_text(json.dumps(meta))
        result = subprocess.run(command, capture_output=True, text=True)
        assert result.returncode != 0 and 'mismatch' in result.stderr, result
        meta['answer'][field] -= 1
        path.write_text(json.dumps(meta))
        data = out/next(iter(meta['files']))
        original = data.read_bytes()
        data.write_bytes(original+b'bad')
        result = subprocess.run(command, capture_output=True, text=True)
        assert result.returncode != 0 and 'SHA256 mismatch' in result.stderr, result
        if kind == 'alloc':
            # Rehash malformed records to exercise semantics after integrity.
            invalid = bytearray(original)
            invalid[20] = 1  # first record attempts a free before any allocation
            data.write_bytes(invalid)
            meta['files'][data.name] = sha256(data)
            path.write_text(json.dumps(meta))
            result = subprocess.run(command, capture_output=True, text=True)
            assert result.returncode != 0 and 'free without live allocation' in result.stderr, result
            data.write_bytes(original[:-29])  # omit final release
            meta['files'][data.name] = sha256(data)
            path.write_text(json.dumps(meta))
            result = subprocess.run(command, capture_output=True, text=True)
            assert result.returncode != 0 and 'live allocations at end' in result.stderr, result
        print(json.dumps(value, sort_keys=True))
    return 0


if __name__ == '__main__':
    sys.exit(main())
