# S8 Windows正式矩阵与CPU采样

2026-10-28（北京时间）· Codex。luowindows / i9-13900HX，MSVC 19.44.35228、CMake3.31.6、Python3.14.3，Release C++20。独立同源快照构建，tx为唯一编辑端。Windows主矩阵已完成；Visual Studio四组CPU采样及匹配PDB热点分析已完成，阶段4完成，证据见[profiling](profiling/README.md)。失联排查见[outage-analysis](outage-analysis.md)。

## 数据与判定

五组positive/cold/queue/counters/stats；1/2/4/8workers、layout×binding、预热1轮+正式10轮和两套独立A/A。1360正式进程、8160指标行，invalid=0/stub=0，每个指标配置10个有效轮次。Windows不采Linux cpu-migrations，不填虚构的0；相较tx每进程少这一项软件事件。

两种模式均限制在每个物理P核一个逻辑CPU：0,2,4,6,8,10,12,14。pinned额外逐worker固定并读取掩码；unbound允许在同一P核资格集合内迁移，不混入E核/SMT差异。API实测P核LP0–15、E核16–31，L1数据缓存行64字节。所用8workers未超出8个物理P核，不是tx8workers超订的同一拓扑情形。

[审计](audit.json) · [全部比值](comparisons.csv) · [结果表](RESULTS.md) · [曲线](s8-layout.svg) · [布局](layout-audit.json)。summary.csv的单因素结果是未绑核的布局变化及packed-pinned的纯绑核变化；summary-pinned.csv只用于固定worker后布局比较。全部阈值复用bench.verdict，不从中位数单独宣称提升。

4T正向分行配对5.620倍（未逐worker固定）/7.894倍（固定worker），均显著；实际结构的各线程档均无显著布局收益。cold1T纯绑核比值0.896、与A/A不重叠且超过10%门限，判显著变慢，如实保留。positive8T固定worker组中出现8.358秒和52.870秒的长样本，A/A范围[0.853,335.326]，分行比值范围[0.016,5429.234]；无显著差异，不删除轮次、不把其中位16.177倍当优化成果。长样本原因未证实；同期SSH/Tailscale间歇超时不能证明是休眠或其因果。

## 来源与验证

源码SHA256为`fd1e81c30451b94e58e8a80e3e8e17513ae569522c3a4ed0fdcc98f757989c24`，五组逐文件清单与tx矩阵及engine-core-labs@3701d67的实现一致。快照源自2026-10-25的8be95f4+dirty工作树，source-snapshot.json核对后才运行；device.git保留这一真实来源，未伪称Windows执行过git checkout。源码无需修改即MSVC编译通过。

Windows S8契约CTest1/1，覆盖五组12变体、1/4workers×绑核开关、逐ID恰好一次/独立摘要及非法CPU拒绝。不是Windows全仓库38项测试，不称Windows跑过TSan。validation保存配置、构建、测试和采集日志，以及实际CMakeCache。MSVC使用/O2 /Ob2 /DNDEBUG /Zi，链接/DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF，PDB用于CPU采样符号解析；调试符号不代表Debug优化级别。

接电后启用高性能方案，最终finally恢复并读回原平衡方案`381b4222-f694-41f0-9685-ff5bb260df2e`，original-power-plan.txt与restored-power-plan.txt完全相同，STATUS.json记录complete/power_plan_restored=true。未并行进行构建、采样或其他实验；常驻服务/系统调度噪声不声称隔离，A/A揭示波动。

## 复现

将同源快照复制到E:\ecl-scratch的独立目录，使用VS2022的CMake对experiments/l1-job-system/phase4做x64 Release构建；具体flags见validation/CMakeCache.txt。vcvars64环境下，从快照根执行：

```bat
set PYTHONUTF8=1
set ECL_DATA=E:\ecl-scratch\phase4-20261028\data
python -m bench.phase4 prepare
python -m bench.phase4 measure --binary E:\ecl-scratch\phase4-20261028\build\Release\phase4_bench.exe --output <新的结果目录> --no-perf
```

报告在tx上用`python -m bench.phase4_report <结果目录> --plots`生成，matplotlib版本保存在plot-version.json。完整方法见[phase4-method](../../stories/phase4-method.md)；CPU采样使用临时保活、独立诊断放大负载与全进程口径，未混入正式CSV，见profiling/README.md。
