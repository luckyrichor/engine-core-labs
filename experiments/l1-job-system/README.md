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

默认包含 balanced / skewed / heterogeneous，默认请求 10000 个任务、每任务 50000 次计算、10 轮；粗负载先按最快均衡预入队样本校准到至少 0.2 秒，实际迭代数以 device.json 和 CSV 为准。微任务使用 1 次计算、不校准，专门观察提交 / 同步开销，不作为业务任务加速结论。

完整默认套件 3 分布 × 2 模式 × 4 线程数 × 2 计时阶段 × 10 轮 = 480 次；旧版两分布各 320 次仍对应旧来源。本轮只新增 heterogeneous 的 160 次，命令加 `--distributions heterogeneous`。每次进程内同一个池先完整执行等量负载预热；配置顺序轮转并交替反向。

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

## 计算量不均的中间负载

`heterogeneous` 每个队列分配相同数量的任务，ID 每 128 个有一个任务做 100 倍迭代（10000 个任务中 79 个重任务，占 0.79%）。对 1/2/4/8 workers 的周期映射，重任务集中在 queue0：这是刻意构造的计算量不均，不是随机生产负载。任务迭代数上限 1000000，放大后最多100000000；串行参考也应用同一权重。工作窃取不能拆分已经开始执行的单个重任务，收益仍受重任务粒度、剩余队列与四核机器限制。

```bash
python3 experiments/l1-job-system/tools/measure.py --distributions heterogeneous --output .local/l1-heterogeneous
```

## 主机负载的解读

loadavg 是延迟的一分钟主机平均，不标识“后台活动”。旧微任务组仅持续 8.54秒、距粗任务结束约19秒，loadavg 包含前一组影响；不能据此断言有后台争用，也不能断言没有。新版保存每次进程时间区间的 `/proc/stat` 及子进程累计 CPU 时间；这些可辅助区分主机总 CPU 与基准进程使用量，但不是精确的后台归因。微任务旧数据的高噪声和短时长仍在报告中说明，不将中位数变化直接归因于某一把锁或唤醒。

## 随机重任务对照

`heterogeneous_random` 先在全部任务ID中无放回选择与周期版完全相同数量的重任务，再按 `ID % workers` 均分队列任务数。10000任务仍为79个重任务、100倍迭代，总计算量一致。使用SplitMix64、带拒绝采样的部分Fisher-Yates，固定种子可重建；随机布局不依赖线程数，两个策略及两阶段同种子使用完全相同布局。

原周期版的成本偏斜来自 `ID % 128 == 0` 与1/2/4/8线程数的相关性；`ID % workers`本身是正常轮转。随机版消除这项刻意相关性，但仍是合成的小对象计算负载，不代表采集到的真实业务。

```bash
python3 experiments/l1-job-system/tools/measure.py --distributions heterogeneous heterogeneous_random --seed 20261009 --output .local/l1-random
```

两分布 × 两策略 × 四线程数 × 两阶段 × 十轮 =320次。随机版每轮使用不同种子20261009–20261018，策略之间按种子配对；周期版为固定布局十次重复。CSV保留seed、重任务数、布局hash、各队列重任务数与总迭代量；串行参考按种子单独计算，生成布局在计时外。

随机版的标准差包含布局差异及运行噪声，不能把它纯粹解释为同输入时延波动；十个种子也不是完整业务分布。收益按同种子配对比较，不预设“必定更小”或“每个样本都更快”。

### 参数维度扫描

默认 `tools/measure.py` 现含 `heterogeneous_random`；完整矩阵较此前增加十六配置 / 轮。

```bash
python3 experiments/l1-job-system/tools/scan.py --build build --output .local/l1-scan
build/experiments/l1-job-system/job_benchmark stealing 4 10000 50000 heterogeneous_random prequeued 20261009 10 100
build/experiments/l1-job-system/job_benchmark stealing 4 10000 50000 uniform_cost prequeued 20261009 0 10
```

末两参数为重任务占比（千分数）和倍数上界。`heterogeneous_random` 占比 0 保留历史默认 ceil(tasks/128)，正整数为 ceil(tasks*permille/1000)；重任务等重。`uniform_cost` 忽略占比，每个任务的整数迭代成本在 base..base*倍数间无偏均匀采样。随机形状独立于线程数，在计时外生成；双策略同 seed / 总工作量。布局 hash 现覆盖实际成本值，而非仅重任务位置；不能跨版本将 hash 当作相同布局判据。

scan.py 固定四线程、预入队、10000 任务、十个种子；两点形状 base=50000，均匀成本 base=round(100000/(倍数+1))，使期望平均成本约 50000，扫描占比 0.1% / 1% / 5% 与倍数 10 / 100，另加区间均匀成本 10 / 100。各形状工作量不同，不直接比较吞吐高低；看配对收益以及逐轮范围。仍为合成分布。
