# S8 tx/Linux正式测量

2026-10-25 · Codex。**仅tx完成，不代表阶段4双机验收通过；Windows离线待补。** `STATUS.json`的complete只表示本目录tx测量完成，power_plan_restored=false在Linux表示未执行Windows电源方案操作。

- 五组：positive（最坏伪共享）、cold（冷字段反向控制）、queue（SC Chase-Lev头尾）、counters（原JobSystem三个字段分别填充）、stats（阶段3执行器统计块）。
- 1/2/4/8 workers，布局×绑核开关，预热1轮+正式10轮，轮内顺序轮转。两套独立packed A/A（未逐worker绑核/固定workers）。
- 1360正式进程、9520指标行，invalid=0、stub=0，每配置10有效轮。8线程为四核tx上的超订。
- 源码SHA256 `fd1e81c30451b94e58e8a80e3e8e17513ae569522c3a4ed0fdcc98f757989c24`，各组device.json含逐文件清单、编译器、数据校验和及测量时git=8be95f4+dirty；后续提交不改写历史来源。
- 正式采集为systemd用户单元ecl-phase4-measure，期间未运行构建或其他实验；没有关闭swap/修改全局perf或ASLR。进程快照保留，不能声称隔离了共享VM。

每组保留raw.csv、summary.csv、summary-pinned.csv、config.json、device.json、executions.jsonl。summary.csv只将未绑核布局和packed-pinned与未绑核packed比较；summary-pinned.csv只解释固定worker后布局比值。组合变化不作单因素结论。cpu_migrations仅Linux软件事件，含生产者/main和ack窗口，不是缓存事件。

[审计](audit.json) · [全配置比值](comparisons.csv) · [结果表](RESULTS.md) · [曲线](s8-layout.svg) · [布局审计](layout-audit.json) · [九项故事](../../stories/S8.md) · [完整方法](../../stories/phase4-method.md)。构造描述在workloads/phase4，由`python3 -m bench.phase4 prepare`生成meta；重跑命令见方法。Windows预期需在同源快照上另建独立结果目录，未实施。
