# L1 设计取舍

最后更新：2026-10-01；Codex；engine-core-labs@65305d9 + 未提交修改。

从 mutex deque 开始：它能把 exactly-once、empty 和任务所有权讲清楚，避免直接复制 lock-free 算法掩盖内存序问题。核心 steal 留给用户；victim 选择与 pool 周边由脚手架负责。

提交使用 packaged_task + future，异常到调用者；任务执行不持队列/状态 mutex，pending 归零由 condition_variable 通知。析构 drain 所有提交任务；调用方必须是 pool 外线程，任务不能在自己所属的 pool 上调用 wait/shutdown 或销毁 pool，否则可能自等待/自 join。

测量区分 balanced/skewed 与无 stealing baseline，不把均匀分配下的线程池吞吐冒充 steal 收益。核心标记与 steal_test 都通过后才写 CSV/SVG；当前返回显式阻塞，不绘制预设数据。
