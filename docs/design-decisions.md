# 设计取舍

最后更新：2026-10-09（北京时间）；Codex。当前源码版本由测量目录的 HEAD + SHA256 标识。

## 分工与算法选择

用户改为助手完成所有核心。steal 使用互斥 deque；Arena 用对齐 bump + 批量 reset，Pool 使用空闲块内部链表。不存在脱离负载的“最佳算法”：mutex deque 容易证明 exactly-once，Arena 适合整体释放，Pool 适合有上限的同尺寸对象；不同生命周期不可只按吞吐排总榜。

## L1：减少对照混杂

旧池每任务完成都广播、全局状态锁与轮询会带来开销。perf 的旧版 787 样本中，52.10% 落在负载调用，7.37% 在 kernel spin unlock，后者多经 futex wake / try_to_wake_up；stat 有 66765 次上下文切换。它确认唤醒开销存在，不能单独证明历史 4-worker 异常因果。

新池按 worker 通知、pending 归零才通知 drain，采用同一个执行器开关 cross-queue stealing。保留必要的队列锁 / 生命周期锁，不在无证据下改为 lock-free。预入队与端到端分开，粗负载与微任务分开，完整预热、10 轮、后台负载、逐 ID 校验与串行参考均入证据。增加 8 workers 是四核主机上的过量订阅实验，不承诺线性扩展。

## L2：替换无法区分实现的指标

用整批计时摊薄时钟成本；分位数命名为 batch ns/op。加入 malloc 通用基线和非整齐大小，明确写入成本、校验记录表和回收语义。固定 pool 不需要连续空闲，删除外部碎片结论；另建全空间填满后隔块释放的 first-fit 示例，验证大请求失败及合并恢复。

保留 pool live bitmap / stack 记录表，用于可复现的非法释放检查，并输出元数据成本；删除检查会改变被测实现，不把当前数字冒充无检查分配器。Arena 不逐对象析构，通用对象仍需调用方处理析构。

## 验证边界

Release、TSan、ASan/UBSan 分开构建。TX 的 TSan 映射问题用进程级 setarch -R 处理；LeakSanitizer 最初因权限 / ptrace 限制全部失败；使用 sudo 测试进程且 detect_leaks=1 后 8/8 通过。普通用户关闭泄漏检测的 ASan/UBSan 也为 8/8，两个口径单独保留日志；不是将关闭泄漏检测称作无泄漏。旧测量与旧分工只作为历史保留。
