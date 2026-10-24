import argparse
from . import generate


def main():
    parser = argparse.ArgumentParser(description='Deterministic workload generator')
    sub = parser.add_subparsers(dest='command', required=True)
    gen = sub.add_parser('generate')
    gen.add_argument('descriptor')
    gen.add_argument('--out', required=True)
    args = parser.parse_args()
    try:
        meta = generate(args.descriptor, args.out)
    except (ValueError, OSError, KeyError) as exc:
        parser.exit(1, f'workload: {exc}\n')
    print(meta['workload_fingerprint'])


if __name__ == '__main__':
    main()
