# Phase 3 paired results

Raw rows, including invalid runs, are retained. Classifications come only from bench.verdict. Work costs are deterministic CPU proxies resampled from measured Bistro mesh bounds with 16x time compression; these are not actual engine mesh tasks.

Audit: 13 groups; 41940 rows; 0 invalid metric rows; minimum valid throughput rounds=10; stub rows=0.

## S1: real 100k tasks, four workers

| group | workload | T | impl | ratio median [min,max] | verdict | flags |
|---|---|---|---|---|---|---|
| S1-end_to_end | real-100000 | 4 | async | 2.556 [1.612,3.429] | significant_improvement |  |
| S1-end_to_end | real-100000 | 4 | chase | 0.741 [0.625,0.806] | significant_regression |  |
| S1-end_to_end | real-100000 | 4 | half | 0.701 [0.594,0.824] | no_significant_difference |  |
| S1-end_to_end | real-100000 | 4 | one | 0.680 [0.572,0.814] | no_significant_difference |  |
| S1-end_to_end | real-100000 | 4 | static | 1.000 [1.000,1.000] | no_significant_difference |  |
| S1-end_to_end | real-100000 | 4 | taskflow | 0.745 [0.637,0.868] | no_significant_difference |  |
| S1-end_to_end | real-100000 | 4 | tbb | 0.658 [0.504,0.717] | significant_regression |  |
| S1-prequeued | real-100000 | 4 | async | 2.074 [1.514,2.681] | no_significant_difference |  |
| S1-prequeued | real-100000 | 4 | chase | 1.162 [0.987,1.789] | no_significant_difference |  |
| S1-prequeued | real-100000 | 4 | half | 1.103 [0.799,1.668] | no_significant_difference |  |
| S1-prequeued | real-100000 | 4 | one | 1.191 [0.986,1.655] | no_significant_difference |  |
| S1-prequeued | real-100000 | 4 | static | 1.000 [1.000,1.000] | no_significant_difference |  |
| S1-prequeued | real-100000 | 4 | taskflow | 0.796 [0.613,1.492] | no_significant_difference |  |
| S1-prequeued | real-100000 | 4 | tbb | 0.601 [0.443,0.895] | no_significant_difference |  |

## S1: uniform control, four workers

| group | workload | T | impl | ratio median [min,max] | verdict | flags |
|---|---|---|---|---|---|---|
| S1-end_to_end | uniform-100000 | 4 | async | 2.629 [1.725,4.459] | significant_improvement |  |
| S1-end_to_end | uniform-100000 | 4 | chase | 0.735 [0.644,1.091] | no_significant_difference |  |
| S1-end_to_end | uniform-100000 | 4 | half | 0.733 [0.627,1.026] | no_significant_difference |  |
| S1-end_to_end | uniform-100000 | 4 | one | 0.780 [0.616,0.983] | no_significant_difference |  |
| S1-end_to_end | uniform-100000 | 4 | static | 1.000 [1.000,1.000] | no_significant_difference |  |
| S1-end_to_end | uniform-100000 | 4 | taskflow | 0.788 [0.665,1.095] | no_significant_difference |  |
| S1-end_to_end | uniform-100000 | 4 | tbb | 0.696 [0.622,0.946] | no_significant_difference |  |
| S1-prequeued | uniform-100000 | 4 | async | 1.637 [0.889,2.726] | no_significant_difference |  |
| S1-prequeued | uniform-100000 | 4 | chase | 1.052 [0.536,1.547] | no_significant_difference |  |
| S1-prequeued | uniform-100000 | 4 | half | 1.037 [0.882,1.684] | no_significant_difference |  |
| S1-prequeued | uniform-100000 | 4 | one | 0.964 [0.664,1.651] | no_significant_difference |  |
| S1-prequeued | uniform-100000 | 4 | static | 1.000 [1.000,1.000] | no_significant_difference |  |
| S1-prequeued | uniform-100000 | 4 | taskflow | 0.945 [0.562,1.524] | no_significant_difference |  |
| S1-prequeued | uniform-100000 | 4 | tbb | 0.534 [0.426,0.866] | no_significant_difference |  |

## S2: conditional notification

| group | workload | T | impl | ratio median [min,max] | verdict | flags |
|---|---|---|---|---|---|---|
| S2-p1-end_to_end | real-100000 | 4 | sleepers | 1.038 [0.775,1.167] | no_significant_difference |  |
| S2-p1-prequeued | real-100000 | 4 | sleepers | 1.117 [0.779,1.528] | no_significant_difference |  |
| S2-p2-end_to_end | real-100000 | 4 | sleepers | 1.054 [0.741,1.436] | no_significant_difference |  |
| S2-p2-prequeued | real-100000 | 4 | sleepers | 0.908 [0.551,1.107] | no_significant_difference |  |
| S2-p4-end_to_end | real-100000 | 4 | sleepers | 1.038 [0.786,1.513] | no_significant_difference |  |
| S2-p4-prequeued | real-100000 | 4 | sleepers | 1.107 [0.867,1.302] | no_significant_difference |  |

## S3: crossover evidence

- block=1, static: first sampled significant improvement near 482 ns/element (a sampled point, not an exact hardware threshold).
- block=1, chase: first sampled significant improvement near 1930 ns/element (a sampled point, not an exact hardware threshold).
- block=1, tbb: first sampled significant improvement near 1930 ns/element (a sampled point, not an exact hardware threshold).
- block=1, taskflow: first sampled significant improvement near 482 ns/element (a sampled point, not an exact hardware threshold).
- block=16, static: first sampled significant improvement near 121 ns/element (a sampled point, not an exact hardware threshold).
- block=16, chase: first sampled significant improvement near 121 ns/element (a sampled point, not an exact hardware threshold).
- block=16, tbb: first sampled significant improvement near 121 ns/element (a sampled point, not an exact hardware threshold).
- block=16, taskflow: first sampled significant improvement near 121 ns/element (a sampled point, not an exact hardware threshold).
- block=256, static: first sampled significant improvement near 121 ns/element (a sampled point, not an exact hardware threshold).
- block=256, chase: first sampled significant improvement near 121 ns/element (a sampled point, not an exact hardware threshold).
- block=256, tbb: first sampled significant improvement near 121 ns/element (a sampled point, not an exact hardware threshold).
- block=256, taskflow: first sampled significant improvement near 121 ns/element (a sampled point, not an exact hardware threshold).

All thread counts, modes and raw ratio ranges are in throughput-comparisons.csv and s3-curve.csv. Do not infer significance from a line chart alone.
