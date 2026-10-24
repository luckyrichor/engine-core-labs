"""Cross-check native oracle against Python brute force, including fallback paths."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from bench.workload.formats import AABB, brute_pairs, grid_pairs
from bench.workload.prng import Random

root = Path(os.environ.get('ECL_DATA', Path.home()/'data/engine-core-labs'))/'scratch'
root.mkdir(parents=True, exist_ok=True)
rng = Random(971)
with tempfile.TemporaryDirectory(dir=root) as temp:
    path = Path(temp)/'boxes.bin'
    sets = [[], [(0,)*6], [(0,0,0,0,0,0),(-1,-1,-1,0,0,0),(-100,-100,-100,100,100,100)]]
    boxes = []
    for _ in range(2000):
        lo = [rng.below(400)-200 for _ in range(3)]
        boxes.append(tuple(lo+[v+rng.below(10) for v in lo]))
    # Place oversized boxes before AND after regular ones; duplicates and touching faces.
    boxes.insert(0, (-500,-500,-500,500,500,500))
    boxes += [(0,0,0,0,0,0), (0,0,0,0,0,0), (-1000,-1,-1,1000,1,1)]
    sets.append(boxes)
    for boxes in sets:
        path.write_bytes(b''.join(AABB.pack(*b) for b in boxes))
        expected = brute_pairs(boxes)
        for cell in (None, 1, 16, 10000):
            command = [sys.argv[1], str(path)] + ([] if cell is None else [str(cell)])
            answer = json.loads(subprocess.check_output(command, text=True))
            assert answer['overlap_pairs'] == expected, (cell, expected, answer)
            assert grid_pairs(boxes, cell) == expected
    # Exercise generator -> native oracle -> deterministic metadata, not just CLI parsing.
    from bench.workload import generate, sha256
    from unittest.mock import patch
    path.write_bytes(b''.join(AABB.pack(*b) for b in boxes))
    descriptor = dict(name='native-answer', kind='aabb_scene',
        dataset=dict(name='test', version='1', checksum={str(path):sha256(path)}),
        filter={}, generator='ecl-v1', params=dict(input=str(path),answer_backend='cpp'),
        seed=1,scale=len(boxes),amplification=dict(factor=1,method='none'))
    config = Path(temp)/'descriptor.json'
    config.write_text(json.dumps(descriptor))
    with patch.dict(os.environ, {'ECL_AABB_REFERENCE':sys.argv[1]}):
        a = generate(config,Path(temp)/'first')
        b = generate(config,Path(temp)/'second')
    assert a == b
    assert a['answer']['overlap_pairs'] == brute_pairs(boxes)
    assert a['answer_method'] == 'independent_cpp_grid_bvh'
    assert a['oversized_fallback'] == 'static_median_bvh_leaf16'
    assert (Path(temp)/'first/native-answer.bin').read_bytes() == path.read_bytes()
    path.write_bytes(b'x')
    assert subprocess.run([sys.argv[1], str(path)], capture_output=True).returncode != 0
    path.write_bytes(AABB.pack(1,0,0,0,0,0))
    assert subprocess.run([sys.argv[1], str(path)], capture_output=True).returncode != 0
print('native/Python/brute agreement: empty, degenerate, touching, random, oversized; corrupt inputs rejected')
