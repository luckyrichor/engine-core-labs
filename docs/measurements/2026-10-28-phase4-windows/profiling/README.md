# VS CPU采样待完成：交接入口

截至2026-10-28（北京时间），正式矩阵已取回；VSDiagnostics和CPUUsageHigh配置、TraceEvent3.1.9已确认存在，但采样前Tailscale ping与SSH持续超时。没有生成或分析.diagsession，不写成“采样通过”。prepare-inputs.py是已准备、**尚未在Windows执行**的诊断输入生成器；正式矩阵使用原workloads/phase4，不使用诊断放大输入。

Windows工作目录E:\ecl-scratch\phase4-20261028，source为同源快照，build\Release保存phase4_bench.exe与PDB。恢复连接后先hostname确认LUOWINDOWS、核对AC及当前电源方案；当前最后已确认原平衡方案恢复。不要修改网络/防火墙来掩盖暂时不可达。

1. 在快照根设置PYTHONPATH为source，PYTHONUTF8=1；执行本目录prepare-inputs.py，生成profiling下的独立descriptor/meta。诊断正向10M次自增、queue/stats各1M任务、counters300k任务，比正式负载放大，不能将其耗时混入正式CSV。
2. 用Visual Studio自带采样器：

```text
F:\VS\IDE\Team Tools\DiagnosticsHub\Collector\VSDiagnostics.exe start <新的GUID> /launch:E:\ecl-scratch\phase4-20261028\build\Release\phase4_bench.exe "/launchArgs:--meta <诊断meta> --impl packed --threads 4 --pin 1 --cpus 0,2,4,6,8,10,12,14 --require-ac 1" "/loadConfig:F:\VS\IDE\Team Tools\DiagnosticsHub\Collector\AgentConfigs\CpuUsageHigh.json"
VSDiagnostics.exe stop <同一GUID> /output:<case>.diagsession
```

   每个case独立会话，正向/queue/counters/stats至少各一份，记录实际命令与返回码。开始前保存电源方案，接电后高性能，try/finally恢复并读回原方案；不要与正式矩阵并行。实际命令尚未运行成功，先用一份验证流程，再扩展其余。
3. 打开Visual Studio CPU视图，或`expandDiagSession`提取ETL后用本机VS PrivateAssemblies中的Microsoft.Diagnostics.Tracing.TraceEvent.dll读取。DLL版本3.1.9，TraceLog.OpenOrConvert(string,TraceLogOptions)、TraceCodeAddresses.LookupSymbolsForModule(SymbolReader,TraceModuleFile)已用反射确认；实际解析尚未实施。SymbolReader与该DLL配套，不下载替换现有程序集。
4. 使用本次PDB解析engine函数，只导出本项目目标进程的样本/热点。采样覆盖全进程准备、计时与验证，不能称其百分比是纯调度路径计时占比；没有单独时窗标记时应明确边界。确认样本数量/符号解析，不把空trace或未解析地址说成热点结论。
5. 原始.diagsession/ETL可能包含系统其他进程信息，不提交public仓库；保存在两机ECL_DATA或scratch，仓库只留目标进程导出表、SHA256、命令、工具版本、方法和结论。然后更新S8九项报告、AGENTS/status/progress、WorkPlan总览及既有飞书开发记录，最后才能标记阶段4完成。

官方使用方式：[Visual Studio命令行采样](https://learn.microsoft.com/en-us/visualstudio/profiling/profile-apps-from-command-line?view=visualstudio)。CPUUsageHigh配置实测4000 samples/second，是请求采样率，不能当作实际采样数量。[TraceEvent官方指南](https://github.com/microsoft/perfview/blob/main/documentation/TraceEvent/TraceEventProgrammersGuide.md)用于后续解析。阶段5正式实验前的swap检查点仍不能越过。
