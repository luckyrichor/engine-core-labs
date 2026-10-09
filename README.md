# engine-core-labs

最后更新：2026-10-09（北京时间）

游戏引擎非渲染通用模块的可复现实验，对应岗位 02。当前实现和边界见 [一页摘要](docs/status.md)。

| 实验 | 内容 | 测量 |
|---|---|---|
| L1 | mutex deque、工作窃取、线程池生命周期 | 均衡 / 集中负载，含提交 / 预入队，粗任务 / 微任务 |
| L2 | intrusive pool、LIFO stack、bump arena、变长空闲区示例 | malloc 对照、批量计时、对齐浪费、合并与碎片 |
| L3 | 资源加载流水线 | 尚未实现 |

按用户 2026-10-09 要求，核心算法均由 Codex 完成，无需用户补写 steal() 或 Arena。历史参考测量保留原来源，不改称用户手写成果。

```bash
bash scripts/bootstrap.sh
python3 experiments/l1-job-system/tools/measure.py --output .local/l1-coarse
python3 experiments/l1-job-system/tools/measure.py --iterations 1 --min-seconds 0 --output .local/l1-overhead
build/experiments/l2-allocators/allocator_bench > .local/l2.csv
build/experiments/l2-allocators/fragmentation_demo
```

详见 [L1 方法](experiments/l1-job-system/README.md)、[L2 方法](experiments/l2-allocators/README.md)、[设计取舍](docs/design-decisions.md)及[历史进度](docs/progress.md)。数据必须核对机器、参数与源码 SHA256；四核虚拟机的八线程结果不能外推到八核物理机。
