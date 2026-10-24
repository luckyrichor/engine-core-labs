import csv
import json
import os
import struct
import tempfile
import unittest
import zipfile
from pathlib import Path

from bench.workload import fingerprint, generate, load, sha256
from bench.workload.formats import AABB, brute_pairs, grid_pairs, read_aabbs, read_trace, trace_answer, write_trace
from bench.workload.prng import Random

ROOT = Path(__file__).resolve().parents[2]
SCRATCH = Path(os.environ.get('ECL_DATA', Path.home()/'data/engine-core-labs'))/'scratch/workload-tests'


def temporary_directory():
    SCRATCH.mkdir(parents=True, exist_ok=True)
    return tempfile.TemporaryDirectory(dir=SCRATCH)


EXAMPLES = sorted((ROOT / 'workloads').glob('*.json'))


class WorkloadTests(unittest.TestCase):
    def descriptor(self, kind):
        return next(load(p) for p in EXAMPLES if load(p)['kind'] == kind)

    def run_descriptor(self, d, directory):
        path = Path(directory) / 'descriptor.json'
        path.write_text(json.dumps(d), encoding='utf-8')
        return generate(path, Path(directory) / 'out')

    def test_examples_byte_identical_and_fingerprint(self):
        for path in EXAMPLES:
            with self.subTest(path=path.name), temporary_directory() as temp:
                root = Path(temp)
                first = generate(path, root / 'a')
                generate(path, root / 'b')
                self.assertEqual({p.name: p.read_bytes() for p in (root/'a').iterdir()},
                                 {p.name: p.read_bytes() for p in (root/'b').iterdir()})
                d = load(path)
                d['seed'] += 1
                self.assertNotEqual(first['workload_fingerprint'], fingerprint(d))
                self.assertEqual(fingerprint(dict(reversed(list(d.items())))), fingerprint(d))

    def test_prng_golden(self):
        r = Random(1)
        self.assertEqual([r.next() for _ in range(3)],
                         [12966619160104079557, 9600361134598540522, 10590380919521690900])

    def test_task_uniform_and_histogram(self):
        d = self.descriptor('task_costs')
        d['params'] = dict(distribution='uniform', min=7, max=7)
        d['amplification'] = dict(factor=3, method='repeat')
        with temporary_directory() as temp:
            m = self.run_descriptor(d, temp)
            self.assertEqual(m['answer'], dict(task_count=6000, cost_sum=42000))
        fixture = ROOT/'workloads/fixtures/cost-histogram.json'
        d['params'] = dict(distribution='histogram', histogram=str(fixture))
        d['dataset']['checksum'] = {str(fixture): sha256(fixture)}
        with temporary_directory() as temp:
            self.run_descriptor(d, temp)
            values = [x[0] for x in struct.iter_unpack('<I', (Path(temp)/'out/tasks-small.bin').read_bytes())]
            self.assertTrue(all(1 <= x < 10 or 100 <= x < 201 for x in values))

    def test_aabb_grid_brute_2000_and_input(self):
        for distribution in ['uniform', 'clustered']:
            with self.subTest(distribution=distribution), temporary_directory() as temp:
                d = self.descriptor('aabb_scene')
                d['scale'] = 2000
                d['params']['distribution'] = distribution
                m = self.run_descriptor(d, temp)
                boxes = read_aabbs(Path(temp)/'out/scene-small.bin')
                self.assertEqual(brute_pairs(boxes), grid_pairs(boxes))
                self.assertEqual(m['answer']['overlap_pairs'], grid_pairs(boxes))
        with temporary_directory() as temp:
            file = Path(temp)/'source.bin'
            boxes = [(0,0,0,1,1,1), (1,1,1,2,2,2), (-1,-1,-1,0,0,0)]
            file.write_bytes(b''.join(AABB.pack(*b) for b in boxes))
            d['scale'] = 3
            d['params'] = dict(input=str(file), grid_spacing=100)
            d['dataset']['checksum'] = {str(file):sha256(file)}
            d['amplification'] = dict(factor=3, method='translate_grid')
            m = self.run_descriptor(d, temp)
            self.assertEqual(m['answer'], dict(object_count=9, overlap_pairs=6))

    def test_large_scene_reference(self):
        d = self.descriptor('aabb_scene')
        d['scale'] = 20001
        d['params'] = dict(distribution='uniform',extent_ticks=1<<22,max_size_ticks=1)
        with temporary_directory() as temp:
            m = self.run_descriptor(d, temp)
            self.assertEqual(m['answer_method'], 'grid_hash')
        # Degenerate boundaries, negative cells, and oversized-grid fallback.
        boxes = [(0,0,0,0,0,0),(-1,-1,-1,0,0,0),(-100,-100,-100,100,100,100)]
        self.assertEqual(brute_pairs(boxes), grid_pairs(boxes, 1))

    def test_trace_roundtrip_and_validation(self):
        with temporary_directory() as temp:
            d = self.descriptor('alloc_trace')
            d['amplification'] = dict(factor=2, method='repeat')
            m = self.run_descriptor(d, temp)
            file = Path(temp)/'out/trace-small.bin'
            trace = read_trace(file)
            target = Path(temp)/'trace.bin'
            write_trace(target, trace)
            self.assertEqual(target.read_bytes(), file.read_bytes())
            self.assertEqual(m['answer'], trace_answer(trace))
            self.assertEqual(m['answer']['allocations'], 64)
            self.assertEqual([(t-64,s,th,op,oid-32) for t,s,th,op,oid in trace[64:]], trace[:64])
            with self.assertRaises(ValueError):
                trace_answer(trace[:-1])
            target.write_bytes(b'\x00')
            with self.assertRaises(ValueError):
                read_trace(target)

    def test_resources_dependency_copy_zip_and_filters(self):
        d = self.descriptor('resource_manifest')
        # Use absolute fixture roots when descriptor is placed in a temporary directory.
        fixture = ROOT/'workloads/fixtures'
        inventory = str(fixture/'files.csv')
        d['params']['files_csv'] = inventory
        d['params']['root'] = str(fixture)
        d['dataset']['checksum'] = {inventory:sha256(inventory)}
        d['amplification'] = dict(factor=2, method='copy_manifest')
        with temporary_directory() as temp:
            m = self.run_descriptor(d, temp)
            with (Path(temp)/'out/resources-small.csv').open() as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(m['answer']['file_count'], 6)
            self.assertEqual(rows[2]['dependency_ids'], '0;1')
            self.assertEqual(rows[5]['dependency_ids'], '3;4')
            d['params']['target_files'] = 5
            m = self.run_descriptor(d,temp)
            self.assertEqual(m['answer']['file_count'], 5)
            d['filter'] = {'extensions':['.gltf']}
            with self.assertRaisesRegex(ValueError, 'dependency'):
                self.run_descriptor(d,temp)
            archive=Path(temp)/'bistro.zip'
            with zipfile.ZipFile(archive,'w') as z:
                z.writestr('Bistro/a.bin',b'123')
                z.writestr('Bistro/b,quoted.txt',b'12345')
            d['filter']={}
            d['scale']=2
            d['params']=dict(bistro_zip=str(archive),target_files=3)
            d['dataset']['checksum']={str(archive):sha256(archive)}
            m=self.run_descriptor(d,temp)
            self.assertEqual(m['answer'], dict(file_count=3,total_bytes=11))
            d['dataset']['checksum'][str(archive)]='0'*64
            with self.assertRaisesRegex(ValueError,'checksum mismatch'):
                self.run_descriptor(d,temp)

    def test_schema_rejects_ambiguous_inputs(self):
        d = self.descriptor('task_costs')
        d['scale'] = True
        with self.assertRaises(ValueError): fingerprint(d)
        d = self.descriptor('task_costs')
        d['extra'] = 1
        with self.assertRaises(ValueError): fingerprint(d)
        d = self.descriptor('task_costs')
        d['params']['bad'] = float('nan')
        with self.assertRaises(ValueError): fingerprint(d)

    def test_glb_json_dependencies_and_portable_data_root(self):
        from unittest.mock import patch
        with temporary_directory() as temp:
            root = Path(temp)
            payload = json.dumps({'asset':{'version':'2.0'}, 'buffers':[{'uri':'buffer.bin'}]}).encode()
            payload += b' ' * ((-len(payload)) % 4)
            glb = root/'model.glb'
            glb.write_bytes(b'glTF'+struct.pack('<II',2,20+len(payload))+
                            struct.pack('<II',len(payload),0x4e4f534a)+payload)
            (root/'buffer.bin').write_bytes(b'abcd')
            inventory = root/'files.csv'
            inventory.write_text(f'path,bytes,extension,model\nmodel.glb,{glb.stat().st_size},.glb,test\nbuffer.bin,4,.bin,test\n')
            d = self.descriptor('resource_manifest')
            d['scale'] = 2
            d['params'] = dict(files_csv='$ECL_DATA/files.csv',root='$ECL_DATA',
                               content_checksums={'model.glb':sha256(glb)})
            d['dataset']['checksum'] = {'$ECL_DATA/files.csv':sha256(inventory)}
            with patch.dict(os.environ, {'ECL_DATA':temp}):
                m = self.run_descriptor(d,temp)
            self.assertEqual(m['answer']['file_count'],2)
            with (root/'out/resources-small.csv').open() as stream:
                rows=list(csv.DictReader(stream))
            self.assertEqual(rows[1]['dependency_ids'],'0')
