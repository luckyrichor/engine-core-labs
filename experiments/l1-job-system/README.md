# L1：工作窃取与线程池测量

最后更新：2026-10-09；实现：Codex。当前测量来源见 `docs/measurements/2026-10-09/*/device.json`，历史目录对应各自旧版本。

## 算法与对照

owner 在 mutex deque 的 front 压入 / 弹出，thief 从 back 取最老任务；检查与移除在同一锁内，执行在锁外。基线与窃取模式共享同一个 executor，区别是是否扫描其他队列，以及提交时是否通知一个轮转 thief。每个 worker 有独立 CV 和通知代数，先记录代数再查任务，避免通知丢失；没有 1ms 轮询。完成只原子递减 pending，归零才通知外部 wait，停止时才唤醒全部 worker。

这是易验证的互斥队列，仍有队列锁、提交生命周期读锁、原子计数及 future 分配。不是 lock-free 引擎调度器。池内任务不能 wait / shutdown 自己所属的池；pause/resume 是外部测量控制，不与 shutdown 并发调用。

## 测量方法

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
python3 experiments/l1-job-system/tools/measure.py --output .local/l1-coarse
python3 experiments/l1-job-system/tools/measure.py --iterations 1 --min-seconds 0 --output .local/l1-overhead
```

默认请求 10000 个任务、每任务 50000 次计算、10 轮；粗负载先按最快均衡预入队样本校准到至少 0.2 秒，实际迭代数以 device.json 和 CSV 为准。微任务使用 1 次计算、不校准，专门观察提交 / 同步开销，不作为业务任务加速结论。

每套 2 分布 × 2 模式 × 4 线程数 × 2 计时阶段 × 10 轮 = 320 次。每次进程内同一个池先完整执行等量负载预热；配置顺序轮转并交替反向。

- `end_to_end`：提交开始到 future 收集 / drain 完成，包含串行提交。
- `prequeued`：暂停所有 worker，全部提交后才启动计时 / resume，排除入队；仍包含执行、启动屏障、future 等待和 drain，不叫纯调度时间。
- `submit_seconds` 单独保留；预热、结果校验均不在计时区间。
- 每个 ID 原子计数必须恰好为 1；结果求和与另一次串行计算比对。不是拿第一份并行结果作真值。已知单步负载结果另有契约测试；串行参考共享计算核，不能独立证明核本身的所有算术正确性。
- raw.csv 保存每轮；summary.csv 有中位数、均值、样本标准差、CV 和时长范围；SVG 的误差线是标准差，非置信区间。
- device.json 保存源码 / 可执行文件 SHA256、基线提交及工作树状态、编译配置、CPU，以及每次运行前后 loadavg / CPU pressure / proc_stat。后台负载记录不等于隔离机器。

当前源代码的 sanitizer 配置：

```bash
cmake -S . -B .local/tsan -DCMAKE_BUILD_TYPE=Debug -DENGINE_SANITIZER=thread
cmake --build .local/tsan -j2
TSAN_OPTIONS=halt_on_error=1 setarch x86_64 -R ctest --test-dir .local/tsan --output-on-failure
```

TX 的 TSan 需测试进程关闭 ASLR 才避免映射冲突，未更改系统设置。perf 旧版证据见本轮 measurements：基线 b8be686，RelWithDebInfo + frame pointer，4 workers / 1000000 × 1000 / balanced。观察到唤醒路径消耗 CPU，不足以证明旧曲线异常全部由脚手架引起；新的共用 executor 对照减少这一混杂因素。不同负载、构建、计时口径的 perf / 吞吐不能直接作前后百分比比较。
