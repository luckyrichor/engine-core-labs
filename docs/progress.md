# 进度记录

最后更新：2026-10-01（北京时间）

本文件是 `engine-core-labs` 的进度事实源，汇总到 `workplan-docs/进度总览.md`。

格式：每条记录写明日期、做了什么、验证方式与结果、遇到的问题。**不写计划，只写已发生的事**；失败和返工也要记，那是面试时最有料的部分。

---

_尚无记录。_

## 2026-10-01 W2 维持（Codex）

来源 engine-core-labs@65305d9 + 未提交修改，机器 tx。新增 CMake 脚手架与互斥 deque 的 owner push/pop；并发生产者测试验证任务不丢。首次构建因 namespace labs 与 C 标准库 labs() 撞名失败，改名 engine_labs 后重新构建与测试（结果见实际复验）。核心 steal() 保持用户 TODO，未实现、未做吞吐实验、无曲线。此项只计 W2，W3 测量负载和 W4 worker pool 另记。未提交/推送，未记录虚构工时。

W2 复验：2026-10-01 tx，改名后 CMake 构建成功，ctest owner_queue 1/1 通过；首次失败已保留记录。

## 2026-10-01 W3 维持（Codex）

来源 engine-core-labs@65305d9 + 未提交修改，tx。本次在 W2 owner queue 后另加确定性 CPU 负载函数、负载契约测试、设备元数据采集脚本。CMake 构建通过，ctest **2/2**；device.py 实跑确认 VM-0-15-ubuntu，AMD EPYC 7K62（虚拟机分配 4 logical CPUs），Linux 6.8，g++ 13.3。尚无 steal 实现，未运行线程吞吐实验/产出曲线。本项只计 W3，与 W2 队列和 W4 pool 不重复。

## 2026-10-01 W4 / L1 自动部分完成，用户核心待办（Codex）

来源 engine-core-labs@65305d9 + 未提交修改，tx。新增 job_system.hpp、future/drain/异常处理测试、steal 用户契约测试、benchmark 与 CSV/SVG 测量框架及 docs/l1-learning.md。steal() 和 implemented=false 未改为实现。

Release CMake 构建通过；ctest **5 passed、1 skipped（user_steal_contract）**。baseline smoke 实跑输出 checksum；stealing benchmark 退出 **2**；measure.py 退出 **1**，报告 contract exit=77 且没有生成输出目录。未产出吞吐曲线、未完成 work stealing 实验，不将 skip 当通过。首轮 benchmark 因 -Werror=misleading-indentation 构建失败，拆开 for 与 wait 后复验通过。

待用户手写 steal()，翻 implemented 标记，重配 -DUSER_STEAL_IMPLEMENTED=ON，先通过核心测试再测曲线。设备/负载将在测量时实际采集；本次设备为 tx 的 4 vCPU VM，不能沿用其数据到 Windows。未提交/推送，不虚构工时。
# 2026-10-02 W6 维持（Codex）

来源 engine-core-labs@b8def96 + 本轮工作树修改。按用户授权临时实现 steal()，补逐任务 exactly-once 并发生产/消费契约，运行 Release ctest：5/5 passed。随后生成48次测量的CSV/SVG与设备/构建/源码哈希元数据，全部标 temporary-reference-removed-after-test。

测量后立即恢复 work_queue.hpp，确认与开工内容 git diff 为零。恢复后重新构建：5 passed、1 skipped，未实现核心的 benchmark 仍被拒绝。临时产物见 docs/temporary-core-validation.md；本轮维护完成，最终用户核心与L1验收仍待办。没有用临时曲线冒充用户算法完成。无实际工时声明，以下为历史记录。
