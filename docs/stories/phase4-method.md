# 阶段4 S8：布局与线程亲和性的方法

2026-10-28更新 · Codex。阶段4要求tx与Windows各自实测；两机正式矩阵均完成，Windows VS CPU采样仍待补，阶段4未验收完成。阶段5正式实验前仍须用户决定swap，不能越过检查点。

## 对照与布局

`experiments/l1-job-system/phase4/benchmark.cpp`提供五组确定性构造负载。每次执行在计时前核对本机L1数据缓存行是64字节；Linux读sysfs，Windows调用GetLogicalProcessorInformationEx(RelationCache)。未知或不一致时失败，不能假定64字节后继续。

- positive：线程各自连续执行200万次relaxed原子自增。两组数组基址均64对齐；packed间隔8字节，1/2/4/8个活动槽都在同一缓存行；separated间隔64字节。这是最坏伪共享正向对照，不代表实际执行器。
- cold：每线程10万次、每次64个整数工作单元，只有计时前读取的seed字段改变布局。热循环是线程本地计算；计时后核对摘要。它与所有组的单线程配置共同构成反向控制。
- queue：实际SC Chase-Lev，owner推入并pop、其他线程steal，共10万任务/64工作单元。两个队列对象基址都64对齐；只切换top/bottom存储布局。**阶段3原默认已分行**，packed是受控反事实，不能声称修复了当前默认的未对齐问题。共享top及done计数存在真共享，填充不能消除。
- counters：实际JobSystem，5万任务/128工作单元，单生产者自动轮询目标，使pending、next、wake_cursor三个原子字段均被访问。一次仅填充pending、next或wake中的一个，通知策略/队列/锁/future不变。所有对象基址64对齐。字段偏移和对象字节数写入layout-audit.json；改变一个字段也会改变后续字段位置，不能推断某次收益只来自该原子本身。
- stats：阶段3实际one策略实验池，10万任务/64工作单元，只填充每worker统计块；不改队列、通知和统计更新算法。gcc下Worker仍640字节，统计块由48变64字节、起始偏移544变576；这是移开统计块与相邻同步字段，不能称Worker总内存减少。

默认别名仍为原布局：JobSystem=BasicJobSystem<0>、ChaseLev=BasicChaseLev<true>、Scheduler=BasicScheduler<false>。修改前后gcc默认sizeof分别392/192/48（JobSystem、ChaseLev、WorkerStats），未将候选改成默认。布局参数仅为实验开关。

队列/执行器逐ID校验seen恰好一次及工作摘要；校验槽各64对齐，避免校验数组重复引入伪共享。校验读在计时后，但任务内写校验槽仍有开销，不能称裸调度纳秒成本。positive核对最终计数，cold核对独立串行摘要。

## 绑核、计时与软件事件

两种模式保留相同CPU资格集合。tx继承进程可用CPU0–3；pinned模式worker i固定到eligible[i % count]，8线程超订。设置后读取线程亲和性精确核对；自建线程另采样首尾CPU。池在线程启动闸门释放前通过native handle设置并读回。生产者/main没有单独绑核。

Windows主矩阵从API实测的最高EfficiencyClass中每个物理P核取一个逻辑CPU，双方保持同一进程资格掩码，再单独切换worker绑核；**unbound仅指未逐worker固定，并非允许混跑P/E**。不把混合核变化当作绑核收益。当前实现只支持单处理器组内的资格掩码，不支持跨组主矩阵。

线程创建、亲和性设置与输入准备在计时外；自建线程的闸门释放/完成协调在计时内，join在外。JobSystem计时resume、提交和wait，future取回及池销毁在外；统计池计时resume、提交、join。组内边界一致，不跨case比较绝对吞吐。

Linux仅采perf软件事件cpu-migrations，perf启动为disabled，通过FIFO enable/disable及ack把采集窗口限定在布局实验区间；clock区间略窄于ack区间。perf的ack含结尾NUL，显式处理并核对。事件覆盖全进程所有线程，**包含未单独固定的生产者/main**，所以固定workers后迁移数仍可非零。不能将迁移数当缓存命中、HITM、失效或一致性流量，也不凭计时差断言其硬件机制。普通权限不足时仅测试进程sudo；未修改全局perf/ASLR配置。原始软件计数保留在raw.csv。

## 重复与判定

每组分别有1/2/4/8线程，预热1轮、正式10轮。每轮所有布局×绑核单元及两套独立基线A/A依次运行，顺序轮转；没有并发测量。swap使用前后变化令该次指标无效，不关swap。共享VM与常驻服务无法声称隔离，保存processes-before.txt并由A/A暴露噪声。

summary.csv以packed/unbound为基线，只有未绑核的布局比较及packed-pinned的纯绑核比较是单因素。summary-pinned.csv以packed-pinned为基线，用于固定worker后的布局比较。报告脚本只展示这些单因素比值；不能把separated-pinned相对packed的组合变化叫作单一布局收益。全部判定复用bench.verdict，无新增阈值。

selected_layout_bytes是相应对象或活动槽字节数：positive/cold为活动槽，queue为队列元数据，counters为整个JobSystem对象，stats为Worker大小×worker数；**不是RSS或进程总内存**。原始数据、设备、逐文件源码SHA256、布局偏移、每次绑核读回证据、轮次顺序均随结果保留。Windows保存原电源方案、要求接电、启用高性能，并在finally恢复和读回；该路径已在2026-10-28实测，恢复后另行SSH读回原平衡方案；Windows Visual Studio CPU采样尚未完成。

## 复现

```sh
export ECL_DATA="${ECL_DATA:-$HOME/data/engine-core-labs}"
cmake -S . -B "$ECL_DATA/build-phase4" -DCMAKE_BUILD_TYPE=Release -DECL_DATA="$ECL_DATA"
cmake --build "$ECL_DATA/build-phase4" -j 2
ctest --test-dir "$ECL_DATA/build-phase4" --output-on-failure
python3 -m bench.phase4 prepare
python3 -m bench.phase4 measure --binary "$ECL_DATA/build-phase4/experiments/l1-job-system/phase4/phase4_bench" --output docs/measurements/<新的日期>-phase4-tx
python3 -m bench.phase4_report docs/measurements/<新的日期>-phase4-tx
# 有matplotlib时可加 --plots，图表为独立SVG。
```

Windows本轮已将同源快照复制到E:\ecl-scratch，在vcvars64环境中对`experiments/l1-job-system/phase4`单独CMake构建，随后在快照根执行相同Python入口。源码快照须保留experiments/bench/tools及workloads/phase4，source-snapshot.json记录真实git来源和源码SHA256，runner先核对指纹；原仓库不在Windows并行编辑。2026-10-28实机在E:\ecl-scratch\phase4-20261028构建并运行成功，工具链、flags与契约日志见Windows结果目录validation；VS CPU采样未验证。
