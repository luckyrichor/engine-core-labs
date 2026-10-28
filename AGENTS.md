# AGENTS.md

最后更新：2026-10-28（北京时间）

本文件为在本仓库工作的编码 agent 提供指引（Claude Code 读 `CLAUDE.md`、Codex 读 `AGENTS.md`，两者都指向这里）。

## 这是什么

游戏引擎**非渲染**通用模块的原理学习与小规模可复现实验：物理、资源流程、内存、CPU 调度。

三个月求职计划六项目之一（原编号 ⑤），对应岗位 **02**（腾讯《灰境行者》游戏引擎开发，通用向）。总计划见 [workplan-docs](https://github.com/luckyrichor/workplan-docs)。

**当前状态：L1 / L2 / L3 核心均由 Codex 实现，已取消用户手写 TODO。当前验证、测量与边界见 docs/status.md；历史记录不代表当前分工。**

**2026-10-23 起按《engine-core-labs 面试级完善方案》推进；后续由用户直接安排、Codex 执行，Claude 不再参与。进度见下一节。**

## 面试级完善方案：当前进度（交接）

方案原文：[`docs/engine-core-labs 面试级完善方案.md`](docs/engine-core-labs%20面试级完善方案.md)（所有者 2026-10-18 定稿）。共 9 个阶段、8 条优化故事 S1–S8。**强制检查点**只有三处：第 1 阶段结束后；第 5 阶段正式实验前，以及每次需要关 swap 的实验前；第 9 阶段结束后。其余阶段做完直接进入下一阶段，只在 docs/progress.md 追加记录。

| 阶段 | 状态 |
|---|---|
| 1 核验 | **已完成**（2026-10-23/24），检查点已过，所有者决定见 docs/phase1-verification.md |
| 2 基础设施（负载生成器、外部基线适配层、结果格式、一键重跑） | A/B/C 已由 Claude 验收并提交；D 已由 Codex 实现并通过 tx 验证（35/35）；用户于 2026-10-24 审核通过，阶段 2 已完成 |
| 3 L1 故事（S1/S2/S3） | **已完成（2026-10-25）**：Release37/37，自研队列/执行器TSan通过；13组、5940次正式运行、41940指标行，均有效；三条九项故事与曲线齐全 |
| 4 S8 伪共享/亲和性 | **已完成（2026-10-28）**：双机2720次/17680行全有效；VS四组26927样本、匹配PDB热点分析完成；电源恢复，阶段5正式实验待swap决定 |
| 5–9 | 未开始 |

**第 4 阶段交接（已完成，2026-10-28）**
- 方法与重跑：`docs/stories/phase4-method.md`，`python3 -m bench.phase4 prepare/measure`，`bench.phase4_report`；二进制在`$ECL_DATA/build-phase4`，TSan在`build-phase4-tsan`。
- 新增`experiments/l1-job-system/phase4/`、`common/cache_cell.hpp`，原执行器/队列/统计池仅增加可选模板布局与启动前线程配置；默认布局保留，原Chase-Lev已对齐。
- 完整Release38/38、TSan定向4/4；正式tx数据`docs/measurements/2026-10-25-phase4-tx/`：5组、1360次进程、9520行指标，invalid=0/stub=0，各配置10有效轮，两套A/A；源码SHA256 fd1e81c30451b94e58e8a80e3e8e17513ae569522c3a4ed0fdcc98f757989c24。
- 正向微基准4T分行吞吐配对6.327倍（未逐worker绑核）/7.455倍（固定workers），均显著；单线程和冷字段无显著收益；实际队列/计数器/统计块与纯绑核各线程档均无显著吞吐差异，默认布局保留。九项报告`docs/stories/S8.md`明确为tx部分结果。
- Windows同源快照已准备在`$ECL_DATA/phase4/windows-source.zip`，内含逐文件指纹与真实来源`source-snapshot.json`；仅在tx编辑，Windows只编译/运行。接手前核对快照与当前源码指纹一致，若改代码要重新生成；不要把旧快照当当前HEAD。
- Windows于2026-10-28完成同源MSVC19.44.35228构建、S8契约CTest1/1与5组1360次/8160行正式矩阵，均有效、各指标10轮；数据`docs/measurements/2026-10-28-phase4-windows/`。Windows没有Linux迁移指标，不虚构为0。L1数据缓存行64，P核LP0–15/E核16–31，双方资格集合均为每个P核一个LP（0,2,…14）。接电、高性能设置与finally恢复原平衡方案已核对。
- Windows正向4T分行5.620/7.894倍，均显著；实际结构各档无显著布局收益。cold1T纯绑核0.896倍显著变慢；positive8T绑核A/A受长样本影响至335.326，不能把中位16.177倍当收益，保留全部数据。长样本原因未证实。
- VS CPU采样与热点分析已完成：四份目标进程exit0、26927样本、EventsLost均0、项目叶未解析均0。匹配原Release PDB，项目self/inclusive与模块分布、脚本、哈希见Windows结果profiling/README.md；全进程放大诊断不与正式CSV混用。原始trace在两机外部目录，不提交系统原始数据。
- 失联排查：08:57空闲触发Modern Standby，后续事件证实低功耗驻留，17:37电源键唤醒；未查到资源耗尽事件，当前CPU4%/可用3.22GiB，历史无连续资源采样，不能彻底排除其他因素。见Windows结果outage-analysis.md。采样时临时保活，finally释放，原平衡方案另行读回确认。没有修改网络/永久睡眠设置。
- 飞书已同步：2026-10-28以codex profile/用户身份追加开发记录revision11，来源engine-core-labs@014e87e；已回读26927样本、阶段4完成/待机排查并回查既有项目父节点。
- 飞书已同步：2026-10-28以codex profile/用户身份追加开发记录revision10，来源engine-core-labs@cc5bb71；已回读2720次/17680行、Windows结果与采样待补状态，并回查原项目节点归档。
- 飞书已同步：2026-10-25以codex profile/用户身份追加开发记录revision9，来源engine-core-labs@913de2d；已回读1360次/9520行与Windows待上线内容，并回查仍归档于既有项目节点。链接见下方飞书文档节。

**第 3 阶段交接（2026-10-25，Codex 独立执行）**
- 代码：`experiments/l1-job-system/phase3/` 含 SC 固定容量 Chase-Lev、共用 inbox 调度实验池、逐ID压力测试、核心之外的分块接口、几何成本采集器和统一实验入口；`job_system.hpp` 增加可选提交阶段插桩与默认关闭的条件通知，原默认策略保留。
- 数据：`docs/datasets/bistro-costs/` 冻结1186真实Bistro网格实例的成本中位数与直方图；16倍时间压缩、整数循环校准后重采样，明确为CPU代理，不是实际引擎吞吐。`workloads/phase3/` 有真实派生10k/100k/1m、均匀控制及8粒度描述。
- 验证：完整外部Release **37/37**、无跳过；TSan覆盖自研队列、四生产者、末元素竞争、回绕、条件唤醒与暂停恢复/关闭，外部库仅验证Release一致性。精确范围在结果目录的validation/README.md，不能称所有37项均跑过TSan。
- 结果：`docs/measurements/2026-10-25-phase3-tx/`，13组、5940次正式进程、41940指标行；invalid=0、stub=0，每个吞吐配置10个有效轮次，13组单一源码SHA256与当前实现一致。各组原始CSV、summary、device、配置和复现入口保留。
- 结论：S1 real100k/4T的Chase预入队1.162倍与A/A重叠，判无显著提升；端到端0.741倍显著变慢。S2六组吞吐/提交p99均无显著改善，条件通知默认仍关闭。S3显示小粒度分块可摊薄开销，但大块使任务数不足时收益消失；不声称通用临界ns。
- 完整九项故事在 `docs/stories/S1.md`、`S2.md`、`S3.md`；`phase3-method.md` 解释自研实验池与原JobSystem的区别、SC内存序、外部库计时边界、共享VM/A-A噪声与估计量局限。
- 一键入口：`python3 -m bench.phase3 prepare`；`python3 -m bench.phase3 measure --suite all --output docs/measurements/<新目录>-phase3-tx`；`python3 -m bench.phase3_report <目录>`，图表加 `--plots` 需matplotlib，本轮3.11.2。构建目录 `$ECL_DATA/build-phase3`，TSan `$ECL_DATA/build-phase3-tsan`。
- 没有关swap或修改perf/ASLR全局设置；profiling只对测试进程使用sudo，TSan只对测试进程setarch -R。Windows/UE未验证，按方案阶段8补测。阶段4尚未启动，后台测量任务已结束。

**第 2 阶段交接（2026-10-24，Codex）**
- A/B/C 提交分别为 `ef9ded8` / `a452624` / `432370a`；实现由 Codex 完成，Claude 独立验收，证据在 `.agent-tasks/2026-10-24-第2阶段基础设施.md`。旧表“未开始”已过时。
- D 在 `engine-core-labs@432370a` 基线上实现；续做修复 SSH 获取路径，补齐 `scripts/third_party/`、`experiments/common/{scheduler_adapter.hpp,alloc_adapter.hpp,adapters/}`、根 CMake 与 `docs/baselines.md`。
- 六库均已按固定提交成功执行 `bash scripts/third_party/build.sh all`。tx 外部 Release 构建 CTest **35/35 通过，无跳过**；默认构建 **25 通过 / 6 明确跳过**；五种调度 × 1/2/4 线程，六种分配器（含 glibc、本地变长堆）通过相同负载独立答案、实际分配器归属及损坏输入拒绝校验。Windows/MSVC 未运行，未作性能测量，尚无性能结论。
- 源码与构建步骤见 [docs/baselines.md](docs/baselines.md)。最终外部构建目录 `$ECL_DATA/build-adapters`；完整日志 `$ECL_DATA/logs-stage-d-{all-final,external-final,ctest-final}.log`。旧 `build-ext/` 已有跟踪产物不在本轮清理范围，避免再次向它写构建结果。
- GitHub 直连 SSH 因 fake-IP 地址而关闭；本轮单进程 `GIT_SSH_COMMAND` 通过现有 `127.0.0.1:7890` CONNECT 代理成功，不改 SSH/网络配置。见 baselines.md 的可复制命令。
- Ubuntu 缺少 autotools 时，脚本将固定版本 Debian 工具包仅解到 `$ECL_DATA/build-tools/`，无 sudo/系统安装。jemalloc 显式使用已核实的标签/提交生成版本字符串；Google 静态链接导出包含传递依赖与 .inc/.def 头文件。
- 分配回放目前按时间戳串行执行，仅核对 trace 的线程标签；多线程回放留给 S4。Google 构建复用第 1 阶段 CMake 缓存，空机器完整重建仍未支持。
- **用户验收（2026-10-24）**：用户明确“审核过了，没有问题”，并授权提交和完成阶段 2 收尾。阶段 2 已验收完成；D 由 Codex 实现，验收人为用户，不记作 Claude 验收。Claude 不再参与后续；阶段 2 收尾已提交 2c29cd2；2026-10-25 用户进一步授权 Codex 开始阶段 3。

**第 1 阶段已验证的事实**（详见 docs/phase1-verification.md）
- 基线提交 c7ca348 与方案一致；Release ctest 11/11。
- Bistro_v5_2.zip 已下载校验（SHA256 写在 `scripts/data/fetch_bistro.sh`）；FBX 用 ufbx v0.23.1 解析。
- glTF Sample Assets@8f49239，150 个模型全部可用（个人非商业用途，不再分发）。
- 外部库版本固定在 [`third_party/versions.md`](third_party/versions.md)，两台机器都从源码构建。google/tcmalloc 只能构建 `tcmalloc_tcmalloc` 目标（全量构建的一个测试在 gcc 13 下报错）。
- luowindows：性能核为逻辑线程 0–15，能效核 16–31；CMake 用 VS2022 自带的 3.31.6；UE 对照目录 `E:\engine-core-labs-ue5-comparison`；Windows 缺页用 `GetProcessMemoryInfo().PageFaultCount`。

**数据与第三方源码的位置（不进仓库）**：`$ECL_DATA`，默认 `~/data/engine-core-labs`（tx）。其下有 `bistro/`（zip、SHA256SUMS、manifest、extracted/）、`gltf-sample-assets/`、`third_party/`（ufbx、assimp、tcmalloc-src、tcmalloc-cmake、bazelisk）、`inventory/`、`logs/`。Windows 临时目录 `E:\ecl-scratch`。

**下一步（按顺序）**
1. 阶段4已完成。阶段5正式实验前等待用户决定tx临时关闭swap时机；不得自行关闭或先跑正式内存实验。阶段5实现/预检应按既有方案，不重跑阶段4矩阵。
2. 后续故事仍须每条九项齐全，按 A 节规则重复、配对、测 A/A；Windows 补测与各阶段明确限制继续如实记录。

**待所有者决定**
- tx swap：第 5 阶段正式实验前，由所有者决定何时临时关闭，实施者不得自行关闭。

**已知坑**
- luowindows 已授予 SeLockMemoryPrivilege（2026-10-24）。程序须先 AdjustTokenPrivileges 启用；64 MiB 以内的大页可申请，128 MiB 在空闲内存 4.4 GB 时失败（错误 1450）。
- 后台长任务要用 `systemd-run --user`（tx 已开 Linger），否则飞书 bridge 一轮结束时会连同子进程一起回收。
- luowindows 偶尔关机属正常；SSH 连不上时告知所有者开机，不要改网络配置。
- PowerShell 脚本保持纯 ASCII（PS 5.1 读非 ASCII 的 .ps1 需要 BOM）；经 SSH 传长命令用 `-EncodedCommand`（UTF-16LE base64）。
- SSH 会话里 `$env:USERDOMAIN` 不一定与账号对应，取当前身份用 `WindowsIdentity::GetCurrent()`。
- tx 上 perf 硬件计数器不可信，只用软件事件。

**飞书文档**（我的文档库 › WorkPlan › engine-core-labs：线程调度、内存分配与资源加载实验）
- 项目节点（Codex 2026-10-13 写的旧介绍，不改）：https://kcn0pbn0hwo0.feishu.cn/wiki/GdU8webePivmECkXi1icCqgCnsh
- engine-core-labs 项目概览（2026-10-24）：https://kcn0pbn0hwo0.feishu.cn/wiki/EktVwBcJBiEk0qkfrhLckJoWnjf
- engine-core-labs 开发记录（每次同步在文末追加一节）：https://kcn0pbn0hwo0.feishu.cn/wiki/IJ3swdgKAigHlUkY1WTcspQ0nVB

## 分工模式：实现与原理说明

用户 2026-10-09 明确要求所有算法由助手完成，覆盖此前“用户手写核心”和“仅保留参考实现”的约定。不要重新留下 steal 或 Arena 占位。代码与测量由助手完成，说明算法、适用生命周期、同步方式和测量限制；作者来源保留为 Codex，不改称用户手写。项目仍是可复现实验，理解设计取舍是面试准备的一部分。

## 实验清单

| 主题 | 实验 | 测什么 |
|---|---|---|
| CPU 调度 | 极简 Job System（work stealing） | 线程数与吞吐的关系曲线 |
| 内存 | pool / stack / arena 三种分配器 | 批量分配 / 回收、malloc 对照、内部浪费与变长碎片 |
| 资源流程 | 资源加载流水线 | 同步／异步／预加载的帧时间尖峰对比 |

**实验必须可复现、有数据**：注明设备、负载和测量方法。不接受「感觉更快了」这种结论。

## 技术栈与环境

C++ / CMake，不依赖任何引擎，比 `tactical-shooter-ue` 更基础。

**Mac 上没装 cmake**；tx 服务器有 gcc13 + cmake3.28，可以直接在那边做，省得污染本机。

## 所有项目共同的约束

这些是用户明确要求的，优先级高于一般工程习惯：

- **区分事实与推断**：JD 原文明确写的、与「行业常见要求／补充推断」必须显式区分。允许联网补充，但要标注来源性质。
- **不夸大**：不把规划写成已完成成果，不把本地测试数据包装成生产规模，不把加分项改写成硬性门槛。
- **不预设降级**：即使用户当下没时间反馈，也按既定方向持续推进产出，不因「时间可能不够」提前砍掉或缩水——取舍由用户自己做。
- **不安排模型算法、训练、微调、推理引擎优化方向的学习。**
- 不需要重新确认用户背景：Python 最熟练，C++/C#/Go 有基础，Agent 开发已较熟悉。直接进入框架层和有深度的工程问题，不安排语言零基础或入门教程。
- **AI 产出的代码不等于可以写进简历。** 用户要能在面试里讲清每个关键设计取舍，偏产出的项目要同步维护 `docs/design-decisions.md`。

## 多机协作

三台机器：Mac、`tx`（Ubuntu 服务器，常开）、`luowindows`（Windows，引擎唯一机器）。GitHub 是唯一事实源。

- **开工 `git pull`，收工 `git push`**，不留未推送的提交过夜
- **同一个项目同一时间只在一台机器上改**
- 密钥永不进仓库，放仓库内已 gitignore 的 `.local/`

**动环境之前先读 [`workplan-docs/环境与踩坑记录.md`](https://github.com/luckyrichor/workplan-docs/blob/main/环境与踩坑记录.md)** —— 三台的规格、三种不同的代理机制、以及十条已经踩过的坑都在那里。其中两条最容易再犯：三台文件系统大小写敏感性不一致（tx 敏感，另外两台不敏感）；PowerShell 5.1 的 BOM 规则（读 `.ps1` 必须带 BOM，写文件绝不能带）。

## 进度记录

进展写在 `docs/progress.md`：**只写已发生的事，不写计划**；失败和返工也要记，那是面试时最有料的部分。汇总到 `workplan-docs/进度总览.md`。

## Docker 运行位置（用户明确要求）

**需要 Docker 时默认跑在 `tx` 服务器上**，不要在 Mac 或 Windows 上起容器。

确有必要在本地跑时，**必须先征得用户同意**，不要自行决定。

tx 上 `ubuntu` **已加入 `docker` 组**（2026-09-19），docker 命令不需要 `sudo`，Testcontainers 也可直接用。该机内存只有 7.5G，多个服务同时跑之前先看 `free -h`。

### 在 tx 上跑 Docker／测试前，必须先同步代码

本地改完直接去 tx 跑，而 tx 上还是旧代码 —— 结果是「测试通过」测的其实是旧版本，**这种假通过比失败更危险**。

用 `workplan-docs/scripts/run-on-tx.sh` 代替手工 ssh，它强制执行正确顺序：

```bash
bash workplan-docs/scripts/run-on-tx.sh agent-memory 'uv run pytest -q'
bash workplan-docs/scripts/run-on-tx.sh agent-memory 'docker compose up -d postgres'
```

它会：本地有未提交改动就拒绝 → push → tx pull → **校验两边哈希一致** → 才执行。哈希比对是最终判据，不是「命令有没有报错」。脚本还顺带剥掉了腾讯云登录横幅。

## 记录规范：新内容写到哪儿

**本仓库的正文只维护 `AGENTS.md` 一份**，`CLAUDE.md` 永远只是几行指路。两份都写正文必然漂移，**不要往 `CLAUDE.md` 里加任何内容**。

| 内容类型 | 去处 |
|---|---|
| 本仓库的指引、约定、边界、分工方式 | 本仓库 `AGENTS.md` |
| 环境、机器、工具链、跨平台的坑 | `workplan-docs/环境与踩坑记录.md` |
| 已经发生的进展、验证结果、失败与返工 | 本仓库 `docs/progress.md` |
| 关键设计取舍及其理由（面试要讲的） | 本仓库 `docs/design-decisions.md` |
| 排期、工时预算、里程碑 | `workplan-docs/总节奏表.md` |
| 岗位要求本身 | `workplan-docs/岗位要求原文.md`（唯一事实来源，不要在别处改写） |

写进「环境与踩坑记录」时，用 **［实测］**／**［预警］** 标注区分「已验证的事实」和「未触发的已知风险」，不要把推断写成结论。

## 更新日志

- 2026-10-28 Codex：完成四组VS CPU采样/匹配PDB热点分析，26927样本、丢失事件0；排查到空闲待机证据，保留历史资源占用未知与长样本归因限制；恢复电源并完成阶段4。

- 2026-10-28 Codex：完成Windows MSVC/S8契约1/1、5组1360次/8160行矩阵；同源双机结果与电源恢复核对；VS采样前连接再次中断，阶段4仍进行中，交接采样步骤。

- 2026-10-25 Codex：阶段4已完成tx实现/验证与5组1360次正式测量；正向微基准有效、实际结构无显著收益，Windows离线待补，阶段4保持进行中。

- 2026-10-25 Codex：完成阶段3 S1/S2/S3；SC Chase-Lev、条件通知候选、分块接口和真实成本采集；Release37/37、TSan自研路径；13组5940次正式运行与三条九项报告，保留负面结果，未进入阶段4。

- 2026-10-24 Codex：用户审核通过阶段 2，授权提交推送；收尾状态同步到 AGENTS、README、status、design-decisions、progress 与飞书开发记录，保留 Windows/串行回放/Google 缓存依赖等已知限制。

- 2026-10-24 Codex：按用户最新决定将后续责任改为用户本人接手，撤销 Claude 的验收与派活依赖；保留代码、现有自检结果和历史作者记录，未启动新开发。

- 2026-10-24 Codex：读取全局记忆与本群完整记录后续做 D；修复源码获取、补齐外部库与适配校验，外部 Release 35/35 通过；A/B/C 状态依据已验收提交回填。D 尚待 Claude 独立验收，未提交推送。

- 2026-10-24 Claude Code：luowindows 授予锁定内存页权限并实测大页；飞书建项目概览与开发记录；第 1 阶段检查点处理（tcmalloc 修复、许可决定、Windows 大页脚本）；新增“面试级完善方案：当前进度”交接节。
- 2026-10-23 Claude Code：第 1 阶段核验完成。
- 2026-10-09：分工改为算法全部由助手完成。
