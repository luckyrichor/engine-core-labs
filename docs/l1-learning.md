# L1 学习材料与用户实现边界

最后更新：2026-10-01；Codex；engine-core-labs@65305d9 + 未提交修改。

每个 worker 拥有 deque。owner push/pop 在 front，最近任务先执行；空闲 thief 从 victim 的 back 取最老任务。mutex 版目标是解释互斥与任务所有权，不直接推论 lock-free Chase-Lev 的性能。本项目未实现/验收 steal，以下是实现约束和问题，不是实测结论。

用户只填写 WorkQueue::steal()：获得 victim 的 mutex，若 empty 返回 nullopt，否则移动 back 元素并移除，然后释放锁。不可持队列锁执行 Job；不可先检查 empty 再加锁（检查/移除必须原子）。函数必须在多个 thief 与 owner 竞争最后一个任务时只允许一个成功，其他返回空。

读代码顺序：work_queue.hpp → queue_test.cpp → job_system.hpp → pool_test.cpp → steal_test.cpp → benchmark.cpp → measure.py。

需要能讲清：

- 为什么 pop 与 steal 取不同端？对 locality 与分摊大任务可能有什么影响？
- 为什么任务出队后才执行？任务再 submit 或执行很慢时，持锁会怎样？
- wait 的 pending 是提交未完成数量，与队列长度为什么不同？future 怎样传递异常？
- shutdown 为什么先拒绝提交，再 drain 和 join？析构调用方不能是池内任务，这个约束在哪里？
- 虚拟机只有 4 vCPU，增加到 8 线程可能发生什么？短负载下为何提交/唤醒开销会影响曲线？以上先提出假设，完成实验后用数据回答。

验证顺序：1. empty/FIFO thief 与 LIFO owner；2. 8 个 consumer 抢 10000 任务 exactly-once；3. balanced 与集中 queue0 的 skewed workload；4. warmup 后多次测量。检查 Release 构建、设备/后台负载、checksum、stdev，不用单次最快值。

测量仅在用户完成核心后进行；本次 baseline smoke 不是曲线。对于锁竞争/睡眠唤醒的成本，后续可使用 profiler 补充证据，当前未做 profile。
