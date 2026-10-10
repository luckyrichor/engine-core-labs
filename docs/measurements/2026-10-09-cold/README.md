# 冷内存路径

来源 engine-core-labs@bc9c6db + 本轮修改；精确源文件 SHA256 见 device.json。Linux上每模式十个新进程，每轮512次、64KiB每块、全部保持存活、逐系统页触碰，共32MiB。最终采样在L1测量结束后单独重跑。

| 模式 | 跨轮p50中位数 ns | 跨轮p99中位数 ns | 每轮minor faults |
|---|---:|---:|---:|
| first_touch | 18642.75 | 38472.00 | ['8195'] |
| prefaulted | 241.00 | 591.00 | ['3'] |
| malloc_growth | 19000.75 | 42996.00 | ['8195'] |

所有轮次major faults均为0；每区域payload通过验证。minor faults含实际触页和计时/进程设施少量缺页，不代表磁盘读。此处p99是每轮512个单次观测的分位数，raw.csv保留每次和最大值；不作跨进程尾部保证。

first_touch为新匿名映射，prefaulted在计时前已触页；malloc_growth包含malloc调用和逐页写，观测到存活集增长，不推断每次发生sbrk/内部堆扩展。固定容量自定义分配器没有同构堆增长路径，构造初始化也会预触页，因此不把此实验作为四分配器排名。

复现：`python3 experiments/l2-allocators/tools/cold_measure.py --build build --output .local/cold`。
