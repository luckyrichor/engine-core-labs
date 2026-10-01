# L1 Job System 脚手架与测量框架

最后更新：2026-10-01；Codex；engine-core-labs@65305d9 + 未提交修改。**核心未完成，L1 未整体验收。**

从仓库根运行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 2
ctest --test-dir build --output-on-failure
python3 experiments/l1-job-system/tools/device.py
python3 experiments/l1-job-system/tools/measure.py
```

已有 mutex deque 的 owner push/pop、固定线程数 pool、future 异常传播、drain/shutdown、balanced/skewed 调度负载和测量工具。steal() 在 include/work_queue.hpp 保留 TODO，由用户手写；实现后将 stealing_implemented 置 true，并配置 `-DUSER_STEAL_IMPLEMENTED=ON` 后重建（去掉拒绝未实现的负向测试）。

当前 steal_test 退出 77（明确 skip），measure.py 在创建任何输出前拒绝运行。benchmark 的 stealing 模式退出 2。baseline smoke 仅验证测量框架能跑，不等同 stealing 实验。本次没有线程数/吞吐关系曲线，也不声称 L1 完成。

用户实现后先通过 steal 并发 exactly-once 测试，再运行 measure.py：balanced/skewed × baseline/stealing × 1/2/4/8 线程 × 3 次，排除 warmup，生成 raw.csv、summary.csv（均值/样本标准差）、device.json 与两张 SVG。设备记录包含 git HEAD、未提交状态、CMake 编译配置、实际 CPU/OS/编译器、负载参数。checksum 必须跨运行相同，失败时拒绝出图。不要混用不同机器/编译模式的数据。

`job_benchmark baseline 2 100 100 balanced` 可单独运行框架 smoke；输出不作为完整实验结论。wall time 包含任务提交和等待完成，不仅是纯任务执行；短任务会被队列、唤醒与提交开销主导，解释曲线时必须说明。
