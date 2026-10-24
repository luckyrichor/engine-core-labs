"""Portable packed little-endian records; no native alignment or headers."""
import math
import struct
from pathlib import Path

TASK = struct.Struct('<I')
AABB = struct.Struct('<6f')
TRACE = struct.Struct('<QQIBQ')


def records(path, fmt):
    with Path(path).open('rb') as stream:
        while data := stream.read(fmt.size):
            if len(data) != fmt.size:
                raise ValueError('truncated record')
            yield fmt.unpack(data)


def read_trace(path):
    return list(records(path, TRACE))


def write_trace(path, trace):
    with Path(path).open('wb') as stream:
        for row in trace:
            stream.write(TRACE.pack(*row))


def read_aabbs(path):
    boxes = list(records(path, AABB))
    for box in boxes:
        if not all(math.isfinite(v) for v in box) or any(box[i] > box[i+3] for i in range(3)):
            raise ValueError('invalid AABB')
    return boxes


def overlaps(a, b):
    return all(a[i] <= b[i+3] and b[i] <= a[i+3] for i in range(3))


def brute_pairs(boxes):
    return sum(overlaps(a, boxes[j]) for i, a in enumerate(boxes) for j in range(i))


def grid_pairs(boxes, cell_size=16):
    """Independent spatial reference. Inclusive faces, unique candidate ownership.

    Very large boxes use an all-pairs fallback against previous boxes rather
    than materializing an unbounded number of grid cells.
    """
    if not math.isfinite(cell_size) or cell_size <= 0:
        raise ValueError('cell_size must be positive')
    grid, large, answer = {}, [], 0
    for i, box in enumerate(boxes):
        lo = [math.floor(v / cell_size) for v in box[:3]]
        hi = [math.floor(v / cell_size) for v in box[3:]]
        cells = math.prod(hi[k] - lo[k] + 1 for k in range(3))
        if cells > 4096:
            answer += sum(overlaps(box, boxes[j]) for j in range(i))
            large.append(i)
            continue
        keys = [(x, y, z) for x in range(lo[0], hi[0]+1)
                for y in range(lo[1], hi[1]+1) for z in range(lo[2], hi[2]+1)]
        candidates = set(large)
        for key in keys:
            candidates.update(grid.get(key, ()))
        answer += sum(overlaps(box, boxes[j]) for j in candidates)
        for key in keys:
            grid.setdefault(key, []).append(i)
    return answer


def trace_answer(trace):
    live, peak, allocs, frees, previous = {}, 0, 0, 0, 0
    total = 0
    for timestamp, size, thread, op, obj in trace:
        if timestamp < previous:
            raise ValueError('timestamps must be monotonic')
        previous = timestamp
        if op == 0 and obj not in live and size > 0:
            live[obj] = (size, thread)
            total += size
            allocs += 1
            peak = max(peak, total)
        elif op == 1 and live.get(obj) == (size, thread):
            del live[obj]
            total -= size
            frees += 1
        else:
            raise ValueError('invalid trace lifetime')
    if live:
        raise ValueError('trace has live allocations at end')
    return dict(allocations=allocs, frees=frees, peak_live_bytes=peak)
