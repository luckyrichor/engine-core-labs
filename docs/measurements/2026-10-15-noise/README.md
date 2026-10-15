# L1 同基线 A/A（2026-10-15，Codex）

来源 engine-core-labs@9134c1e 加本轮源码，精确 SHA256 见 device.json。命令：

```bash
python3 experiments/l1-job-system/tools/noise.py --threads 3 4 --output docs/measurements/2026-10-15-noise
```

固定 10000 个任务 × 50000 迭代、balanced、baseline，3/4 工作者 × prequeued/end_to_end × 10 轮 × A/B，共 80 次。每次内置完整负载预热，同输入独立串行求和和逐 ID exactly-once 均通过。A/B 用同一实现，仅标签不同，交替先后顺序。

| 工作者 | 阶段 | A/B 耗时比中位数 | 最小 | 最大 |
|---|---|---:|---:|---:|
| 3 | prequeued | 1.0159 | 0.9373 | 1.0551 |
| 3 | end_to_end | 1.0098 | 0.9954 | 1.0277 |
| 4 | prequeued | 1.0200 | 0.9646 | 1.0926 |
| 4 | end_to_end | 1.0565 | 0.9697 | 1.1891 |

运行工作区间 0.240–0.343s。guest 报告 steal_ticks 总和 0。原始时长、load-independent 固定输入及系统 steal 区间见 raw.csv；配对比值见 paired.csv。

相同实现也存在噪声。该区间不是置信区间，也不能直接拿作不同日期/负载的显著性阈值；只说明本机本轮固定输入的重复性。记录 guest steal 而非本线程调度离开时间，0 不排除 guest 内争用。3/4 工作者不等于严格 CPU 亲和性隔离，未修改系统调度参数。
