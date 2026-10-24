# Workload format v1 (phase 2B)

This infrastructure generates correctness fixtures, not performance results.
Python 3.12 standard library only. Existing L1/L2/L3 executables are unchanged.

```bash
python3 -m bench.workload generate workloads/tasks-small.json --out "$HOME/data/engine-core-labs/scratch/tasks"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 2
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s bench/tests -v
```

## Descriptor and identity

All descriptors have exactly these fields: `name`, `kind`,
`dataset{name,version,checksum}`, `filter`, `generator`, `params`, `seed`,
`scale`, `amplification{factor,method}`. `generator` is `ecl-v1`; seed is a
uint64 and scale/factor are positive integers. Names use ASCII letters,
ASCII digits, `_`, `-`. Descriptor filenames need not equal names.

The fingerprint is the first 16 hex digits of SHA256 of the descriptor
serialized as sorted-key JSON, separators `,` and `:`, UTF-8, non-ASCII
characters unescaped. Non-finite numbers and duplicate keys are rejected.
Changing whitespace/key order does not change identity. Metadata has no
clock, absolute resolved input paths, hostname or output-directory field.
`<name>.meta.json` records the descriptor, fingerprint, actual input hashes,
output filename → SHA256 map, scale, amplification, answer and answer method.
Consumers must validate all file hashes before using data.

Input filenames are relative to the descriptor directory, or absolute.
For portable external-data descriptors, use `$ECL_DATA/...` for inputs and
resource `root`; the prefix resolves against ECL_DATA, defaulting to
`~/data/engine-core-labs`. The literal prefix remains in the fingerprint.
`dataset.checksum` is `"synthetic"` for synthetic sources, or a mapping from
literal input filename to its complete SHA256 (histogram, AABB, files.csv,
Bistro zip). Every consumed source is checked. `content_checksums` additionally
pins the JSON content of all inventoried `.gltf` and `.glb` files by inventory
relative path; inventory size/path alone would not pin dependency references.
No implicit download or dataset discovery occurs during generation.

splitmix64 expands seed into four words, then xoshiro256** produces uint64s;
all arithmetic wraps modulo 2^64. Bounded draws use rejection sampling of
`x < 2^64 - (2^64 mod n)`, then `x mod n`. A golden-vector test locks this
stream. No library random distributions are used. Synthetic coordinates
are integer ticks divided by 256, rounded to binary32 on output. Reference
answers use those rounded values. Identical descriptors and pinned inputs
are intended to generate identical bytes across platforms; MSVC/Windows
execution has not been verified in phase B.

## Records and generation parameters

All binary files are headerless, little endian, with no padding. Count
comes from metadata and/or file length. CSV is UTF-8, LF, RFC4180 quoting.
The C-phase AABB tool should emit the same headerless AABB record layout.

| Kind | Data record | Scale and amplification | Independent answer |
| --- | --- | --- | --- |
| task_costs | `uint32` work units, 4 bytes | scale = base task count; `repeat` duplicates costs factor times | task_count, cost_sum (uint64) |
| aabb_scene | six IEEE754 binary32: min.xyz, max.xyz, 24 bytes | scale = source box count; `translate_grid` copies factor times | object_count, overlap_pairs |
| resource_manifest | CSV: id,source_path,bytes,extension,dependency_ids | scale = selected source count; `copy_manifest` copies list, optionally truncates last copy to params.target_files | file_count, total_bytes |
| alloc_trace | timestamp u64, size u64, thread u32, op u8, object_id u64, **29 bytes** | scale = allocations per base trace; `repeat` duplicates lifetimes with disjoint IDs/timestamps | allocations, frees, peak_live_bytes |
| constructive | no data file | positive scale describing experiment size; factor=1, method=none | empty answer, descriptor_only |

`method=none` requires factor=1 for all kinds. Generated results belong under
ECL_DATA, not in the repository. `workloads/fixtures/` contains small source
fixtures only, including a four-byte buffer and a textual texture stand-in;
these are format fixtures, not renderable assets.

- Tasks: `distribution=uniform` with inclusive `min,max`; `two_tier` with
  `light,heavy,heavy_numerator,heavy_denominator`; or `histogram` with an input
  path in `histogram`. Histogram JSON is `{ "unit":"work_units", "buckets":
  [[lower_inclusive,upper_exclusive,count],...] }`. Buckets must be ordered,
  disjoint, nonempty, positive-count and within uint32. First select a bucket
  weighted by counts, then select an integer uniformly within it. Real timing
  collection is deferred to phase 3; these units do not claim measured ns.
- AABBs: use `input` (the binary layout above), or
  `distribution=uniform|clustered`, `extent_ticks`, `max_size_ticks`.
  The synthetic clustered distribution uses four diagonal clusters, with
  centers separated by four extents. `grid_spacing` is an integer world-unit
  translation distance (default 65536). Copies are placed x-fastest in a cube
  of side ceil(cuberoot(factor)); spacing and grid side are recorded. Copy
  separation is caller-controlled and need not eliminate inter-copy overlaps.
  Touching faces count as overlapping. Up to 20000 final boxes use brute force;
  larger scenes use the independent grid hash (cell size 16). A box spanning
  over 4096 cells falls back to checking previous boxes, avoiding cell blowup.
  Dense scenes can still require quadratic work; this is a reference answer.
- Resources: `files_csv` is the extra output of `scripts/data/gltf_inventory.py`;
  `root` is the glTF checkout; optional `bistro_zip` contributes zip entry sizes
  without extraction. Either or both input sources may be used. glTF/GLB JSON
  buffer/image URIs become dependency IDs; embedded buffers and data URIs need
  no file dependency. External URLs, escaping root, cyclic or missing/filtered
  dependencies are rejected. Optional filter `extensions`/`models` lists select
  entries; other filter keys are rejected. Inputs are sorted, then topologically
  ordered so dependencies precede models. Copies retain original source paths
  and reference copy-local IDs; **no amplified file is materialized**. Default
  output count is scale*factor; `target_files` may specify a partial final copy,
  with factor exactly ceil(target_files/scale). ZIP source paths use `zip:`.
- Trace: `threads` (default 2), `batch` (8), `max_size` (1024). Each batch allocates
  positive uniformly sampled sizes, assigns threads, then frees in reverse
  order on the allocating thread. Timestamp is a synthetic event ordinal,
  not a wall-clock duration. Operations: 0=alloc, 1=free; free repeats size and
  thread, IDs are unique, trace ends with no live allocation. Python
  `formats.read_trace/write_trace` round-trip the exact packed layout.
- Constructive: params register the future S8 layout; no kernel runs here.
  Non-resource filters must be empty.

## C++ consumption and verification

`experiments/common/workload_io.hpp` is header-only C++20, standard library
only. `verify_meta(path)` parses JSON and verifies every file SHA256. Then
`verified_file(path, meta, filename)` returns checked bytes. Use
`read_task_costs`, `read_aabbs`, `read_trace`, `read_resources` to decode them.
All little-endian decoding is explicit; no unaligned native struct reads.
Record sizes, finite/min≤max boxes, operation range and resource dependencies
are checked. Lifetime and expected-answer comparison are consumer duties
(the C++ correctness test demonstrates both). JSON uint64s are parsed from
integer lexemes, avoiding loss through double. Hashes detect damaged data,
not malicious edits to metadata; metadata is a trusted local artifact.

`bench_workload_cpp` generates all five examples, compares C++ statistics
with Python independent answers, rejects data tampering and rehashed
truncated binary records. Python tests cover repeat generation, fingerprint
mutation, fixed PRNG stream, histogram sampling, amplification, input hashes,
trace lifetimes, glTF dependencies, ZIP sizes and brute/grid agreement at 2000
boxes. Tests store their temporary generated data under ECL_DATA/scratch.
