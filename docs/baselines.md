# 固定版本外部基线与一致性校验

本设施由 Codex 实现；它验证相同负载的执行结果与分配器身份，不产生性能结论。
版本与提交以 `third_party/versions.md` 为准，构建时必须核对实际 HEAD。

## 构建与复现（tx）

```bash
export ECL_DATA="${ECL_DATA:-$HOME/data/engine-core-labs}"
# 只有直连 SSH 不通时，给本次命令指定现有代理；不修改 SSH 配置。
export GIT_SSH_COMMAND='ssh -o ConnectTimeout=15 -o BatchMode=yes -o ProxyCommand="nc -X connect -x 127.0.0.1:7890 %h %p"'
bash scripts/third_party/build.sh all
cmake -S . -B "$ECL_DATA/build-adapters" -DCMAKE_BUILD_TYPE=Release -DECL_DATA="$ECL_DATA"
cmake --build "$ECL_DATA/build-adapters" -j 2
ctest --test-dir "$ECL_DATA/build-adapters" --output-on-failure
```

也可单独构建 `oneTBB`、`Taskflow`、`jemalloc`、`mimalloc`、`gperftools`、`google-tcmalloc`。
源码在 `$ECL_DATA/third_party/src/`，安装在 `third_party/install/<名字>-<标签或提交>/`。
`build-info.json` 只在成功后写入；重建开始先撤掉旧回执，防止失败构建仍被认证为可用。
回执包含提交、编译器、Release 编译选项、配置项、源码/安装位置、依赖和构建时间。
CMake 只发现该目录下带匹配回执的固定版本，不回退到系统包。
未构建的库显示明确提示，并注册返回 77 的 CTest 项，结果为 **Skipped**。

Ubuntu 缺少 autoconf、automake、libtool、m4 时，构建脚本用 `apt-get download` 下载固定版本
Debian 包，在 `$ECL_DATA/build-tools/` 解包并重定位脚本；不执行维护脚本、不调用 sudo、不写系统目录。
一并提取 autotools-dev 提供的 config.guess/config.sub。原包 SHA256 与版本记入 `build-tools/receipt.json`，并复制进库构建回执。
其他平台须预装构建工具；本轮 Windows/MSVC 路径尚未运行，不声称跨平台验收通过。

## 调度接口

`experiments/common/scheduler_adapter.hpp` 的 `execute(impl, costs, threads)` 返回成本总和、
每个任务执行次数与实际工作摘要。所有实现运行相同的整数循环，成本单位是工作单位，不是校准后的纳秒。

| 实现名 | 提交与等待 | 线程设置 |
|---|---|---|
| jobs-steal / jobs-no-steal | 原 JobSystem 的 submit/future/wait；偷取开/关 | T 个 worker |
| oneTBB | task_arena + task_group，每个任务单独提交 | arena 最大并发 T，包含参与工作的调用线程 |
| Taskflow | Executor + 逐任务 emplace + run/get | T 个 worker |
| std-async | T 个 launch::async 工作分片，每片循环执行下标 t,t+T,… | T 个异步任务；不为每个成本项创建操作系统线程 |

oneTBB 用 task_group、Taskflow 用逐任务 emplace，是为了保持明确的任务 ID 和逐任务校验，
避免 parallel_for 的自动分块将当前调度单位改变。std::async 的分片明确属于该基线的适配策略；
不能把它解释为逐任务 async 开销。这里只控制线程数，不修改库的默认调度策略。
独立答案来自负载 meta；对 2000 个任务分别用 1/2/4 线程验证每个 ID 恰好一次、校验和及工作摘要。

## 分配接口与身份

同一份 `alloc_replay.cpp` 按库分别链接。按时间戳顺序串行回放 trace：分配后写入由对象 ID
及偏移决定的字节模式，释放前逐字节核对，并核对线程标签、分配/释放次数、峰值存活字节与最终零存活。
此阶段保留 thread 字段用于一致性检查；尚未实现按 trace 线程并发回放，不能用它给出多线程分配性能结论。
校验器读取 meta 并验证 SHA256；错误答案、损坏文件必须失败。

| 目标后缀 | 链接方式 | 进程内核验 |
|---|---|---|
| libc | 系统 malloc/free，无替换库 | Linux dlsym/dladdr 确认 malloc 来自 libc.so；其他平台仅报告 system-malloc |
| local | 显式调用原 VariableHeap，不替换进程 malloc | 编译选择固定为本地接口；容量沿用 64 KiB |
| jemalloc | 本地 Release 共享库，直接链接并写 build RPATH | mallctl version + arenas.lookup 确认 malloc 分配归属 + dladdr |
| mimalloc | 本地 Release 共享库，直接链接并写 build RPATH | mi_version + mi_is_in_heap_region + dladdr |
| gperftools | 本地 libtcmalloc 共享库，直接链接并写 build RPATH | MallocExtension 属性与 GetOwnership + dladdr；结果名 tcmalloc-gperftools |
| google | 静态库及其 CMake 解析出的完整传递依赖闭包 | google MallocExtension 属性与强枚举 Ownership::kOwned + dladdr；结果名 tcmalloc-google |

mimalloc 本轮采用同源码分别链接的方式，避免靠 LD_PRELOAD 让测试意外使用另一个安装版本。
这只是基线校验目标的选择，后续真实 trace 采集方式单独记录。
Google 构建复用第 1 阶段缓存，仅构建 `tcmalloc_tcmalloc`，不编有 gcc 13 错误的上游测试。
jemalloc 额外传入由已核实标签和 HEAD 组成的 `--with-version`，避免上游在源码外构建时版本检测回退到 missing_version。
Google 构建脚本生成一个不构建的 CMake 消费目标，从 link.txt 解析链接依赖，复制静态库与公共头文件
（含 .inc/.def），生成 `google-tcmalloc.cmake`；依赖 HEAD 逐个写入回执。
Google 构建目前依赖本机第 1 阶段已有 CMake 缓存，尚不支持在空机器上独立重建该缓存。

## 当前边界

本轮只做功能和一致性校验，未执行正式性能实验。重复次数、A/A、swap、亲和性和统计判定
由 `bench/` 及后续故事负责。不得从 CTest 耗时推导任何库的性能优劣。
源码之外的构建与验证产物应放 ECL_DATA；历史已被跟踪的 `build-ext/` 不在本轮清理范围。
