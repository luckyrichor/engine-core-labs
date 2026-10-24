"""Deterministic workload descriptors and generation (format version 1)."""
import hashlib
import json
import re
from pathlib import Path

FIELDS = {'name', 'kind', 'dataset', 'filter', 'generator', 'params', 'seed', 'scale', 'amplification'}
KINDS = {'task_costs', 'aabb_scene', 'resource_manifest', 'alloc_trace', 'constructive'}


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False,
                      allow_nan=False).encode('utf-8')


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        while block := stream.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def integer(value, name, minimum=0, maximum=(1 << 64)-1):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f'{name} must be an integer in {minimum}..{maximum}')
    return value


def validate(d):
    if set(d) != FIELDS or d['kind'] not in KINDS:
        raise ValueError('unexpected descriptor fields or kind')
    if not isinstance(d['name'], str) or not re.fullmatch(r'[A-Za-z0-9_-]+', d['name']):
        raise ValueError('name must be a portable filename component')
    if set(d['dataset']) != {'name', 'version', 'checksum'}:
        raise ValueError('dataset requires name, version, checksum')
    if d['generator'] != 'ecl-v1':
        raise ValueError('unsupported generator version')
    integer(d['seed'], 'seed')
    integer(d['scale'], 'scale', 1)
    a = d['amplification']
    if set(a) != {'factor', 'method'}:
        raise ValueError('amplification requires factor and method')
    integer(a['factor'], 'factor', 1)
    if not isinstance(d['params'], dict) or not isinstance(d['filter'], dict):
        raise ValueError('params and filter must be objects')
    canonical(d)
    return d


def fingerprint(d):
    validate(d)
    return hashlib.sha256(canonical(d)).hexdigest()[:16]


def load(path):
    def unique(pairs):
        out = {}
        for k, v in pairs:
            if k in out:
                raise ValueError(f'duplicate key: {k}')
            out[k] = v
        return out
    return validate(json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique))


def generate(path, out):
    from .generate import generate as implementation
    return implementation(path, out)
