"""Revalidate packed outputs and assemble repository-safe statistics (no asset data)."""
import argparse
import json
import math
import os
from pathlib import Path
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from bench.workload import sha256
from bench.workload.formats import AABB, brute_pairs, grid_pairs, read_aabbs, records


def validate(root, source, oracle):
    results = []
    expected = {'BistroInterior':1046609, 'BistroInterior_Wine':1320323, 'BistroExterior':2832120}
    for name, count in expected.items():
        meta = json.loads((root/(name+'.meta.json')).read_text())
        assert meta['triangle_count'] == count
        assert sha256(source/meta['source']) == meta['source_sha256']
        unions = []
        for suffix, expected_count in [('instances',meta['object_count']),('triangles',count)]:
            path = root/f'{name}.{suffix}.bin'
            assert sha256(path) == meta['files'][path.name]
            assert path.stat().st_size == expected_count*AABB.size
            union = [float('inf')]*3 + [-float('inf')]*3
            for b in records(path, AABB):
                assert all(math.isfinite(v) for v in b)
                assert all(b[k] <= b[k+3] for k in range(3))
                for k in range(3):
                    union[k] = min(union[k], b[k])
                    union[k+3] = max(union[k+3], b[k+3])
            # JSON decimal roundtrip is within a float32 ULP; binary union is exact across granularities.
            assert all(abs(a-b) <= 1e-6*max(1,abs(a)) for a,b in zip(union,meta['overall_aabb']))
            unions.append(union)
        assert unions[0] == unions[1]
        boxes = read_aabbs(root/f'{name}.instances.bin')
        brute, grid = brute_pairs(boxes), grid_pairs(boxes)
        native = json.loads(subprocess.check_output([str(oracle),str(root/f'{name}.instances.bin')],text=True))
        assert brute == grid == native['overlap_pairs']
        meta['instance_answer_check'] = dict(overlap_pairs=brute, methods_agree=[
            'python_brute_force','python_adaptive_grid_bvh','independent_cpp_grid_bvh'],
            oracle_settings={k:v for k,v in native.items() if k not in {'overlap_pairs','object_count'}})
        results.append(meta)
        print(f'{name}: hashes, counts, union and instance answers validated', file=sys.stderr)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, default=Path(os.environ.get('ECL_DATA',Path.home()/'data/engine-core-labs')))
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--answer-meta',type=Path)
    parser.add_argument('--answer-time',type=Path)
    args = parser.parse_args()
    result = dict(format_version=1, purpose='offline preprocessing and independent-answer correctness; no experiment performance conclusions',
                  scenes=validate(args.data/'bistro/aabb',args.data/'bistro/extracted/Bistro_v5_2',
                                  args.data/'tools/bin'/('aabb_reference.exe' if os.name=='nt' else 'aabb_reference')))
    if args.answer_meta:
        result['interior_triangle_workload'] = json.loads(args.answer_meta.read_text())
    if args.answer_time:
        # GNU time -v describes the complete generator process and its waited-for oracle child.
        lines = args.answer_time.read_text().splitlines()
        elapsed = next(v.split('): ',1)[1] for v in lines if 'Elapsed (wall clock)' in v)
        rss = next(int(v.rsplit(':',1)[1]) for v in lines if 'Maximum resident set size' in v)
        status = next(int(v.rsplit(':',1)[1]) for v in lines if 'Exit status:' in v)
        wall_seconds = 0.0
        for part in elapsed.split(':'):
            wall_seconds = wall_seconds*60 + float(part)
        assert status == 0 and wall_seconds <= 1800
        result['answer_budget_validation'] = dict(scope='whole Python generation plus C++ oracle child',
            wall_time_hms=elapsed, wall_seconds=wall_seconds, budget_met=True, max_rss_kib=rss, exit_status=status,
            command='python3 -m bench.workload generate workloads/bistro/bistro-interior-triangles.json --out "$ECL_DATA/scratch/bistro-interior-triangles"',
            method='/usr/bin/time -v; max_rss is maximum across parent/waited children, not simultaneous summed RSS',
            machine='tx', platform='Linux; GCC 13.3 Release; Python 3.12.3',
            code_version='engine-core-labs@a452624 + uncommitted phase C changes',
            budget_seconds=1800)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')


if __name__ == '__main__':
    main()
