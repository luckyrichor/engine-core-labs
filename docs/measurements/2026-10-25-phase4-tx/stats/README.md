# S8 stats

10 rounds + 1 warmup. All binding/layout cells interleave and rotate in the same round. packed@aa and packed-pinned@aa are independent noise baselines.

summary.csv uses packed (unbound) baseline: compare unbound layouts and packed-pinned for one-factor effects. summary-pinned.csv uses packed-pinned baseline: compare pinned layouts only. Joint layout+binding changes are not a one-factor result.

Source/topology in device.json; actual affinity/readback in executions.jsonl. Windows unbound has no per-worker pin but retains the same P-core eligible mask. Producer/main is not pinned.

Reproduce with python -m bench.phase4 measure --binary <binary> --output <new directory> --cases stats.

Status: complete. stub=0.
