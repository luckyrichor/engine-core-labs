# VS CPU采样与热点分析（2026-10-28，已完成）

Codex在luowindows使用VS2022 VSDiagnostics/CpuUsageHigh采集四份独立trace，并用同机VS内置TraceEvent 3.1.9读取ETL、以本次Release PDB解析项目符号。源实现指纹仍为fd1e81c30451b94e58e8a80e3e8e17513ae569522c3a4ed0fdcc98f757989c24，与双机正式矩阵一致；未修改或重编译实验实现。阶段4剩余采样项已完成。

## 实际样本

| 诊断负载 | CPU样本 | 有栈样本 | 项目模块叶样本 | 项目叶未解析 |
|---|---|---|---|---|
| positive | 5764 | 5764 | 5710 | 0 |
| queue | 3779 | 3766 | 3086 | 0 |
| counters | 4196 | 4192 | 1162 | 0 |
| stats | 13188 | 13185 | 1763 | 0 |

四进程exit_status均0，TraceEvent EventsLost均0；合计26927样本。请求/ETL记录间隔0.25ms，不能推断每个时刻均精确采到4000次。PDB GUID 5f4b0268-6930-4f9d-bdc1-ca8e3b897346、Age1，签名与本机PDB匹配；二进制/PDB/trace SHA256见[manifest](trace-manifest.json)。少数样本无栈，未将其补成项目样本。JSON只导出目标进程，系统模块保留聚合名，不公开内核地址；原始解析日志与trace仅在两机外部目录。

## 热点与解释

- positive：原子自增工作线程lambda占self 5710/5764（99.06%），inclusive 5720（99.24%）。优化编译内联，函数级采样无法把单条原子指令或缓存一致性成本单独量化；正式布局对照才是性能收益证据。
- queue：消费者lambda self2550/3779（67.48%），work self447（11.83%）；消费者inclusive3416（90.39%）。原子队列、done、验证槽、yield可能内联在同一函数，不能把整个lambda百分比称为steal或伪共享成本。
- counters：JobSystem::run self296/4196（7.05%）、inclusive3026（72.12%），submit self272（6.48%），work self317（7.55%）。相当多叶样本归于ntdll；系统符号未下载解析，不能进一步断言具体锁/堆函数，更不能把run inclusive解释为pending字段成本。
- stats：BasicScheduler::run self859/13188（6.51%）、inclusive10220（77.49%），大量叶样本在ntdll/msvcp140。该定位显示运行循环下存在项目外执行成本，但没有把统计更新的边际成本分离出来；仍维持正式矩阵“无显著布局收益”和原默认。

每份*-hotspots.json有项目self/inclusive函数完整表及目标进程模块分布。inclusive每样本对同一函数只计一次，不同函数可重叠，不能相加当总占比。系统模块只有模块级归属，未冒称全部系统符号已解析。

## 口径与复现

Windows目录E:\ecl-scratch\phase4-20261028；source为历史8be95f4+dirty同源快照，build\Release保留原EXE/PDB。prepare-inputs.py从原descriptor生成独立诊断数据：positive每worker10M次（正式2M，5倍），queue/stats各1M任务（正式100k，10倍），counters300k（正式50k，6倍）；原工作单元不变。独立名称/meta与正式矩阵分开，不能混入正式CSV。

在source根设置PYTHONPATH=source、PYTHONUTF8=1，运行prepare-inputs.py（需将脚本复制到scratch根），然后用[collect-positive.ps1](collect-positive.ps1)、[collect-rest.ps1](collect-rest.ps1)执行前台采集。脚本仅用于本轮固定scratch；再次运行请改新目录/输出文件，勿覆盖证据。使用新GUID启动VSDiagnostics /launch /launchArgs /loadConfig，等待目标退出再stop，实际会话GUID与命令返回结果见collector-log.txt。

采集时接电、高性能、4workers逐个绑至P核，资格集合0,2,4,6,8,10,12,14；用SetThreadExecutionState临时请求系统/显示保持唤醒，finally释放，不更改永久睡眠/网络配置。原/恢复方案均381b4222-f694-41f0-9685-ff5bb260df2e，并另行SSH读回平衡方案；没有残留实验/采集进程。

[export-hotspots.ps1](export-hotspots.ps1)用expandDiagSession提取ETL，TraceLog.OpenOrConvert及GetCallStackForEvent枚举目标进程SampledProfileTraceData。仅对项目模块调用LookupSymbolsForModule；SymbolReader只允许本轮scratch内本地产物。导出脚本的原始JSON包含系统地址，不能直接提交；本仓库JSON已仅保留项目函数和系统模块聚合。

采样覆盖**完整目标进程**（准备、执行、校验、销毁），不是正式计时时窗；放大输入减少采样不足，不能用这些百分比量化正式负载的净调度成本。每case一份诊断样本，没有采样A/A，不据此给出新的加速比、缓存命中率或因果结论。正式矩阵及其负面结果保持不变。

## 原始证据位置

四份.diagsession保存在Windows上述profiling目录及tx的$ECL_DATA/phase4-windows-20261028/profiling；已逐一核对跨机SHA256。ETL/ETLX及原始解析日志不进public仓库。expandDiagSession可能调整会话包，manifest记录的是展开后实际保留并传回的包哈希；collector-log里的最初大小因此可能不同，不能按最初大小误判传输损坏。

参考：[微软VSDiagnostics说明](https://learn.microsoft.com/en-us/visualstudio/profiling/profile-apps-from-command-line?view=visualstudio)、[微软TraceEvent样例](https://github.com/microsoft/perfview/blob/main/src/TraceEvent/Samples/41_TraceLogMonitor.cs)。
