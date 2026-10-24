# 阶段 3：方法、接口与验证边界

2026-10-25 · Codex；依据已确认方案 S1/S2/S3。此文件描述方法，不预写性能结果。

## 数据与时间

真实输入为 BistroInterior.fbx（SHA256 见 docs/datasets/bistro-costs/collection.json）。
工具 phase3_cost_collector 使用 ufbx 0.23.1，对每个真实 mesh instance 的所有顶点做 geometry_to_world 变换及紧包围盒归约。
每个实例先预热 2 次，再测 5 次取中位数，共 1186 个实例；原始逐实例统计与冻结直方图在 docs/datasets/bistro-costs/。
单次几何任务的时钟、数据访问等均包含在采集值中，不能把采集值解释成纯数学运算成本。

采集值按校准整数工作循环的 ns/unit 换算，**时间压缩 16 倍**后量化为整数，保持分布形状的近似，再用阶段 2 的确定性 PRNG 重采样至 10^5/10^6 任务。
正式调度测量运行的是这个成本代理，不是再次调用真实网格代码；不把 tasks/s 解释为引擎或 FBX 的实际吞吐。
均匀控制使用近似相同平均成本，指纹与数据和真实分布组分开。

S3 使用工作量为 64～524288 次整数循环的 8 档，按本机采集校准约为 100ns～1ms；每档总工作量固定为 2^23 次循环，块大小 1/16/256，线程 1/2/4。

## 调度口径

- 自研静态、偷一、偷半、Chase-Lev 共用相同的外部生产者 inbox、工作主体和校验。worker 将入口任务转入 owner deque；不能将入口锁成本归入“全执行器无锁”。
- Chase-Lev 是固定容量、保守 SC 版本；top/bottom/槽均为 seq_cst 原子。单 owner push/pop、多 thief steal；最后一个元素用 top CAS 仲裁。ID 指向直到 join 后才释放的不可变任务存储，不做运行期扩容/回收。参考算法：[Lê 等，PPoPP 2013](https://fzn.fr/readings/ppopp13.pdf)；本实现并未宣称采用论文优化后的最弱内存序。
- 静态分发按任务 ID % workers；偷半按受害队列当前大小取上半批到本地；每个 worker 独立记录偷取尝试、成功和搬移任务数；stolen_tasks 是移动次数，同一任务可能多次移动，不是唯一任务数。idle_fraction 只计睡眠等待，search_fraction 计扫描/转移开销；二者是插桩观测，有观测成本。
- 自研 S1 的计时包含最终 worker join 固定开销；池创建在计时外。prequeued 排除 producer 提交，但包含 inbox 转移与启动释放；end_to_end 包含提交。
- oneTBB 用 task_arena + task_group；prequeued 先 run 到等待释放的任务，再开始计时/释放，wait 完成；T=1 时由 wait 的调用线程执行，不用阻塞调用线程等待一个永远无人执行的工作。
- Taskflow 每块一个图任务；prequeued 排除建图与 run 调度，计时释放和完成；end_to_end 包含建图/运行。std::async 为 T 个分片，prequeued 排除 async 创建，end_to_end 包含创建；这不是逐元素创建线程。
- 每个元素 ID 用相同原子次数与工作摘要校验，结果在计时后与独立串行循环逐项比较。校验开销会影响最小粒度的适用范围，不能据此声称裸执行器的纳秒成本。

## S2 的定位与单因素变化

在旧策略上先采集可选提交分段计时（gate/prepare/enqueue/wake）、perf 软件事件/cpu-clock、futex 系统调用统计。
插桩各阶段之和是多个生产者的累计耗时，不是墙钟占比；perf 覆盖整个进程，含输入验证和结果校验，不把全进程样本直接解释成提交路径内的占比。
普通权限的 perf/ptrace 被环境拒绝后，仅使用 sudo 测试进程，没有修改 perf_event_paranoid 或全局 ASLR。

本轮选择只改变通知条件：仍每次提交递增 generation、仍用原队列/生命周期锁/future，不改批量接口；仅当目标 worker 在同一 wake mutex 下标为 sleeping 时 notify。
即使不 notify，generation 仍递增，覆盖“扫描完成到准备等待”的窗口；sleeping 在 wait 原子释放互斥锁之前置位，避免丢唤醒。
默认 wake_sleepers_only=false，旧策略保持可回退；S2 两组在相同源码版本下只切换此参数。
逐批提交延迟每批最多 32 次提交，含 producer 持有 future 的开销，1/2/4 生产者分开统计，样本不足100不报有效 p99。

## 实验流程

```bash
export ECL_DATA="${ECL_DATA:-$HOME/data/engine-core-labs}"
cmake -S . -B "$ECL_DATA/build-phase3" -DCMAKE_BUILD_TYPE=Release -DECL_DATA="$ECL_DATA"
cmake --build "$ECL_DATA/build-phase3" -j 2
ctest --test-dir "$ECL_DATA/build-phase3" --output-on-failure
python3 -m bench.phase3 prepare
python3 -m bench.phase3 measure --suite all --output docs/measurements/<新的日期>-phase3-tx
python3 -m bench.phase3_report docs/measurements/<新的日期>-phase3-tx
```

每配置预热1轮、正式10轮，顺序轮转并加入基线 @aa；两口径独立记录。swap 前后值变化使该次所有指标作废；本轮不关闭 swap。
每组 device.json 包括源码逐文件 SHA256、编译器、数据校验和、外部库回执与环境状态。8线程为超订，不能用于物理扩展性结论。
有常驻服务的共享虚拟机不声称隔离；进程快照与 A/A 用于暴露噪声，噪声重叠仍按唯一 verdict 规则判定。

TSan：只在测试进程用 setarch x86_64 -R，不改全局 ASLR；外部库未使用 TSan 构建，所以 TSan 验证自研队列/执行器，外部参照运行 Release 一致性检查。
Windows 和 UE 的跨平台补测在原方案阶段8执行，本阶段只生成 tx 数据。新实现、测量是否通过，以故事报告和原始目录为准。
