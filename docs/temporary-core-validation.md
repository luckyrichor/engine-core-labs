# W6 维持：临时核心实现验证

最后更新：2026-10-02（北京时间）；Codex；engine-core-labs@b8def96 + 本轮工作树修改。

用户明确允许临时实现用户函数以推进后续验证，结束后删除。此次临时用 mutex 保护从 deque 尾取任务，将 stealing_implemented 临时置 true。增强 steal_test：每个任务独立计数，同时并发生产和消费，防止「丢一条、重复一条」被总数掩盖。

临时版本 Release 构建，5/5 ctest 通过；测量 balanced / skewed、baseline / stealing、1/2/4/8 线程，每组合 3 次，2000 tasks × 2000 iterations，48 个采样。产物在 measurements/2026-10-02-temporary-steal/：raw.csv、summary.csv、device.json、两张 SVG。元数据含 baseline HEAD、dirty 工作树、每份实际源文件 SHA256、设备/编译器/构建配置；标签 temporary-reference-removed-after-test。

这是临时参考实现的测量，不是用户实现的成绩，也不证明所有负载 work stealing 更快。设备只有 4 vCPU，8 线程属于超订阅；标准差与重复数据保留，避免只挑最快的一次。

测量后 **work_queue.hpp 已恢复到开工内容，git diff 为零**，steal 返回 nullopt、implemented=false。恢复后重新构建：5 passed + 1 skipped；测量入口仍会拒绝未实现核心。用户算法与最终 L1 验收仍待办；W6 的专项测试/测量工具维护已完成。
