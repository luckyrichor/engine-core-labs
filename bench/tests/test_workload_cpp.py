"""Invoked by ctest with the built C++ reader; no compiler needed by unittest."""
import json
import subprocess
import sys
from pathlib import Path

from bench.workload import generate
from bench.tests.test_workload import temporary_directory


def main():
    executable = sys.argv[1]
    root = Path(__file__).resolve().parents[2]
    with temporary_directory() as temp:
        for descriptor in sorted((root/'workloads').glob('*.json')):
            out = Path(temp)/descriptor.stem
            meta = generate(descriptor,out)
            path = out/(descriptor.stem+'.meta.json')
            result = subprocess.run([executable,str(path)],capture_output=True,text=True,check=True)
            assert json.loads(result.stdout) == meta['answer']
            if meta['files']:
                data = out/next(iter(meta['files']))
                original = data.read_bytes()
                data.write_bytes(original+b'\x00')
                result = subprocess.run([executable,str(path)],capture_output=True,text=True)
                assert result.returncode != 0 and 'SHA256 mismatch' in result.stderr, result
                data.write_bytes(original)
                # Rehash malformed data to test structural checks after integrity checks.
                from bench.workload import sha256
                if descriptor.stem in {'tasks-small','scene-small','trace-small'}:
                    data.write_bytes(original[:-1])
                    meta['files'][data.name]=sha256(data)
                    path.write_text(json.dumps(meta))
                    result=subprocess.run([executable,str(path)],capture_output=True,text=True)
                    assert result.returncode != 0 and 'truncated' in result.stderr, result
    print('C++ readers: all five kinds agree with independent answers; corruption/truncation rejected')


if __name__ == '__main__':
    main()
