# L1 Job System 脚手架与测量框架

最后更新：2026-10-02；Codex；engine-core-labs@59c123b + 本次工作树修改。**L1 基本完成：Codex 参考实现通过，用户手写替换待办。**

从仓库根运行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 2
ctest --test-dir build --output-on-failure
python3 experiments/l1-job-system/tools/device.py
python3 experiments/l1-job-system/tools/measure.py
```

已有 mutex deque 的 owner push/pop、固定线程数 pool、future 异常传播、drain/shutdown、balanced/skewed 调度负载和测量工具。steal() 在 include/work_queue.hpp 现保留 Codex 参考实现，按用户要求标注作者；用户以后自行替换。默认 USER_STEAL_IMPLEMENTED=ON，包含 stealing smoke；如果恢复未实现占位，须将源码标记置 false 并配置 `-DUSER_STEAL_IMPLEMENTED=OFF`，恢复拒绝未实现的负向测试。

当前 6/6 ctest 通过、无 skip。参考实现的48次采样及线程/吞吐曲线在 docs/measurements/2026-10-02-codex-reference/，标签 codex-reference-user-exercise-pending。历史 temporary-steal 目录保留原始临时验证记录。参考数据不代表用户手写成绩，也不证明每种负载 stealing 都更快。

用户实现后先通过 steal 并发 exactly-once 测试，再运行 measure.py：balanced/skewed × baseline/stealing × 1/2/4/8 线程 × 3 次，排除 warmup，生成 raw.csv、summary.csv（均值/样本标准差）、device.json 与两张 SVG。设备记录包含 git HEAD、未提交状态、CMake 编译配置、实际 CPU/OS/编译器、负载参数。checksum 必须跨运行相同，失败时拒绝出图。不要混用不同机器/编译模式的数据。

`job_benchmark baseline 2 100 100 balanced` 可单独运行框架 smoke；输出不作为完整实验结论。wall time 包含任务提交和等待完成，不仅是纯任务执行；短任务会被队列、唤醒与提交开销主导，解释曲线时必须说明。
