import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from bench.device import ROOT, collect, source_fingerprint
from bench.run import parse_output, run
from bench.schema import RAW_FIELDS


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)
        (self.path / 'small.json').write_text('{"name":"small","seed":7}')
        fake = str(ROOT / 'bench/tests/fake_bench.py')
        self.config = dict(story='S-test-stub', rounds=5, warmup_rounds=1, threads=[1, 2],
                           baseline='base', implementations=[
                               {'name': 'base', 'command': ['{python}', fake, '--value', '100', '--impl', '{impl}']},
                               {'name': 'candidate', 'command': ['{python}', fake, '--value', '130', '--impl', '{impl}']}],
                           workload_files=['small.json'], output_dir='results')
        self.config_path = self.path / 'config.json'

    def save(self):
        self.config_path.write_text(json.dumps(self.config))

    def test_cli_end_to_end_and_warmup_order(self):
        self.save()
        completed = subprocess.run([sys.executable, str(ROOT / 'bench/run.py'), str(self.config_path)],
                                   text=True, capture_output=True, check=True)
        directory = self.path / 'results'
        self.assertEqual(Path(completed.stdout.strip()), directory)
        for name in ('raw.csv', 'summary.csv', 'device.json', 'README.md', 'config.json'):
            self.assertTrue((directory / name).is_file())
        with (directory / 'raw.csv').open(newline='') as stream:
            reader = csv.DictReader(stream)
            self.assertEqual(reader.fieldnames, RAW_FIELDS)
            rows = list(reader)
        self.assertEqual(len(rows), 5 * 2 * 3)  # No warmup rows.
        self.assertTrue(all(r['stub'] == '1' for r in rows))
        self.assertTrue(all(r['valid'] == '1' for r in rows))
        self.assertEqual({r['round'] for r in rows}, set('12345'))
        for number in range(1, 6):
            names = ['base', 'candidate', 'base@aa']
            offset = (number - 1) % 3
            expected = names[offset:] + names[:offset]
            subset = [r for r in rows if r['round'] == str(number) and r['threads'] == '1']
            self.assertEqual([r['impl'] for r in subset], expected)
            self.assertEqual([r['order'] for r in subset], ['1', '2', '3'])
        self.assertEqual(len({r['workload_fingerprint'] for r in rows}), 1)
        device = json.loads((directory / 'device.json').read_text())
        self.assertTrue(device['source']['files'])
        self.assertIn('stub=1', (directory / 'README.md').read_text())
        subprocess.run([sys.executable, str(ROOT / 'bench/summarize.py'), str(directory), '--baseline', 'base'], check=True)
        with self.assertRaises(FileExistsError):
            run(self.config_path)

    def test_invalid_execution_and_swap_invalidate_all_metrics(self):
        self.config.update(warmup_rounds=0, threads=[1])
        for impl in self.config['implementations']:
            impl['command'] += ['--metric', 'latency_p99']
        self.config['implementations'][1]['command'] += ['--valid', '0', '--exit-code', '3']
        self.save()
        statuses = [{'used_kb': value} for _ in range(15) for value in (0, 1)]
        with patch('bench.run.swap_status', side_effect=statuses):
            directory = run(self.config_path)
        with (directory / 'raw.csv').open(newline='') as stream:
            rows = list(csv.DictReader(stream))
        self.assertEqual(len(rows), 30)
        self.assertTrue(all(r['valid'] == '0' and 'swap_usage_changed' in r['invalid_reason'] for r in rows))
        candidate = [r for r in rows if r['impl'] == 'candidate']
        self.assertTrue(all('fake_validation_failure' in r['invalid_reason'] and 'exit_code=3' in r['invalid_reason'] for r in candidate))

    def test_config_validation(self):
        for key, value in [('rounds', 4), ('threads', [0]), ('threads', [1, 1]),
                           ('warmup_rounds', -1), ('mode', 'unknown')]:
            with self.subTest(key=key, value=value):
                saved = self.config[key] if key in self.config else None
                self.config[key] = value
                self.save()
                with self.assertRaises(ValueError):
                    run(self.config_path)
                if saved is None:
                    del self.config[key]
                else:
                    self.config[key] = saved
        self.assertFalse((self.path / 'results').exists())

    def test_malformed_program_output_fails_closed(self):
        for stdout in ('', 'metric,value\nx,1\n',
                       'metric,value,unit,better,valid,invalid_reason,stub\nx,nan,u,higher,1,,1\n'):
            with self.assertRaises(ValueError):
                parse_output(stdout)
        self.config['implementations'][0]['command'] = ['{python}', '-c', 'print("bad header")']
        self.save()
        with self.assertRaises(ValueError):
            run(self.config_path)
        self.assertIn('Status: failed', (self.path / 'results/README.md').read_text())
        self.assertFalse((self.path / 'results/summary.csv').exists())

    def test_device_fingerprint(self):
        root = self.path / 'repo'
        (root / 'bench').mkdir(parents=True)
        (root / 'experiments').mkdir()
        (root / 'bench/z.py').write_text('z=1\n')
        (root / 'experiments/a.cpp').write_text('int x;\n')
        first = source_fingerprint(root)
        self.assertEqual(first, source_fingerprint(root))
        self.assertEqual(list(first['files']), sorted(first['files']))
        (root / 'bench/z.py').write_text('z=2\n')
        self.assertNotEqual(first['sha256'], source_fingerprint(root)['sha256'])
        (root / 'bench/z.py').rename(root / 'bench/y.py')
        self.assertNotEqual(first['sha256'], source_fingerprint(root)['sha256'])
        device = collect(compile_options=['-O2'], dataset_checksums={'tiny': 'abc'},
                         external_libraries=[{'name': 'fake', 'version': '1', 'configuration': 'stub'}],
                         threads=[( __import__('os').cpu_count() or 1) + 1])
        for field in ('machine', 'system', 'kernel', 'cpu_model', 'physical_cores',
                      'logical_threads', 'memory_total_kb', 'compiler', 'compile_options',
                      'source', 'git', 'dataset_checksums', 'external_libraries',
                      'oversubscribed', 'swap', 'transparent_hugepage', 'power_plan',
                      'performance_cpus', 'efficiency_cpus'):
            self.assertIn(field, device)
        self.assertTrue(device['oversubscribed'])
        self.assertEqual(device['compile_options'], ['-O2'])


if __name__ == '__main__':
    unittest.main()
