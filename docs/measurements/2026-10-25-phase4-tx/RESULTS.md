# S8 result comparisons

All significance classifications come from bench.verdict. Pinning and each layout change are paired in the same round; combined changes are not presented as a one-factor effect. Windows unbound still retains the P-core eligible mask.

Audit: 1360 measured processes, 9520 metric rows, invalid=0, minimum valid throughput rounds=10.

| case | comparison | threads | impl | paired median [min,max] | A/A range | verdict | flags |
|---|---|---|---|---|---|---|---|
| cold | affinity-packed | 1 | packed-pinned | 0.998 [0.933,1.047] | [0.881,1.054] | no_significant_difference |  |
| cold | padding-unbound | 1 | separated | 1.004 [0.975,1.069] | [0.881,1.054] | no_significant_difference |  |
| cold | affinity-packed | 4 | packed-pinned | 1.299 [0.699,1.476] | [0.583,1.368] | no_significant_difference |  |
| cold | padding-unbound | 4 | separated | 1.028 [0.622,1.442] | [0.583,1.368] | no_significant_difference |  |
| cold | padding-pinned | 1 | separated-pinned | 1.004 [0.948,1.087] | [0.876,1.068] | no_significant_difference |  |
| cold | padding-pinned | 4 | separated-pinned | 1.015 [0.741,1.261] | [0.681,1.733] | no_significant_difference |  |
| counters | padding-unbound | 1 | next | 1.016 [0.956,1.103] | [0.781,1.071] | no_significant_difference |  |
| counters | affinity-packed | 1 | packed-pinned | 1.042 [0.828,1.125] | [0.781,1.071] | no_significant_difference |  |
| counters | padding-unbound | 1 | pending | 0.976 [0.850,1.075] | [0.781,1.071] | no_significant_difference |  |
| counters | padding-unbound | 1 | wake | 1.014 [0.942,1.098] | [0.781,1.071] | no_significant_difference |  |
| counters | padding-unbound | 4 | next | 1.002 [0.696,1.222] | [0.828,1.586] | no_significant_difference |  |
| counters | affinity-packed | 4 | packed-pinned | 0.914 [0.755,1.128] | [0.828,1.586] | no_significant_difference |  |
| counters | padding-unbound | 4 | pending | 0.987 [0.674,1.441] | [0.828,1.586] | no_significant_difference |  |
| counters | padding-unbound | 4 | wake | 1.059 [0.847,1.581] | [0.828,1.586] | no_significant_difference |  |
| counters | padding-pinned | 1 | next-pinned | 0.934 [0.854,1.213] | [0.879,1.229] | no_significant_difference |  |
| counters | padding-pinned | 1 | pending-pinned | 0.991 [0.922,1.209] | [0.879,1.229] | no_significant_difference |  |
| counters | padding-pinned | 1 | wake-pinned | 0.990 [0.816,1.246] | [0.879,1.229] | no_significant_difference |  |
| counters | padding-pinned | 4 | next-pinned | 0.952 [0.905,1.078] | [0.839,1.234] | no_significant_difference |  |
| counters | padding-pinned | 4 | pending-pinned | 0.981 [0.778,1.071] | [0.839,1.234] | no_significant_difference |  |
| counters | padding-pinned | 4 | wake-pinned | 0.998 [0.921,1.208] | [0.839,1.234] | no_significant_difference |  |
| positive | affinity-packed | 1 | packed-pinned | 0.996 [0.955,1.042] | [0.974,1.049] | no_significant_difference |  |
| positive | padding-unbound | 1 | separated | 0.997 [0.781,1.027] | [0.974,1.049] | no_significant_difference |  |
| positive | affinity-packed | 4 | packed-pinned | 1.010 [0.934,1.332] | [0.945,1.051] | no_significant_difference |  |
| positive | padding-unbound | 4 | separated | 6.327 [3.720,7.927] | [0.945,1.051] | significant_improvement |  |
| positive | padding-pinned | 1 | separated-pinned | 1.001 [0.967,1.046] | [0.339,1.033] | no_significant_difference |  |
| positive | padding-pinned | 4 | separated-pinned | 7.455 [5.867,7.931] | [0.764,1.090] | significant_improvement |  |
| queue | affinity-packed | 1 | packed-pinned | 1.012 [0.983,1.066] | [0.754,1.081] | no_significant_difference |  |
| queue | padding-unbound | 1 | separated | 0.997 [0.918,1.062] | [0.754,1.081] | no_significant_difference |  |
| queue | affinity-packed | 4 | packed-pinned | 0.886 [0.503,1.203] | [0.646,1.268] | no_significant_difference |  |
| queue | padding-unbound | 4 | separated | 0.831 [0.722,0.963] | [0.646,1.268] | no_significant_difference |  |
| queue | padding-pinned | 1 | separated-pinned | 0.990 [0.954,1.044] | [0.614,1.008] | no_significant_difference |  |
| queue | padding-pinned | 4 | separated-pinned | 0.862 [0.460,1.191] | [0.604,1.104] | no_significant_difference |  |
| stats | affinity-packed | 1 | packed-pinned | 1.006 [0.736,1.289] | [0.821,1.260] | no_significant_difference |  |
| stats | padding-unbound | 1 | separated | 1.023 [0.750,1.199] | [0.821,1.260] | no_significant_difference |  |
| stats | affinity-packed | 4 | packed-pinned | 0.730 [0.646,1.110] | [0.708,1.204] | no_significant_difference |  |
| stats | padding-unbound | 4 | separated | 1.129 [0.942,1.381] | [0.708,1.204] | no_significant_difference |  |
| stats | padding-pinned | 1 | separated-pinned | 1.036 [0.639,1.554] | [0.712,1.618] | no_significant_difference |  |
| stats | padding-pinned | 4 | separated-pinned | 1.055 [0.600,1.986] | [0.584,2.316] | no_significant_difference |  |

All thread counts are retained in comparisons.csv. CPU migrations are a Linux software event over the controlled interval, include producer/main, and are not cache/coherence hardware events. Layout bytes are selected objects/active cells, not RSS.
