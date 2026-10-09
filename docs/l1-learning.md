# L1 原理与阅读路线

最后更新：2026-10-09；Codex。用户已授权助手完成所有算法，无手写任务。

阅读：work_queue.hpp → job_system.hpp → pool_test / steal_test → benchmark.cpp → measure.py。

需要理解：owner LIFO / thief FIFO 的局部性动机；检查与删除为什么要同一临界区；为什么执行任务不持队列锁；pending 与队列长度的差别；packaged_task 的异常如何进 future；通知代数如何避免丢唤醒；为什么只在归零时广播给外部等待者；提交与 shutdown 的生命周期锁；为什么池内任务不能等自己所属的池；粗任务和微任务 / 预入队与端到端各自测到了什么。

当前实现与证据见 L1 README 和 docs/status.md。互斥队列优先保障可验证性；若改 Chase-Lev，需要另验内存序、最后任务竞争、扩容和回收，不能仅凭“无锁”二字称更快。
