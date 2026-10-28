# engine-core-labs


当前进展（2026-10-28）：阶段3已完成；阶段4 S8的tx/Windows双机矩阵2720次测量完成，Windows MSVC与S8契约通过，原电源方案已恢复；VS CPU采样前连接再次中断，**阶段4尚未完成**。见[当前状态](docs/status.md)、[S8报告](docs/stories/S8.md)。
最后更新：2026-10-25（北京时间）

游戏引擎非渲染通用模块的可复现实验，对应岗位 02。当前实现和边界见 [一页摘要](docs/status.md)。

| 实验 | 内容 | 测量 |
|---|---|---|
| L1 | mutex deque、工作窃取、线程池生命周期 | 均衡 / 集中 / 成本偏斜 / 随机成本，含提交 / 预入队，粗任务 / 微任务 |
| L2 | intrusive pool、LIFO stack、bump arena、变长空闲区示例 | malloc 对照、批量计时、对齐浪费、合并与碎片 |
| L3 | 真实热文件读取、合成解码、主线程安装，复用 L1 / L2 | 同步 / 异步 / 预加载、3 / 4 工作者、缓冲复用、请求延迟、A/A 噪声 |

按用户 2026-10-09 要求，核心算法均由 Codex 完成，无需用户补写 steal() 或 Arena。历史参考测量保留原来源，不改称用户手写成果。

```bash
bash scripts/bootstrap.sh
python3 experiments/l1-job-system/tools/measure.py --output .local/l1-coarse
python3 experiments/l1-job-system/tools/measure.py --iterations 1 --min-seconds 0 --output .local/l1-overhead
build/experiments/l2-allocators/allocator_bench > .local/l2.csv
build/experiments/l2-allocators/fragmentation_demo
```

详见 [L1 方法](experiments/l1-job-system/README.md)、[L2 方法](experiments/l2-allocators/README.md)、[设计取舍](docs/design-decisions.md)及[历史进度](docs/progress.md)。数据必须核对机器、参数与源码 SHA256；四核虚拟机的八线程结果不能外推到八核物理机。

## 面试级完善方案进度

第 1 阶段核验、第 2 阶段基础设施已完成；阶段 2 于 2026-10-24 通过用户审核。
新增统一结果格式/判定/运行器、五类确定性负载生成器、Bistro AABB 预处理与独立答案、六个固定版本外部库及调度/分配适配层。
完整外部 Release 构建 CTest 35/35 通过；默认构建 25 通过、6 项外部库明确跳过。
第2阶段只验证功能与一致性，没有性能结论。Windows/MSVC 未验证；trace 回放目前串行；Google tcmalloc 依赖第 1 阶段已有 CMake 缓存。

入口：[结果与运行器](docs/bench-infrastructure.md)、[负载](docs/workloads.md)、[Bistro 预处理](docs/bistro-aabb.md)、[外部基线与复现](docs/baselines.md)。第 3 阶段 S1/S2/S3 已完成：Release37/37、TSan自研路径通过，13组5940次正式运行；[三条故事与数据](docs/measurements/2026-10-25-phase3-tx/README.md)、[S3曲线](docs/measurements/2026-10-25-phase3-tx/s3-speedup.svg)。S1部分策略显著变慢、S2无显著改善，未将候选通知设为默认；S3分块收益有任务数量边界。第4阶段尚未开始，当前交接见 [AGENTS.md](AGENTS.md)。

## L3 资源加载

**异步把代价从帧时间转成资源暂时缺失，预加载把它转到启动时间。** 这是本实验的生命周期取舍，不保证异步消除所有帧尖峰。

[资源流水线与三种帧时间对照](experiments/l3-resource-loading/README.md)已实现；原计划L1–L3均有工程验收证据，实验边界见docs/status.md。

2026-10-15 新测量与评审逐项处置见 [L3 对照结果](docs/measurements/2026-10-15-l3/README.md)。
