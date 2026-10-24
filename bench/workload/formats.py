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


def grid_parameters(boxes):
    """Upper median, including zero widths; exact binary64 differences of float32."""
    size = max((sorted(b[k+3]-b[k] for b in boxes)[len(boxes)//2]
                for k in range(3)), default=0) if boxes else 0
    return dict(cell_size=4*size if size > 0 else 1,
                cell_size_rule='4*max_axis_upper_median_extent; all_zero=1',
                max_cells_per_box=512, oversized_fallback='static_median_bvh_leaf16')


def grid_pairs(boxes, cell_size=None):
    """Independent spatial oracle, with bounded-cell insertion and tree fallback.

    The static median tree limits storage to O(n), avoids unconditional all-pairs
    scans for outliers, and prunes non-overlapping subtrees. Dense true overlaps
    can still require quadratic work (no subquadratic worst-case claim).
    """
    if cell_size is None:
        cell_size = grid_parameters(boxes)['cell_size']
    if not math.isfinite(cell_size) or cell_size <= 0:
        raise ValueError('cell_size must be positive')
    ranges, large = [], []
    for i, b in enumerate(boxes):
        lo = tuple(math.floor(v/cell_size) for v in b[:3])
        hi = tuple(math.floor(v/cell_size) for v in b[3:])
        big = math.prod(hi[k]-lo[k]+1 for k in range(3)) > 512
        ranges.append((lo, hi, big))
        if big:
            large.append(i)

    def build(ids):
        if not ids:
            return None
        bound = tuple(min(boxes[i][k] for i in ids) for k in range(3)) + tuple(
            max(boxes[i][k] for i in ids) for k in range(3, 6))
        if len(ids) <= 16:
            return bound, min(ids), ids, None, None
        axis = max(range(3), key=lambda k: bound[k+3]-bound[k])
        ids.sort(key=lambda i: (boxes[i][axis]+boxes[i][axis+3], i))
        mid = len(ids)//2
        return bound, min(ids), None, build(ids[:mid]), build(ids[mid:])

    def query(node, box, before):
        if node is None or node[1] >= before or not overlaps(box, node[0]):
            return 0
        if node[2] is not None:
            return sum(j < before and overlaps(box, boxes[j]) for j in node[2])
        return query(node[3], box, before) + query(node[4], box, before)

    large_tree = build(large)
    full_tree = build(list(range(len(boxes)))) if large else None
    grid, answer = {}, 0
    for i, box in enumerate(boxes):
        lo, hi, big = ranges[i]
        if big:
            answer += query(full_tree, box, i)
            continue
        answer += query(large_tree, box, i)
        for x in range(lo[0], hi[0]+1):
            for y in range(lo[1], hi[1]+1):
                for z in range(lo[2], hi[2]+1):
                    key = (x,y,z)
                    bucket = grid.setdefault(key, [])
                    for j in bucket:
                        other = ranges[j][0]
                        if (x,y,z) == tuple(max(lo[k], other[k]) for k in range(3)):
                            answer += overlaps(box, boxes[j])
                    bucket.append(i)
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
