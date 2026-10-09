# 后续评审：成本不均、尾部与计时解释

Codex，2026-10-09。来源 engine-core-labs@2ea3b9a + 本轮工作树修改；每组device.json保存实际源码与二进制SHA256。旧2026-10-09目录仍对应上轮版本，没有更换旧CSV来源。

## L1

heterogeneous：任务数轮转均分，10000任务中79个（ID%128==0）做100倍迭代，重任务落在queue0。它是故意构造的成本偏斜，不是随机业务负载，也不把已执行的任务拆分。实际普通任务50000次、重任务5000000次；两模式、四线程数、两阶段、十轮，共160次，每轮0.431–1.699秒。

预入队吞吐中位数：

| workers | baseline | stealing | baseline CV | stealing CV |
|---|---|---|---|---|
| 1 | 5988 | 5979 | 0.3% | 0.6% |
| 2 | 8273 | 11737 | 1.1% | 1.1% |
| 4 | 10262 | 22605 | 1.9% | 3.4% |
| 8 | 10266 | 22462 | 5.4% | 4.0% |

四线程约2.2倍限定到这个fixture和四核VM，不能把极端单队列约3.6倍当所有不均负载收益。

loadavg是延迟的主机平均；device.json新增RUSAGE_CHILDREN累计CPU时间，配合每轮前后的proc_stat和时间区间，可辅助检查基准CPU量，不是后台精确归因。旧微任务解释见 ../../review-2026-10-09.md。

## L2

allocators/raw.csv：80行批量计时，保留原有分配口径；Arena的单次成本字段留空，操作数1，含时钟区域只作观测，不除以256解释为reset耗时。

allocators/tail.csv：四分配器+clock_write_control × 两模式 × 十轮，共100行，每行102400次独立计时。tail-windows.csv保存40000个窗口最大值；所有窗口的最大值和p99已与汇总逐组核对，payload checksum通过。

aligned模式跨轮中位数：control / Pool / Stack / Arena 的instrumented p50 / p99 / p99.9均约30 / 41 / 50ns；malloc约40 / 65.5 / 85.5ns。这表示当前插桩没有解析出三种自定义分配器的几纳秒增量差异，不是证明它们同速。control的最大值53390ns，比这里任一分配器最大值还大；系统或测量噪声本身会产生孤立长间隔，不能据此诊断分配器故障。逐次读时钟本身会扰动缓存和时序。

记录逐窗口最大值让停顿不被平均稀释，但不建立跨机器排序、不从分位数直接减控制值、不把这个热小对象malloc场景外推到长存活和跨线程释放。

## 验证

Release CTest8/8；TSan CTest8/8（进程级setarch -R），另跑heterogeneous stealing smoke，见heterogeneous-tsan.csv；ASan/UBSan/启用泄漏检测CTest8/8（sudo测试进程）。新tail通道在ASan/UBSan/泄漏检测下完成100行并验证payload，见tail-sanitizer.json；其计时数据不作为性能结果。

测量先完成，再构建/跑sanitizer，不互相污染本项目CPU负载。主机未独占，保留主机CPU/pressure/loadavg记录。
