# L2 初始化与触页成本（2026-10-15，Codex）

来源 engine-core-labs@9134c1e 加本轮源码，精确 SHA256 见 device.json。

```bash
python3 experiments/l2-allocators/tools/cold_measure.py --output docs/measurements/2026-10-15-cold
```

三模式 × 十个新进程 × 512 个 64KiB 块 = 15360 个单次观测；总存活数据 32MiB。所有逐页 payload 校验通过。顺序轮换，无预热；页故障不是磁盘读取。32MiB 匿名映射与 64KiB 自定义 Arena 热路径分开解读。

| 模式 | 初始化 ms | 使用时 p50 us | 初始化+512次使用 ms | 初始化 minor | 使用 minor |
|---|---:|---:|---:|---:|---:|
| first_touch | 0.005 | 18.585 | 10.360 | 4.000 | 8192.000 |
| prefaulted | 9.717 | 0.250 | 9.861 | 8196.000 | 0.000 |
| malloc_growth | 0.001 | 18.796 | 10.320 | 4.000 | 8192.000 |

每格是逐轮指标中位数，不保证同一行所有格来自同一轮。setup_ns 含 mmap、预触页（如有）与块指针准备；逐次 allocate_and_touch_ns 不含 rusage，但仍有读钟成本。setup_plus_touches_ns 是初始化加逐次计时之和，不是整个进程 wall time，不含输出和采样间开销。setup 原始字段每行重复，只累计一次。

原来已有 first_touch / prefaulted；本轮补上启动耗时和缺页，避免只展示预触页之后的低延迟而隐藏成本。预触页把大部分缺页移到启动阶段，总时间不保证更少；匿名 mmap 不保证物理驻留，未用 mlock，也不是 Windows commit 的实现。malloc_growth 表示同时存活的分配越来越多，不保证每次调用发生系统堆扩容。不会把合成触页结果包装成实际引擎帧收益。
