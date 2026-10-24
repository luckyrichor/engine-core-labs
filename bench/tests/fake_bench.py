"""Synthetic CSV producer; never use its numbers as performance measurements."""
import argparse
import csv
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--impl', choices=('base', 'candidate'), default='base')
    parser.add_argument('--value', type=float, default=100)
    parser.add_argument('--valid', choices=('0', '1'), default='1')
    parser.add_argument('--samples', type=int, default=100)
    parser.add_argument('--exit-code', type=int, default=0)
    parser.add_argument('--metric', default='throughput')
    parser.add_argument('--better', choices=('higher', 'lower'), default='higher')
    args = parser.parse_args()
    writer = csv.writer(sys.stdout)
    writer.writerow('metric value unit better valid invalid_reason stub'.split())
    writer.writerow([args.metric, args.value, 'synthetic', args.better, args.valid,
                     '' if args.valid == '1' else 'fake_validation_failure', 1])
    if args.metric.endswith('_p99'):
        writer.writerow([args.metric[:-4] + '_samples', args.samples, 'count', 'higher', 1, '', 1])
    sys.exit(args.exit_code)


if __name__ == '__main__':
    main()
