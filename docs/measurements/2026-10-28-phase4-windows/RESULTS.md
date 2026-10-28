# S8 result comparisons

All significance classifications come from bench.verdict. Pinning and each layout change are paired in the same round; combined changes are not presented as a one-factor effect. Windows unbound still retains the P-core eligible mask.

Audit: 1360 measured processes, 8160 metric rows, invalid=0, minimum valid throughput rounds=10.

| case | comparison | threads | impl | paired median [min,max] | A/A range | verdict | flags |
|---|---|---|---|---|---|---|---|
| cold | affinity-packed | 1 | packed-pinned | 0.896 [0.881,0.919] | [0.922,1.015] | significant_regression |  |
| cold | padding-unbound | 1 | separated | 0.998 [0.953,1.023] | [0.922,1.015] | no_significant_difference |  |
| cold | affinity-packed | 4 | packed-pinned | 1.452 [0.950,3.487] | [0.419,1.709] | no_significant_difference |  |
| cold | padding-unbound | 4 | separated | 0.731 [0.411,1.269] | [0.419,1.709] | no_significant_difference |  |
| cold | padding-pinned | 1 | separated-pinned | 1.000 [0.943,1.035] | [0.937,1.033] | no_significant_difference |  |
| cold | padding-pinned | 4 | separated-pinned | 1.000 [0.925,1.057] | [0.981,1.083] | no_significant_difference |  |
| counters | padding-unbound | 1 | next | 0.951 [0.863,1.146] | [0.932,1.181] | no_significant_difference |  |
| counters | affinity-packed | 1 | packed-pinned | 0.949 [0.839,1.105] | [0.932,1.181] | no_significant_difference |  |
| counters | padding-unbound | 1 | pending | 0.942 [0.842,1.158] | [0.932,1.181] | no_significant_difference |  |
| counters | padding-unbound | 1 | wake | 1.008 [0.882,1.128] | [0.932,1.181] | no_significant_difference |  |
| counters | padding-unbound | 4 | next | 1.092 [0.881,1.299] | [0.801,1.103] | no_significant_difference |  |
| counters | affinity-packed | 4 | packed-pinned | 1.036 [0.804,1.163] | [0.801,1.103] | no_significant_difference |  |
| counters | padding-unbound | 4 | pending | 0.999 [0.817,1.197] | [0.801,1.103] | no_significant_difference |  |
| counters | padding-unbound | 4 | wake | 1.139 [0.859,1.296] | [0.801,1.103] | no_significant_difference |  |
| counters | padding-pinned | 1 | next-pinned | 0.963 [0.897,1.073] | [0.915,1.137] | no_significant_difference |  |
| counters | padding-pinned | 1 | pending-pinned | 0.962 [0.934,1.048] | [0.915,1.137] | no_significant_difference |  |
| counters | padding-pinned | 1 | wake-pinned | 0.999 [0.927,1.133] | [0.915,1.137] | no_significant_difference |  |
| counters | padding-pinned | 4 | next-pinned | 1.004 [0.891,1.091] | [0.882,1.088] | no_significant_difference |  |
| counters | padding-pinned | 4 | pending-pinned | 0.981 [0.893,1.047] | [0.882,1.088] | no_significant_difference |  |
| counters | padding-pinned | 4 | wake-pinned | 0.999 [0.872,1.143] | [0.882,1.088] | no_significant_difference |  |
| positive | affinity-packed | 1 | packed-pinned | 0.982 [0.731,1.012] | [0.929,1.036] | no_significant_difference |  |
| positive | padding-unbound | 1 | separated | 1.000 [0.991,1.031] | [0.929,1.036] | no_significant_difference |  |
| positive | affinity-packed | 4 | packed-pinned | 1.031 [0.834,1.234] | [0.791,1.234] | no_significant_difference |  |
| positive | padding-unbound | 4 | separated | 5.620 [3.956,9.545] | [0.791,1.234] | significant_improvement |  |
| positive | padding-pinned | 1 | separated-pinned | 1.034 [0.995,1.365] | [0.917,1.363] | no_significant_difference |  |
| positive | padding-pinned | 4 | separated-pinned | 7.894 [5.386,10.233] | [0.711,1.414] | significant_improvement |  |
| queue | affinity-packed | 1 | packed-pinned | 0.903 [0.857,0.970] | [0.969,1.115] | no_significant_difference |  |
| queue | padding-unbound | 1 | separated | 0.996 [0.894,1.115] | [0.969,1.115] | no_significant_difference |  |
| queue | affinity-packed | 4 | packed-pinned | 0.919 [0.525,1.216] | [0.587,1.450] | no_significant_difference |  |
| queue | padding-unbound | 4 | separated | 1.035 [0.613,1.081] | [0.587,1.450] | no_significant_difference |  |
| queue | padding-pinned | 1 | separated-pinned | 0.986 [0.897,1.038] | [0.956,1.044] | no_significant_difference |  |
| queue | padding-pinned | 4 | separated-pinned | 1.023 [0.788,1.133] | [0.895,1.099] | no_significant_difference |  |
| stats | affinity-packed | 1 | packed-pinned | 0.971 [0.908,1.014] | [0.970,1.049] | no_significant_difference |  |
| stats | padding-unbound | 1 | separated | 0.998 [0.962,1.024] | [0.970,1.049] | no_significant_difference |  |
| stats | affinity-packed | 4 | packed-pinned | 0.949 [0.815,1.065] | [0.799,1.071] | no_significant_difference |  |
| stats | padding-unbound | 4 | separated | 1.065 [0.884,1.153] | [0.799,1.071] | no_significant_difference |  |
| stats | padding-pinned | 1 | separated-pinned | 1.021 [0.984,1.058] | [0.983,1.075] | no_significant_difference |  |
| stats | padding-pinned | 4 | separated-pinned | 1.019 [0.943,1.102] | [0.950,1.074] | no_significant_difference |  |

All thread counts are retained in comparisons.csv. CPU migrations are a Linux software event over the controlled interval, include producer/main, and are not cache/coherence hardware events. Layout bytes are selected objects/active cells, not RSS.
