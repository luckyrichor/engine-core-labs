# S2 定位材料（不用于吞吐提升判定）

先在未加入条件通知参数的旧策略上做 submission.csv 分段插桩：4 producers、100000任务、4 workers；gate/prepare/enqueue/wake 是生产者累计耗时，包含时钟插桩开销。
时间分别为 5.717 / 51.656 / 72.164 / 84.162 ms；合计约213.699ms，不能作为约69.655ms墙钟内的百分比分解。wake路径最高约39.4%，但包含唤醒互斥锁与notify，不足以证明系统调用是主因。

perf-stat-baseline/sleepers 为两次独立软件事件诊断，100000任务、4 producers/4 workers；不是配对吞吐结论。
perf-flat 为旧策略百万任务 cpu-clock 499Hz 的全进程平面采样：979样本、0 lost；含输入验证和结果校验。Release没有显式frame pointer，因此不使用 -g 产生的调用栈做因果解释。work约23.70%、pthread_mutex_lock约8.27%、futex_wake约3.58%为全进程采样，不等于提交路径内占比。

strace 的同一 10000 任务派生负载：旧策略10266次 futex，条件通知10248次。两次单独trace且会扰动调度，这个微小差值不构成系统调用减少的证据；正式显著性只看各 S2 目录的 raw/summary 与 A/A。
普通用户 perf 和 ptrace 被环境拒绝后，仅对测试进程使用 sudo；没有调整系统安全开关。追踪进程正常退出、payload校验通过；strace errors列是系统调用返回值计数，不是payload校验结果。

命令结构：
```sh
sudo perf stat -e task-clock,context-switches,cpu-migrations,page-faults -- phase3_bench --meta ...real-100000.meta.json --impl legacy --threads 4 --producers 4
sudo perf record -e cpu-clock -F 499 -g -o "$ECL_DATA/phase3/profiles/baseline.perf.data" -- phase3_bench --meta ...real-1000000.meta.json --impl legacy --threads 4 --producers 4
sudo perf report -f --stdio --call-graph none --no-children --percent-limit 0.5 -i "$ECL_DATA/phase3/profiles/baseline.perf.data"
sudo strace -f -c -e trace=futex -- phase3_bench --meta ...real-10000.meta.json --impl legacy --threads 4 --producers 4
# 改为 --impl sleepers 重复诊断。不要拿 strace 耗时作为性能结果。
```

原始 perf.data 二进制在 ECL_DATA，不进入仓库。两个 device.json 分别记录定位前后的源码；正式矩阵统一使用 audit.json 中的单一源码指纹。
