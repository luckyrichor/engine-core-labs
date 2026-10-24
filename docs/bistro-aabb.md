# Bistro AABB preprocessing (phase 2C)

Offline correctness infrastructure by Codex, based on `a452624` plus phase C
working-tree changes. No experiment performance conclusions. Existing L1/L2/L3
programs are unchanged. Source data and generated binaries stay in ECL_DATA.

```bash
export ECL_DATA="${ECL_DATA:-$HOME/data/engine-core-labs}"
cmake -S . -B "$ECL_DATA/build-phase-c" -DCMAKE_BUILD_TYPE=Release -DECL_DATA="$ECL_DATA"
cmake --build "$ECL_DATA/build-phase-c" -j 2
"$ECL_DATA/tools/bin/bistro_aabb" "$ECL_DATA/bistro/extracted/Bistro_v5_2" "$ECL_DATA/bistro/aabb"
python3 -m bench.workload generate workloads/bistro/bistro-interior-instances.json --out "$ECL_DATA/scratch/bistro-interior-instances"
/usr/bin/time -v -o "$ECL_DATA/logs/bistro-answer-time.txt" \
  python3 -m bench.workload generate workloads/bistro/bistro-interior-triangles.json --out "$ECL_DATA/scratch/bistro-interior-triangles"
python3 tools/bistro-aabb/validate.py --output docs/datasets/bistro/aabb-stats.json \
  --answer-meta "$ECL_DATA/scratch/bistro-interior-triangles/bistro-interior-triangles.meta.json" \
  --answer-time "$ECL_DATA/logs/bistro-answer-time.txt"
ctest --test-dir "$ECL_DATA/build-phase-c" --output-on-failure
```

Create `$ECL_DATA/logs` first if absent. Long unattended invocations should use
`systemd-run --user` as required in the execution instructions. GNU time is only
needed for Linux budget validation; the tools and generator use standard C++20
and Python 3.12. Windows/MSVC execution has **not** been verified.

`bistro_aabb` is optional: it requires an explicit CMake ECL_DATA setting or
ECL_DATA environment variable and `third_party/ufbx-v0.23.1/{ufbx.c,ufbx.h}`.
The default build succeeds without ufbx; it still builds the independent
`aabb_reference` oracle. When ECL_DATA is set executables go under its
`tools/bin/`; otherwise the oracle stays in the normal build directory.
Set `ECL_AABB_REFERENCE` to override its location, including multi-config
Windows build output paths. ufbx header/source version must both be 0.23.1.

For each FBX, `*.instances.bin` contains one tight AABB per mesh instance and
`*.triangles.bin` one per triangulated face triangle per instance, in ufbx
mesh/instance/face order. Every vertex is transformed using the instance's
`geometry_to_world`, rounded to binary32, then included in its bounds.
Records are exactly six little-endian float32 values (min.xyz, max.xyz), no
header or padding. Touching faces, edges and points count as overlaps.
`*.meta.json` records source SHA256, ufbx version, counts, overall bounds,
triangle union and output SHA256. No transforms are applied to just two local
bounding-box corners. Triangulation uses ufbx's face triangulator.

The tool rejects unexpected triangle counts and unequal overall/triangle
unions. `validate.py` independently reads every record, validates finite ordered
bounds, file lengths, source/output hashes and both unions. JSON bound
comparisons allow 1e-6 relative error; the unions reconstructed from the two
binary files must match exactly. The repository stores only aggregate stats
and metadata at `docs/datasets/bistro/aabb-stats.json`, never FBX/AABB assets.

The Python oracle and the C++ oracle choose cell size as
`4 * max(upper_median(axis_extent))`, including zeros, with fallback 1 if all
three medians are zero. Upper median is sorted index `n//2`. Boxes covering
more than 512 cells use static binary trees split at median center on the
longest bound axis; leaves contain at most 16 boxes. Large boxes query the
full tree against previous IDs; regular boxes query the large-box tree plus
grid buckets. Regular pairs belong to the first shared cell, avoiding repeated
counting without a large candidate set. Trees prune by bounds and minimum ID.
Storage is O(n) with a fixed cell cap; the fallback avoids unconditional O(n)
scans per large box, but densely intersecting scenes can still incur quadratic
candidate work. This is an independent **answer oracle**, never a measured
broad-phase implementation; future experiment implementations must not share
its code. It includes no experiment headers or libraries.

The new ctest `bench_aabb_reference` compares C++/Python/brute force on 2000
random boxes plus deliberately oversized, touching and degenerate boxes,
several cell sizes, empty input and malformed input. It also exercises the
native backend through the workload generator and checks deterministic output.
Default tests do not require real Bistro data. `validate.py` separately compares
all three real instance scenes using brute force, Python grid and native grid.
The million-triangle Interior scene uses the native oracle. The recorded GNU
time duration covers generation, input checking and the oracle, while RSS is
the maximum reported for the parent or waited-for children, not their summed
simultaneous resident memory. This validates only the preprocessing budget.
