# AGENTS.md

最后更新：2026-10-24（北京时间）

本文件为在本仓库工作的编码 agent 提供指引（Claude Code 读 `CLAUDE.md`、Codex 读 `AGENTS.md`，两者都指向这里）。

## 这是什么

游戏引擎**非渲染**通用模块的原理学习与小规模可复现实验：物理、资源流程、内存、CPU 调度。

三个月求职计划六项目之一（原编号 ⑤），对应岗位 **02**（腾讯《灰境行者》游戏引擎开发，通用向）。总计划见 [workplan-docs](https://github.com/luckyrichor/workplan-docs)。

**当前状态：L1 / L2 / L3 核心均由 Codex 实现，已取消用户手写 TODO。当前验证、测量与边界见 docs/status.md；历史记录不代表当前分工。**

**2026-10-23 起按《engine-core-labs 面试级完善方案》推进（Claude Code 主导），进度见下一节。**

## 面试级完善方案：当前进度（交接）

方案原文：[`docs/engine-core-labs 面试级完善方案.md`](docs/engine-core-labs%20面试级完善方案.md)（所有者 2026-10-18 定稿）。共 9 个阶段、8 条优化故事 S1–S8。**强制检查点**只有三处：第 1 阶段结束后；第 5 阶段正式实验前，以及每次需要关 swap 的实验前；第 9 阶段结束后。其余阶段做完直接进入下一阶段，只在 docs/progress.md 追加记录。

| 阶段 | 状态 |
|---|---|
| 1 核验 | **已完成**（2026-10-23/24），检查点已过，所有者决定见 docs/phase1-verification.md |
| 2 基础设施（负载生成器、外部基线适配层、结果格式、一键重跑） | 下一步，未开始 |
| 3–9 | 未开始 |

**第 1 阶段已验证的事实**（详见 docs/phase1-verification.md）
- 基线提交 c7ca348 与方案一致；Release ctest 11/11。
- Bistro_v5_2.zip 已下载校验（SHA256 写在 `scripts/data/fetch_bistro.sh`）；FBX 用 ufbx v0.23.1 解析。
- glTF Sample Assets@8f49239，150 个模型全部可用（个人非商业用途，不再分发）。
- 外部库版本固定在 [`third_party/versions.md`](third_party/versions.md)，两台机器都从源码构建。google/tcmalloc 只能构建 `tcmalloc_tcmalloc` 目标（全量构建的一个测试在 gcc 13 下报错）。
- luowindows：性能核为逻辑线程 0–15，能效核 16–31；CMake 用 VS2022 自带的 3.31.6；UE 对照目录 `E:\engine-core-labs-ue5-comparison`；Windows 缺页用 `GetProcessMemoryInfo().PageFaultCount`。

**数据与第三方源码的位置（不进仓库）**：`$ECL_DATA`，默认 `~/data/engine-core-labs`（tx）。其下有 `bistro/`（zip、SHA256SUMS、manifest、extracted/）、`gltf-sample-assets/`、`third_party/`（ufbx、assimp、tcmalloc-src、tcmalloc-cmake、bazelisk）、`inventory/`、`logs/`。Windows 临时目录 `E:\ecl-scratch`。

**下一步（按顺序）**
1. 第 2 阶段：按方案 D 节与附录 B，实现负载描述文件与生成器（同指纹字节级一致，带独立答案）、外部基线适配层（先接 oneTBB、Taskflow、各分配器）、统一的 raw.csv / summary.csv / device.json 格式与汇总脚本（“无显著提升”判定只在一处定义）、一键重跑脚本。验收：同指纹字节一致；外部库与自研实现通过同一校验；现有 11/11 测试仍通过。
2. 第 3 阶段起逐条做故事，每条九项齐全，按 A 节规则重复、配对、测 A/A。

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

- 2026-10-24 Claude Code：luowindows 授予锁定内存页权限并实测大页；飞书建项目概览与开发记录；第 1 阶段检查点处理（tcmalloc 修复、许可决定、Windows 大页脚本）；新增“面试级完善方案：当前进度”交接节。
- 2026-10-23 Claude Code：第 1 阶段核验完成。
- 2026-10-09：分工改为算法全部由助手完成。
