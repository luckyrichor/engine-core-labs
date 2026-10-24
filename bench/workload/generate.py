"""Generation never reads clocks, host names, or machine-dependent PRNGs."""
import bisect
import csv
import io
import json
import os
import struct
import zipfile
from pathlib import Path, PurePosixPath
from urllib.parse import unquote, urlsplit

from . import fingerprint, integer, load, sha256
from .formats import AABB, TASK, brute_pairs, grid_pairs, read_aabbs, trace_answer
from .prng import Random


def generate(path, out):
    path, out = Path(path), Path(out)
    d = load(path)
    p, n, factor = d['params'], d['scale'], d['amplification']['factor']
    rng = Random(d['seed'])
    inputs, files = {}, {}

    def resolve(relative):
        if relative == '$ECL_DATA':
            return Path(os.environ.get('ECL_DATA', str(Path.home()/'data/engine-core-labs')))
        if relative.startswith('$ECL_DATA/'):
            return Path(os.environ.get('ECL_DATA', str(Path.home()/'data/engine-core-labs'))) / relative[len('$ECL_DATA/'):]
        return path.parent / relative

    def source(key):
        relative = p[key]
        file = resolve(relative)
        expected = d['dataset']['checksum']
        if not isinstance(expected, dict) or relative not in expected:
            raise ValueError(f'checksum missing for input {relative}')
        actual = sha256(file)
        if actual != expected[relative]:
            raise ValueError(f'checksum mismatch: {relative}')
        inputs[relative] = actual
        return file

    def method(allowed):
        if d['amplification']['method'] not in allowed:
            raise ValueError('unsupported amplification method')
        if d['amplification']['method'] == 'none' and factor != 1:
            raise ValueError('none amplification requires factor=1')

    if d['kind'] != 'resource_manifest' and d['filter']:
        raise ValueError('this kind does not support filters')
    # Generation is complete in memory before publishing any outputs.
    data, answer, details = None, {}, {}
    kind = d['kind']
    if kind == 'task_costs':
        method({'none', 'repeat'})
        distribution = p['distribution']
        costs = []
        if distribution == 'histogram':
            hist = json.loads(source('histogram').read_text(encoding='utf-8'))
            if hist.get('unit') != 'work_units' or not hist.get('buckets'):
                raise ValueError('histogram requires work_units and buckets')
            cumulative, total, previous = [], 0, 0
            for lo, hi, count in hist['buckets']:
                integer(lo, 'bucket lower', 0, (1 << 32)-1)
                integer(hi, 'bucket upper', lo+1, 1 << 32)
                integer(count, 'bucket count', 1)
                if lo < previous:
                    raise ValueError('histogram buckets must be ordered and disjoint')
                previous = hi
                total += count
                cumulative.append(total)
            for _ in range(n):
                lo, hi, _ = hist['buckets'][bisect.bisect_right(cumulative, rng.below(total))]
                costs.append(lo + rng.below(hi-lo))
        elif distribution == 'uniform':
            lo = integer(p['min'], 'min', 0, (1 << 32)-1)
            hi = integer(p['max'], 'max', lo, (1 << 32)-1)
            costs = [lo + rng.below(hi-lo+1) for _ in range(n)]
        elif distribution == 'two_tier':
            light = integer(p['light'], 'light', 0, (1 << 32)-1)
            heavy = integer(p['heavy'], 'heavy', light, (1 << 32)-1)
            denominator = integer(p['heavy_denominator'], 'heavy_denominator', 1)
            numerator = integer(p['heavy_numerator'], 'heavy_numerator', 0, denominator)
            costs = [heavy if rng.below(denominator) < numerator else light for _ in range(n)]
        else:
            raise ValueError('unsupported task distribution')
        costs *= factor
        answer = dict(task_count=len(costs), cost_sum=integer(sum(costs), 'cost_sum'))
        data = b''.join(TASK.pack(c) for c in costs)
        details = dict(unit='work_units', answer_method='serial_integer_sum')
    elif kind == 'aabb_scene':
        method({'none', 'translate_grid'})
        if 'input' in p:
            boxes = read_aabbs(source('input'))
            if len(boxes) != n:
                raise ValueError('input box count must equal scale')
        else:
            distribution = p['distribution']
            if distribution not in {'uniform', 'clustered'}:
                raise ValueError('unsupported AABB distribution')
            extent = integer(p.get('extent_ticks', 16384), 'extent_ticks', 1, 1 << 22)
            width = integer(p.get('max_size_ticks', 1024), 'max_size_ticks', 1, 1 << 20)
            boxes = []
            # Integer ticks / 256 are exact in binary64; output is explicitly rounded to binary32.
            for _ in range(n):
                cluster = rng.below(4) if distribution == 'clustered' else 0
                xyz = [rng.below(extent) + cluster * extent * 4 for _ in range(3)]
                end = [v + 1 + rng.below(width) for v in xyz]
                boxes.append(tuple(v / 256 for v in xyz + end))
        spacing = integer(p.get('grid_spacing', 65536), 'grid_spacing', 1, 1 << 24)
        side = 1
        while side ** 3 < factor:
            side += 1
        expanded = []
        for copy in range(factor):
            offset = [(copy % side)*spacing, ((copy//side) % side)*spacing,
                      (copy//(side*side))*spacing]
            for box in boxes:
                # Round to binary32 BEFORE computing reference answer.
                expanded.append(AABB.unpack(AABB.pack(*(v + offset[k % 3] for k, v in enumerate(box)))))
        boxes = expanded
        reference = brute_pairs if len(boxes) <= 20000 else grid_pairs
        answer = dict(object_count=len(boxes), overlap_pairs=reference(boxes))
        details = dict(answer_method='brute_force' if len(boxes) <= 20000 else 'grid_hash',
                       grid_spacing=spacing, copy_grid_side=side)
        data = b''.join(AABB.pack(*box) for box in boxes)
    elif kind == 'alloc_trace':
        method({'none', 'repeat'})
        threads = integer(p.get('threads', 2), 'threads', 1, (1 << 32)-1)
        max_size = integer(p.get('max_size', 1024), 'max_size', 1, (1 << 32)-1)
        batch = integer(p.get('batch', 8), 'batch', 1)
        trace, timestamp, obj = [], 0, 0
        remaining = n
        while remaining:
            live = []
            for _ in range(min(batch, remaining)):
                row = (timestamp, 1 + rng.below(max_size), rng.below(threads), 0, obj)
                trace.append(row)
                live.append(row)
                timestamp += 1
                obj += 1
            for _, size, thread, _, oid in reversed(live):
                trace.append((timestamp, size, thread, 1, oid))
                timestamp += 1
            remaining -= len(live)
        # Amplification repeats the same trace with disjoint IDs and timestamps.
        base_trace = trace[:2*n]
        trace = [(t + copy*2*n, size, thread, op, oid + copy*n)
                 for copy in range(factor) for t, size, thread, op, oid in base_trace]
        from .formats import TRACE
        data = b''.join(TRACE.pack(*row) for row in trace)
        answer = trace_answer(trace)
        details = dict(answer_method='serial_lifetime_map', record_count=len(trace))
    elif kind == 'resource_manifest':
        method({'none', 'copy_manifest'})
        rows, dependencies = resource_rows(p, d['filter'], source, resolve, inputs)
        if not rows:
            raise ValueError('empty resource selection')
        if n != len(rows):
            raise ValueError('scale must equal selected source file count')
        target = integer(p.get('target_files', n*factor), 'target_files', n)
        if (target+n-1)//n != factor:
            raise ValueError('factor must equal ceil(target_files / scale)')
        # Cyclic copies keep the same source path; dependencies reference copy-local ids.
        output = []
        ids = {r['source_path']: i for i, r in enumerate(rows)}
        for i in range(target):
            row = rows[i % n]
            base = (i//n)*n
            deps = [base + ids[v] for v in dependencies.get(row['source_path'], [])]
            if any(v >= target for v in deps):
                raise ValueError('partial final copy would omit a dependency; use a complete copy')
            output.append([i, row['source_path'], row['bytes'], row['extension'],
                           ';'.join(str(v) for v in sorted(deps))])
        stream = io.StringIO(newline='')
        writer = csv.writer(stream, lineterminator='\n')
        writer.writerow(['id', 'source_path', 'bytes', 'extension', 'dependency_ids'])
        writer.writerows(output)
        data = stream.getvalue().encode('utf-8')
        answer = dict(file_count=len(output), total_bytes=sum(row[2] for row in output))
        details = dict(answer_method='serial_manifest_sum', target_files=target)
    else:
        method({'none'})
        details = dict(answer_method='descriptor_only')
    out.mkdir(parents=True, exist_ok=True)
    if data is not None:
        filename = d['name'] + ('.csv' if kind == 'resource_manifest' else '.bin')
        (out / filename).write_bytes(data)
        files[filename] = sha256(out / filename)
    meta = dict(format_version=1, name=d['name'], kind=kind, descriptor=d,
                workload_fingerprint=fingerprint(d), files=files, inputs=inputs,
                scale=n, amplification=d['amplification'], answer=answer, **details)
    (out / (d['name']+'.meta.json')).write_text(
        json.dumps(meta, sort_keys=True, indent=2, ensure_ascii=False, allow_nan=False)+'\n',
        encoding='utf-8', newline='\n')
    return meta


def resource_rows(p, filters, source, resolve, inputs):
    if set(filters) - {'extensions', 'models'}:
        raise ValueError('unsupported resource filter')
    rows, deps = [], {}
    if 'files_csv' in p:
        inventory = source('files_csv')
        root = resolve(p['root'])
        with inventory.open(encoding='utf-8', newline='') as stream:
            for row in csv.DictReader(stream):
                rows.append(dict(source_path=row['path'], bytes=int(row['bytes']),
                                 extension=row['extension'], model=row['model']))
        # Inventory checksum pins path/size; content_checksums pins actual glTF JSON.
        for row in rows:
            if row['extension'] not in {'.gltf', '.glb'}:
                continue
            relative = row['source_path']
            content = root / relative
            actual = sha256(content)
            if p.get('content_checksums', {}).get(relative) != actual:
                raise ValueError(f'glTF content checksum missing/mismatch: {relative}')
            inputs[relative] = actual
            if row['extension'] == '.glb':
                raw = content.read_bytes()
                if len(raw) < 20 or raw[:4] != b'glTF' or struct.unpack_from('<II', raw, 4) != (2, len(raw)):
                    raise ValueError('invalid GLB header')
                length, chunk_type = struct.unpack_from('<II', raw, 12)
                if chunk_type != 0x4e4f534a or 20+length > len(raw):
                    raise ValueError('invalid GLB JSON chunk')
                model = json.loads(raw[20:20+length].decode('utf-8'))
            else:
                model = json.loads(content.read_text(encoding='utf-8'))
            paths = set()
            for item in model.get('buffers', []) + model.get('images', []):
                uri = item.get('uri')
                if not uri or uri.startswith('data:'):
                    continue
                split = urlsplit(uri)
                if split.scheme or split.netloc or split.query or split.fragment:
                    raise ValueError('only relative glTF dependency URIs are supported')
                target = (content.parent / unquote(split.path)).resolve()
                if not target.is_relative_to(root.resolve()):
                    raise ValueError('glTF dependency escapes root')
                paths.add(target.relative_to(root.resolve()).as_posix())
            deps[relative] = sorted(paths)
    if 'bistro_zip' in p:
        archive = source('bistro_zip')
        with zipfile.ZipFile(archive) as z:
            for info in sorted(z.infolist(), key=lambda r: r.filename):
                if not info.is_dir():
                    rows.append(dict(source_path='zip:'+info.filename, bytes=info.file_size,
                                     extension=PurePosixPath(info.filename).suffix.lower() or '<none>',
                                     model='Bistro'))
    selected = [r for r in rows if (not filters.get('extensions') or r['extension'] in filters['extensions'])
                and (not filters.get('models') or r['model'] in filters['models'])]
    selected.sort(key=lambda r: r['source_path'])
    names = {r['source_path'] for r in selected}
    if len(names) != len(selected) or any(r['bytes'] < 0 for r in selected):
        raise ValueError('duplicate path or negative size in inventory')
    if any(v not in names for r in selected for v in deps.get(r['source_path'], [])):
        raise ValueError('selection omits glTF dependency')
    # Topological order means every partial final copy retains its dependencies.
    ordered, visited, active = [], set(), set()
    by_name = {r['source_path']: r for r in selected}
    def visit(name):
        if name in active:
            raise ValueError('cyclic resource dependencies')
        if name in visited:
            return
        active.add(name)
        for dependency in sorted(deps.get(name, [])):
            visit(dependency)
        active.remove(name)
        visited.add(name)
        ordered.append(by_name[name])
    for name in sorted(names):
        visit(name)
    return ordered, deps
