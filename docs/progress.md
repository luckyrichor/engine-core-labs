# 进度记录

最后更新：2026-10-23（北京时间）

本文件是 `engine-core-labs` 的进度事实源，汇总到 `workplan-docs/进度总览.md`。

格式：每条记录写明日期、做了什么、验证方式与结果、遇到的问题。**不写计划，只写已发生的事**；失败和返工也要记，那是面试时最有料的部分。

---

## 2026-10-24 luowindows 授予 SeLockMemoryPrivilege（Claude Code）

所有者 2026-10-24 同意后，经 SSH 执行 `lock-memory-privilege.ps1 -Action Grant`，`LUOWINDOWS\shnh2` 已拥有 SeLockMemoryPrivilege。新 SSH 会话 `whoami /priv` 可见该权限（默认未启用）；程序内 AdjustTokenPrivileges 启用成功。用 `tools/windows-probes/large_page_probe.c` 实测 MEM_LARGE_PAGES：2/4/8/16/32/64 MiB 均成功，128 MiB 失败，错误 1450（ERROR_NO_SYSTEM_RESOURCES），当时空闲物理内存 4.42 GB。推断是物理内存碎片导致凑不出连续的 2 MiB 页，尚未验证。S5 Windows 大页组：每轮记录空闲物理内存，申请失败的轮次单独列出、不计入汇总，区域大小先限制在 64 MiB 以内。撤销用 `-Action Revoke`。

## 2026-10-24 第 1 阶段检查点处理（Claude Code）

所有者决定：google/tcmalloc 尽量修好；glTF 受限许可模型个人非商业用途全部可用；方案数字以实测为准；luowindows 连不上时告知所有者开机。

google/tcmalloc 全量 CMake 构建失败的原因查明：只坏在测试 huge_page_aware_allocator_test，gcc 13 把 abseil insert(T&&) 替换出的 `PageId&*` 当硬错误。改为只构建 tcmalloc_tcmalloc 与 hello_main，exit 0，hello_main 输出的 heap size 来自 tcmalloc 自身统计，确认生效，纳入 S4（仅 tx，静态链接）。Tracy v0.14.1 许可为 BSD-3-Clause。

新增 scripts/windows/lock-memory-privilege.ps1（调用 LSA API 授予或撤销 SeLockMemoryPrivilege，家庭版无 secpol.msc）。第一次 Status 失败（SSH 会话里 USERDOMAIN 与账号不符，IdentityNotMappedException），改为默认取当前身份 SID 后通过；当前未授予，等所有者决定，未改动 Windows 设置。

## 2026-10-23 面试级方案第 1 阶段：核验（Claude Code）

来源 engine-core-labs@c7ca348，与方案附录 A 基线一致；tx 上 Release ctest 11/11。方案原文存入 docs/，逐项结果见 docs/phase1-verification.md。原始数据与库源码放仓库外 `~/data/engine-core-labs`；仓库只保存下载脚本、校验和与统计。

Bistro 用 `scripts/data/fetch_bistro.sh` 完整下载（每次重新解析签名地址，断点续传），894,377,473 字节，ZIP CRC 通过，SHA256 已写进脚本。实际速率约 2 MB/s，不是方案记录的 45 KB/s，6.5 分钟下完。ufbx v0.23.1 与 Assimp v6.0.5（只开 FBX 导入器）都能解析三个 FBX，三角形数两库一致；Interior 与 Exterior 与页面一致，Interior_Wine 实测 1,320,323，页面写 1,293,691，以文件为准。选定 ufbx：速度约快一倍，内存约一半。

glTF Sample Assets@8f49239 浅克隆：不使用 LFS，150 模型 / 2482 文件 / 1.43 GB，逐模型许可全部有标注，其中 9 个受限。页缓存清理确认生效（Cached 4.08 GiB→0.92 GiB；冷读 0.487 s，热读 0.035 s）。fio 3.36 已安装，strace 可用，判断不需要 clang。外部库按最新标签固定到提交号（third_party/versions.md），两台机器都用源码构建。

google/tcmalloc 没有版本标签：Bazel 9.3.1 因 aspect_bazel_lib 不兼容失败，Bazel 8.4.2 因 protobuf/abseil 兼容级别冲突失败，两次都在构建前就失败；实验性 CMake 能配置，在编译整套依赖，本条记录时尚未结束。

luowindows 第一次 SSH 超时（Tailscale rx 0），稍后恢复。性能核为逻辑线程 0–15（8 核，带超线程），能效核为 16–31。E 盘剩余 175.9 GB，大于 F 盘的 94.4 GB，已建 `E:\engine-core-labs-ue5-comparison`。CMake 采用 VS2022 自带的 3.31.6。用 MSVC 编译探测程序确认：`PageFaultCount` 能统计首次触碰的缺页（64 MiB 增加 16416）；大页因缺 SeLockMemoryPrivilege 失败（错误 1314）。电源计划当前为“平衡”，未改动。

第 1 阶段检查点：已停下等所有者决定 tcmalloc 取舍、受限许可模型的用法、Windows 大页权限。


## 2026-10-15 评审补齐：A/A、启动触页、资源安装延迟与缓冲复用（Codex）

来源 engine-core-labs@9134c1e 加本轮工作树；测量时源码和可执行文件 SHA256 见 2026-10-15-l3 / -noise / -cold 的 device.json，已逐文件核对一致。L1固定输入3/4工作者、双阶段、A/B同baseline交替，共80次，0.240–0.343秒，逐ID及独立串行求和通过；相同实现的配对比值也有明显波动。Linux guest-reported steal均0，不代表主线程无抢占。

L3新增3/4工作者、vector/Arena缓冲槽和sync/async A/A控制，8配置十轮顺序采样，共80次、7680帧、2560个逐资源延迟，全部校验和通过。新增需求到安装和首次受理到安装两种时钟，保留缺失累计；初始化线程和缓冲费用计入setup/startup。4 worker异步需求到安装p99中位数约10.2ms，缺失48资产×帧；预加载启动约17.1ms。3 workers帧p99中位数下降但max仍接近1.9ms，不能证明抢占因果。Arena4相对vector4的p99配对收益中位数约1.025，范围0.740–1.107，与A/A噪声重叠，默认vector保持。

L2原已有first_touch/prefaulted，补setup耗时、启动缺页及setup+使用总计，三模式十新进程共15360个触页样本。预触页初始化中位数约9.717ms，随后每块使用p50约0.250us；首次使用触页约18.585us，初始化约0.005ms。预触页只是迁移成本，不保证永久驻留或跨平台commit。

64KiB槽使用L2 Arena，8槽512KiB数据容量；L1任务持shared ownership，RAII归还，锁只覆盖领还，安装Asset不持scratch指针。测试扩展同槽容量耗尽/归还、双槽不重叠、32资源并发一致和异常后再用。Release11/11、TSan11/11（进程setarch -R）、ASan/UBSan/启用LeakSanitizer11/11（sudo测试进程）全通过。未改全局ASLR或调度参数，采样期间未运行编译/sanitizer。

同步修正文档：根README残留L3尚未实现改为现状，加入一句生命周期取舍；详细评审处置与不能下的结论见2026-10-15-l3/README.md。未声称缓冲复用稳定提速，也未把guest steal当线程级抢占轨迹。

## 2026-10-09 成本维度扫描与冷内存路径（Codex）

来源 engine-core-labs@bc9c6db + 本轮工作树修改，精确 SHA256 见 measurements/2026-10-09-scan/ 与 2026-10-09-cold/。确认原随机版只覆盖位置变化，增加重任务占比千分数与倍数参数，以及整数区间均匀成本；默认 measure.py 加入随机布局。scan.py 四线程预入队、六组两档成本（占比0.1%/1%/5%，倍数10/100）加两组均匀成本、十种子配对，共160次，计时0.244–1.684秒，逐ID与同种子串行校验和均通过。配对收益中位数1.025–1.126，多个配置有更慢样本，不作单调收益或生产分布结论。布局hash覆盖成本值，旧hash仅位置，不可跨版本当作相等判据。

新增Linux首次触页 / 预触页控制 / malloc存活集增长，每模式十个新进程、每轮512个64KiB块、逐页写且保持存活，共15360个单次观测。跨轮p50中位数约18.64us / 0.241us / 19.00us；minor faults每轮8195 / 3 / 8195，major为0。记录原始单次、p99、max与缺页；不把页故障叫磁盘IO，不推定每次malloc都发生堆扩容，不建立固定容量分配器冷路径排名。

删除无调用方reference_arena兼容别名，allocators.hpp显式包含algorithm；progress说明恢复到记录前，status合并为当前摘要。Release10/10、TSan10/10及新增uniform smoke、ASan/UBSan/启用泄漏检测10/10；first_touch和malloc_growth另跑sanitizer均通过。TSan仅进程setarch -R，泄漏检测仅sudo测试进程。试跑曾并行采样和设置过重uniform成本，全部丢弃；最终扫描、冷采样、编译与sanitizer顺序进行，源文件SHA256已核对。

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

## 2026-10-13 L3 资源加载（Codex）

来源 bee55fd + 本轮修改。真实热文件+合成CPU解码，复用L1工作线程；主线程有限提交、在途8上限、重复去重和失败传播。三模式十轮、2880帧，checksum全部一致。p99跨轮中位数sync6.498ms/async1.739ms/preload0.297ms；预加载startup17.679ms，异步缺失资源×帧数48；分别报告代价，不称真实渲染收益。Release/TSan/ASan/UBSan/启用泄漏检测11/11；初次编译发现JobSystem仅future<void>，采用Asset packaged_task封装，未改其已有接口。源码SHA256核对通过，数据见measurements/2026-10-13-l3/，不含GPU上传/冷磁盘。

## 2026-10-24 第 2 阶段 D：外部基线构建与适配（Codex 自检，待独立验收）

来源 engine-core-labs@432370a + 未提交修改。用户在本群要求读取全局记忆与聊天后继续；前序 A/B/C 已由 Claude 验收。D 此前因 SSH 连续三次关闭停下，本轮通过已有 CONNECT 代理按 SSH 获取固定版本，未修改系统网络配置。

补齐 scripts/third_party 的六库固定版本构建、用户目录 autotools；experiments/common 的调度/分配接口、各库实现、调度校验器、同源码分配回放、CTest 驱动；根 CMake 接入和 docs/baselines.md。build.sh all 最终成功，源码外 Release 构建 CTest 35/35、无跳过。五种调度在 1/2/4 线程执行 2000 任务并核对每个 ID 恰好一次；六种分配器验证 32 alloc/32 free、峰值存活 5221 字节、字节模式、零存活、实际分配归属；错误答案、损坏数据、释放不存在对象和遗漏最终释放必须失败。

修正过的失败：缺少 autoconf/m4 与 config.guess/config.sub；CMake UTF-8 版本表读取与 Threads 目标作用域；mimalloc 纯 C 配置和版本化头文件目录；Google Ownership 强枚举、静态依赖导出及 .inc/.def；jemalloc 源码外构建报告 missing_version（用已核实标签与 HEAD 显式设置版本）。构建目录一度触及历史已跟踪 build-ext，已恢复本轮生成的改动，最终产物移到 ECL_DATA/build-adapters。

本轮未做性能测量、无性能结论；Windows/MSVC 未验证；trace 回放尚为串行；Google 依赖第 1 阶段 CMake 缓存。完整日志在 ECL_DATA/logs-stage-d-all-final.log、logs-stage-d-external-final.log、logs-stage-d-ctest-final.log。按执行说明交回 D，待 Claude 独立验收后才继续第 3 阶段。未提交、未推送，未关闭 swap，未使用 sudo。交接状态同步到 AGENTS.md（依据用户全局交接要求）。

## 2026-10-24 后续由用户本人接手

Codex · 2026-10-24 22:33:12（北京时间）。用户在本群明确“后续部分由我来处理，跟 Claude 无关了”。本条取代此前等待 Claude 独立验收和继续派活的安排，后续由用户本人处理。D 保持 Codex 自检 35/35、独立验收未完成的事实；代码基线仍为 engine-core-labs@432370a + 未提交修改。未新增开发、测试、提交或推送；代码与既有验证产物保留。交接入口 AGENTS.md，复现命令 docs/baselines.md。

## 2026-10-24 第 2 阶段验收与收尾

Codex · 2026-10-24 22:37:41（北京时间）。用户明确“审核过了，没有问题”，授权提交并完成阶段 2 收尾；该结论取代此前 D 待复核状态，验收人为用户，Claude 不再参与。核对 D 的 16 个源文件与上次 35/35 通过的 SHA256 快照完全一致；本轮仅补交接和公开状态文档，没有新代码变更，未重复运行功能测试。README、AGENTS、status、design-decisions 与 baselines 同步完成状态和既有限制；A/B/C 保留原提交及作者记录。第 3 阶段尚未开始。
