import csv
import json
from pathlib import Path
import tempfile
import unittest

from bench.schema import RAW_FIELDS, SUMMARY_FIELDS
from bench.summarize import summarize


class SummaryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)
        (self.path / 'device.json').write_text(json.dumps({'logical_threads': 4}))

    def make_rows(self, values=None, noise=None, rounds=10, better='higher', metric='rate'):
        values = values if values is not None else [130] * rounds
        noise = noise if noise is not None else [100] * rounds
        rows = []
        for name, numbers in [('base', [100] * rounds), ('base@aa', noise), ('candidate', values)]:
            for index, value in enumerate(numbers, 1):
                row = dict.fromkeys(RAW_FIELDS, '')
                row.update(round=index, order=1, impl=name, workload='small',
                           workload_fingerprint='abc', threads=2, mode='na', metric=metric,
                           value=value, unit='units', better=better, valid=1, stub=0,
                           swap_used_before_kb=0, swap_used_after_kb=0, pinned=0)
                rows.append(row)
        return rows

    def flags_for(self, threads):
        rows = self.make_rows()
        for row in rows:
            row['threads'] = threads
        return set(filter(None, self.report(rows)['flags'].split(';')))

    def test_oversubscription_uses_physical_cores(self):
        # SMT host: 2 physical cores, 4 logical threads. 3 threads exceed the cores.
        (self.path / 'device.json').write_text(json.dumps({'physical_cores': 2, 'logical_threads': 4}))
        self.assertIn('oversubscribed', self.flags_for(3))
        self.assertNotIn('oversubscribed', self.flags_for(2))

    def test_hybrid_cores_flag(self):
        # luowindows layout: 24 physical cores, 32 threads, 16 performance-core threads.
        (self.path / 'device.json').write_text(json.dumps({
            'physical_cores': 24, 'logical_threads': 32,
            'performance_cpus': list(range(16)), 'efficiency_cpus': list(range(16, 32))}))
        self.assertEqual(self.flags_for(16) & {'hybrid_cores', 'oversubscribed'}, set())
        self.assertEqual(self.flags_for(20) & {'hybrid_cores', 'oversubscribed'}, {'hybrid_cores'})
        self.assertEqual(self.flags_for(28) & {'hybrid_cores', 'oversubscribed'}, {'hybrid_cores', 'oversubscribed'})

    def report(self, rows, impl='candidate', metric='rate'):
        with (self.path / 'raw.csv').open('w', newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=RAW_FIELDS)
            writer.writeheader()
            writer.writerows(rows)
        result = summarize(self.path, 'base')
        with (self.path / 'summary.csv').open(newline='') as stream:
            self.assertEqual(csv.DictReader(stream).fieldnames, SUMMARY_FIELDS)
        return next(row for row in result if row['impl'] == impl and row['metric'] == metric)

    def test_all_verdicts_and_threshold(self):
        for value, expected in [(130, 'significant_improvement'), (70, 'significant_regression'),
                                (110, 'no_significant_difference'), (90, 'no_significant_difference'),
                                (100, 'no_significant_difference')]:
            with self.subTest(value=value):
                row = self.report(self.make_rows(values=[value] * 10))
                self.assertEqual(row['verdict'], expected)
                self.assertEqual(row['ratio_median'], value / 100)
        row = self.report(self.make_rows(rounds=4))
        self.assertEqual(row['verdict'], 'insufficient_rounds')

    def test_lower_direction(self):
        for value, expected in [(70, 'significant_improvement'), (130, 'significant_regression')]:
            row = self.report(self.make_rows(values=[value] * 10, better='lower'))
            self.assertEqual(row['verdict'], expected)
            self.assertEqual(row['ratio_median'], value / 100)

    def test_noise_overlap_including_touch(self):
        for noise in ([130] * 10, [120, 140] * 5):
            row = self.report(self.make_rows(noise=noise))
            self.assertEqual(row['verdict'], 'no_significant_difference')
        row = self.report(self.make_rows(values=[70] * 10, noise=[70, 80] * 5))
        self.assertEqual(row['verdict'], 'no_significant_difference')

    def test_flags(self):
        rows = self.make_rows(rounds=5)
        rows = [r for r in rows if r['impl'] != 'base@aa']
        for row in rows:
            row.update(threads=8, stub=1)
        row = self.report(rows)
        self.assertEqual(row['verdict'], 'no_significant_difference')
        self.assertEqual(set(row['flags'].split(';')), {'below_10_rounds', 'oversubscribed', 'stub', 'no_aa_group'})
        row = self.report(self.make_rows(noise=[100] * 4))
        self.assertIn('insufficient_aa_rounds', row['flags'])
        self.assertEqual(row['verdict'], 'no_significant_difference')
        self.assertNotIn('below_10_rounds', self.report(self.make_rows())['flags'])
        self.assertIn('below_10_rounds', self.report(self.make_rows(rounds=9))['flags'])

    def test_invalid_rows_and_pair_by_round(self):
        rows = self.make_rows(values=[130, 140, 150, 160, 170, 180, 190, 200, 210, 999999])
        rows[-1].update(valid=0, invalid_reason='bad')
        row = self.report(list(reversed(rows)))
        self.assertEqual(row['rounds_valid'], 9)
        self.assertEqual((row['min'], row['median'], row['max']), (130, 170, 210))
        self.assertEqual((row['ratio_min'], row['ratio_median'], row['ratio_max']), (1.3, 1.7, 2.1))
        # Five valid impl values, but only four matching baseline rounds.
        rows = self.make_rows(rounds=5)
        rows[0]['valid'] = 0
        self.assertEqual(self.report(rows)['verdict'], 'insufficient_rounds')
        for r in rows:
            r['valid'] = 0
        self.assertEqual(self.report(rows)['rounds_valid'], 0)

    def test_p99_samples_per_round_per_impl(self):
        rows = self.make_rows(metric='latency_p99')
        self.assertEqual(self.report(rows, metric='latency_p99')['verdict'], 'insufficient_samples')
        samples = [dict(row, metric='latency_samples', value=100) for row in rows]
        self.assertEqual(self.report(rows + samples, metric='latency_p99')['verdict'], 'significant_improvement')
        for bad_impl in ('candidate', 'base', 'base@aa'):
            damaged = [dict(r) for r in samples]
            next(r for r in damaged if r['impl'] == bad_impl)['value'] = 99
            self.assertEqual(self.report(rows + damaged, metric='latency_p99')['verdict'], 'insufficient_samples')
        samples[-1]['valid'] = 0
        self.assertEqual(self.report(rows + samples, metric='latency_p99')['verdict'], 'insufficient_samples')

    def test_pair_ratios_are_not_ratios_of_aggregate_medians(self):
        rows = self.make_rows(values=[200, 900, 800, 700, 600] * 2)
        baseline_values = [100, 1000, 100, 1000, 100] * 2
        for row in rows:
            if row['impl'] in ('base', 'base@aa'):
                row['value'] = baseline_values[row['round'] - 1]
        row = self.report(rows)
        self.assertEqual(row['ratio_median'], 2)
        self.assertEqual((row['ratio_min'], row['ratio_max']), (0.7, 8))
        self.assertEqual((row['aa_min'], row['aa_max']), (1, 1))

    def test_p99_without_aa_still_checks_samples(self):
        rows = [r for r in self.make_rows(metric='latency_p99') if r['impl'] != 'base@aa']
        rows += [dict(row, metric='latency_samples', value=100) for row in rows]
        row = self.report(rows, metric='latency_p99')
        self.assertEqual(row['verdict'], 'no_significant_difference')
        self.assertIn('no_aa_group', row['flags'])

    def test_zero_denominator_cannot_produce_significance(self):
        rows = self.make_rows()
        for r in rows:
            if r['impl'] == 'base':
                r['value'] = 0
        row = self.report(rows)
        self.assertEqual(row['ratio_median'], '')
        self.assertEqual(row['verdict'], 'insufficient_rounds')

    def test_reject_mixed_or_ambiguous_data(self):
        for field, value in [('unit', 'other'), ('better', 'lower'), ('stub', 1),
                             ('value', 'nan'), ('workload_fingerprint', 'other')]:
            with self.subTest(field=field):
                rows = self.make_rows()
                rows[-1][field] = value
                with self.assertRaises(ValueError):
                    self.report(rows)
        rows = self.make_rows()
        with self.assertRaises(ValueError):
            self.report(rows + [rows[-1]])


if __name__ == '__main__':
    unittest.main()
