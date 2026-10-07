# engine-core-labs

最后更新：2026-10-02（北京时间）

游戏引擎**非渲染**通用模块的原理学习与小规模可复现实验：物理、资源流程、内存、CPU 调度。

**状态：L1基本完成；L2工程基本完成，用户Arena核心TODO、参考验证通过。** 2026-10-07 Codex补充：Release 7/7测试通过；L2分配延迟/碎片测量与用户待办见 [L2说明](experiments/l2-allocators/README.md)。用户手写替换与讲解复验仍待办。L1历史48次采样见 docs/measurements/2026-10-02-codex-reference/。

## 对应岗位

02 腾讯《灰境行者》游戏引擎开发（通用向）。岗位原文见 [workplan-docs](https://github.com/luckyrichor/workplan-docs)。

## 形态

这个方向考的是**原理能不能讲清楚**，不是有没有产品，所以形态是「讲解文档 + 可复现小实验」而非一个大工程。每个主题一份讲解 + 一个能跑能测出数据的实验：

| 主题 | 实验 | 测什么 |
|---|---|---|
| CPU 调度 | 极简 Job System（work stealing） | 线程数与吞吐的关系曲线 |
| 内存 | pool / stack / arena 三种分配器 | 分配延迟分布、碎片率 |
| 资源流程 | 资源加载流水线 | 同步/异步/预加载的帧时间尖峰对比 |

实验必须**可复现、有数据**：注明设备、负载和测量方法，不接受"感觉更快了"这种结论。

## 分工

本项目采用**偏学习**模式：脚手架（测量框架、benchmark、出图）由 Claude 提供，每个实验**最核心的算法留空由用户实现**。目的是让用户能在面试里讲清楚这几十行代码，而不是拥有一份读过的代码。

2026-10-02 用户要求先保留 Codex 的 `steal()` 参考实现，源代码已标注作者。用户以后手写替换并复验，参考测量不当作用户成绩。

## 技术栈

C++ / CMake。比 `tactical-shooter-ue` 更基础，不依赖任何引擎。


执行说明见 experiments/l1-job-system/README.md、docs/l1-learning.md 与 docs/progress.md。`bash scripts/bootstrap.sh` 重建并测试；当前包含 stealing 契约与 benchmark 验证，没有 skipped 测试。
