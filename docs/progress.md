# 进度记录

## 2026-10-09 随机重任务布局对照（Codex）

来源 engine-core-labs@0483fc8 + 本轮工作树修改，精确SHA256见measurements/2026-10-09-random/。用户指出周期重任务仍有刻意队列相关性，新增heterogeneous_random；ID%workers是正常轮转，偏斜来自ID%128重任务周期。随机版无放回选79/10000个100倍任务，生成成本向量在计时外，同seed双策略双阶段共享；每轮种子20261009–20261018，记录布局hash、每队列重任务数与总迭代量。

同版本重测两布局，共320次/十轮，每次0.426–1.695秒，逐ID与按种子串行求和均通过。总迭代量891050000保持一致，同seed布局hash一致、十随机hash互异。四线程预入队配对收益中位数周期版2.224、随机版1.136；随机版范围0.997–1.199，1/10略慢；八线程随机版1.050，范围0.848–1.178，1/10更慢。结论限定到这批布局和四vCPU机器，不保证随机输入都获益。

新增布局契约测试：固定数量、工作守恒、种子复现、选定十seed散布、预算上限拒绝。Release9/9、TSan9/9及随机版smoke、ASan/UBSan/启用LeakSanitizer9/9。随机版SD包含输入和运行差异，不当同输入纯计时噪声。旧测量保留，新增按轮次配对结果与版本/机器/种子证据。

## 2026-10-09 后续评审：成本偏斜与尾部观测（Codex）

来源 engine-core-labs@2ea3b9a + 本轮工作树修改，精确SHA256见 measurements/2026-10-09-review/。新增任务数均分、79/10000重任务做100倍迭代的heterogeneous fixture；显式按ID%workers分配，避免预热轮转偏移影响分布。160次、十轮，逐ID和独立串行结果均通过；每轮0.431–1.699秒，四线程预入队stealing中位数22605对baseline10262，约2.2倍，只限定本分布与机器。

L2新增100行逐次计时（每行102400样本）、clock+write控制、40000个原始窗口最大值。全部payload与窗口汇总核对通过；aligned的control/Pool/Stack/Arena插桩p50/p99/p99.9跨轮中位数均30/41/50ns，不能解析纳秒增量差异，不能排名。control最大值53390ns，提示孤立停顿不能直接归因分配器。Arena一次reset不再除以256称单次成本，留空均摊字段并标未解析。

补旧微任务分析：balanced baseline端到端从1到8线程约1325815降至283080任务/秒，提交中位数6.015增至33.692ms；不单独归因唤醒。该组8.54秒、距粗任务结束18.83秒，loadavg保留历史活动，不可称后台争用量；CV并非每点超过20%。malloc实际多种小尺寸、每批持有256块再释放，纠正“单尺寸/逐次立即释放”描述；“均快于malloc”也仅为具体场景观察。

Release8/8、TSan8/8及heterogeneous smoke、ASan/UBSan/启用泄漏检测8/8；新tail通道在ASan/UBSan/LeakSanitizer下完成100行无诊断。最终源文件SHA256与测量记录一致，旧CSV保留原版本；完整六项处置见review-2026-10-09.md，数据索引见新目录README。

## 2026-10-09 L1 / L2 核心完成与测量修订（Codex）

来源 engine-core-labs@b8be686 + 本轮工作树修改，精确源文件 SHA256 见 measurements/2026-10-09/。用户授权助手完成全部算法，取消此前手写 steal / Arena 待办；已更新 AGENTS、README 与学习说明，旧分工保留为历史。

L1 保留互斥 owner LIFO / thief FIFO，重写 executor 为 worker 独立通知代数、归零通知 drain，移除每任务完成广播和 1ms 轮询；暂停屏障用于预入队阶段。复查时发现 pause 通知若先于 worker 记录代数可能被漏看，等待谓词加入 paused 条件后最终复验通过。增加双策略多生产者、4000 个 ID、20 轮暂停恢复及关闭后拒绝提交测试。

旧版 perf：b8be686、RelWithDebInfo / frame pointer、4 workers / 1000000 × 1000 / balanced，787 CPU 样本、66765 上下文切换。负载调用占 52.10%，kernel spin unlock 占 7.37%，多经 futex wake；只确认开销存在，不能把旧吞吐异常全部归因于广播。普通权限 perf 被 paranoid=4 拒绝，sudo 仅测自身进程；报告读取曾需 -f，未改系统配置。

新 L1 粗 / 微任务各 320 次，每配置 10 轮；同池完整预热，配置顺序轮转，含提交 / 预入队分开，逐 ID exactly-once 和单独串行校验和，记录每次 loadavg / CPU pressure。粗负载实际 10000 × 50000，每轮 0.241–1.031 秒。均衡基线 1/2/4/8 workers 中位数 10590 / 20935 / 37161 / 36958 任务/秒，4/8 的 CV 15.8% / 9.6%，不判四线程优于八线程。集中单队列四线程窃取约基线 3.6 倍。本机四核且有后台负载，结果不外推。

L2 实现 Arena 对齐 bump / reset；Pool 改为块内空闲链表，保留 live bitmap；Stack 记录表预留成本输出。4 分配器含 malloc × 2 模式 × 10 轮 = 80 行，每批 256、50 批预热，每轮至少 2000 批与 100ms。odd 尺寸实际触发 3009 字节对齐浪费；新 p99 是 batch 均摊分位数。相邻时钟中位数 30ns，摊薄后约 0.117ns/op。删除固定池外部碎片结论；VariableHeap 全空间填满后隔块释放，32768 空闲 / 最大 128，256 字节请求失败，合并后成功。单列 first-fit 示例，不当通用分配器性能结论。

最终 Release **8/8**、TSan **8/8**（进程级 setarch -R）、ASan/UBSan **8/8**、LeakSanitizer **8/8**（sudo 测试进程，detect_leaks=1）。TSan 首轮映射冲突，普通用户 LeakSanitizer 全部权限 / ptrace 失败；记录失败及最终解决方式，未改全局 ASLR 或 perf 设置。测量 / sanitizer 顺序执行；一页摘要见 status.md。未将助手代码标成用户手写。

## 2026-10-02 按用户要求保留 Codex 参考实现

来源 engine-core-labs@59c123b + 本次工作树修改。恢复 steal() 并标注 Codex 作者，用户稍后手写替换待办。CMake 默认启用已实现核心验证，新增 stealing smoke；Release 构建、6/6 ctest通过，无skip。48次重复测量、CSV/SVG和设备/源码哈希见 measurements/2026-10-02-codex-reference/，标签 codex-reference-user-exercise-pending，checksum一致。记录为 L1 基本完成、参考实现验证通过，未声称用户手写完成。此前删除临时实现的记录为历史事实，本次是用户改变要求后补回。

最后更新：2026-10-09（北京时间）

本文件是 `engine-core-labs` 的进度事实源，汇总到 `workplan-docs/进度总览.md`。

格式：每条记录写明日期、做了什么、验证方式与结果、遇到的问题。**不写计划，只写已发生的事**；失败和返工也要记，那是面试时最有料的部分。

---


## 2026-10-01 W2 维持（Codex）

来源 engine-core-labs@65305d9 + 未提交修改，机器 tx。新增 CMake 脚手架与互斥 deque 的 owner push/pop；并发生产者测试验证任务不丢。首次构建因 namespace labs 与 C 标准库 labs() 撞名失败，改名 engine_labs 后重新构建与测试（结果见实际复验）。核心 steal() 保持用户 TODO，未实现、未做吞吐实验、无曲线。此项只计 W2，W3 测量负载和 W4 worker pool 另记。未提交/推送，未记录虚构工时。

W2 复验：2026-10-01 tx，改名后 CMake 构建成功，ctest owner_queue 1/1 通过；首次失败已保留记录。

## 2026-10-01 W3 维持（Codex）

来源 engine-core-labs@65305d9 + 未提交修改，tx。本次在 W2 owner queue 后另加确定性 CPU 负载函数、负载契约测试、设备元数据采集脚本。CMake 构建通过，ctest **2/2**；device.py 实跑确认 VM-0-15-ubuntu，AMD EPYC 7K62（虚拟机分配 4 logical CPUs），Linux 6.8，g++ 13.3。尚无 steal 实现，未运行线程吞吐实验/产出曲线。本项只计 W3，与 W2 队列和 W4 pool 不重复。

## 2026-10-01 W4 / L1 自动部分完成，用户核心待办（Codex）

来源 engine-core-labs@65305d9 + 未提交修改，tx。新增 job_system.hpp、future/drain/异常处理测试、steal 用户契约测试、benchmark 与 CSV/SVG 测量框架及 docs/l1-learning.md。steal() 和 implemented=false 未改为实现。

Release CMake 构建通过；ctest **5 passed、1 skipped（user_steal_contract）**。baseline smoke 实跑输出 checksum；stealing benchmark 退出 **2**；measure.py 退出 **1**，报告 contract exit=77 且没有生成输出目录。未产出吞吐曲线、未完成 work stealing 实验，不将 skip 当通过。首轮 benchmark 因 -Werror=misleading-indentation 构建失败，拆开 for 与 wait 后复验通过。

待用户手写 steal()，翻 implemented 标记，重配 -DUSER_STEAL_IMPLEMENTED=ON，先通过核心测试再测曲线。设备/负载将在测量时实际采集；本次设备为 tx 的 4 vCPU VM，不能沿用其数据到 Windows。未提交/推送，不虚构工时。
## 2026-10-02 W6 维持（Codex）

来源 engine-core-labs@b8def96 + 本轮工作树修改。按用户授权临时实现 steal()，补逐任务 exactly-once 并发生产/消费契约，运行 Release ctest：5/5 passed。随后生成48次测量的CSV/SVG与设备/构建/源码哈希元数据，全部标 temporary-reference-removed-after-test。

测量后立即恢复 work_queue.hpp，确认与开工内容 git diff 为零。恢复后重新构建：5 passed、1 skipped，未实现核心的 benchmark 仍被拒绝。临时产物见 docs/temporary-core-validation.md；本轮维护完成，最终用户核心与L1验收仍待办。没有用临时曲线冒充用户算法完成。无实际工时声明，此前章节为历史记录。


## 2026-10-07 Codex：W8–W10实际执行

W8 L2工程基本完成：64KiB pool/stack、用户Arena核心TODO、独立CodexReferenceArena验证夹具。临时实现用于继续推进后，用户函数为空并显式抛USER_ARENA_TODO；参考不记用户手写完成。实际30轮7680次分配的p50/p95/p99、内部浪费及外部碎片CSV已记录。

W10维护：禁用缓冲区所有者拷贝/移动，补非法释放、耗尽、对齐、1000次回收等边界验证。Release 7/7 CTest；首次ASan/UBSan因ptrace导致LeakSanitizer全部失败，禁用leak检测后7/7通过；没有完成泄漏检测。用户arena手写、理解与替换复验继续待办。来源engine-core-labs@11c42ae + 本轮工作树，精确SHA256见测量元数据。
