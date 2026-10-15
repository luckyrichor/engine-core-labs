# L3 资源加载流水线

最后更新：2026-10-15（Codex）

**异步把代价从帧时间转成资源暂时缺失，预加载把它转到启动时间。**

真实文件读取 + 合成 CPU 解码 + 主线程有限安装。复用 L1 JobSystem；可选的 64KiB 固定缓冲槽复用 L2 Arena，串起 L1 / L2 / L3。不是图片解码器、GPU 上传或完整引擎资源系统。

## 生命周期与所有权

单 owner 调用 Pipeline，后台任务读取文件并解码，future 返回只含 id/hash 的 Asset。最多 8 个在途请求，重复 id 合并；每帧最多 2 次安装，异常向 owner 传播且失败资源不安装。退出等待在途任务，不能中断已经运行的 I/O，读取期间不得修改源文件。

`vector` 每次 load 新建 64KiB 缓冲；`arena` 在启动时创建 8 个 L2 Arena，每个槽 64KiB，初始化会清零触页。后台任务在互斥锁下领取独占槽，RAII lease 在解码结束或异常时 reset 并归还。锁只保护槽领取/归还，文件读与解码不持锁。Asset 不保留缓冲区指针，避免 reset 后悬空。池由任务 shared_ptr 保活，JobSystem 退出等待，不会先释放仍在使用的槽。

原有 L2 Pool 是 128 字节块，不能直接装 64KiB；本实现使用固定大小槽池管理 L2 Arena，不修改小对象 Pool 的原契约。8 槽净数据 512KiB，另有元数据和 future/stream 分配；不代表整个加载过程零分配。初始化、占用和同步成本也需要比较，未假定一定更快。

## 构建和复现

在仓库根目录：

```bash
bash scripts/bootstrap.sh
python3 experiments/l3-resource-loading/tools/measure.py --output .local/l3-review
# 单次；夹具由 measure.py 生成
build/experiments/l3-resource-loading/resource_bench async .local/l3-assets 500000 3 arena
```

单次参数：MODE ASSET_DIR DECODE_ITERATIONS [WORKERS vector|arena]，旧三参数调用仍默认 4 workers/vector。测量默认每配置 10 轮，支持 --build / --output / --repeats / --iterations。顺序轮换且奇数轮反转；采样时不要同时跑编译和 sanitizer。

## 八组对照

- sync_a / sync_b：相同同步实现的 A/A 对照。
- async_a / async_b：相同 4 worker/vector 异步实现的 A/A 对照。
- async_3：3 worker/vector。
- arena_4 / arena_3：相应工作线程数下复用 Arena 缓冲槽。
- preload：4 worker/vector，帧循环前安装全部资源，单列 startup_ms。

每轮输入相同：32 个 64KiB 热文件，每 8 帧需求增加 4 个，96 帧、目标间隔 5ms、基础工作 sleep200us、解码 500000 次整数混合。warm OS cache 是新写夹具的特征，不是冷磁盘测量。所有配置与独立串行加载校验和相等才纳入。合成解码通常比 64KiB 分配本身昂贵，缓冲复用收益可能被淹没。

## 指标怎样读

`raw.csv`：frame>=0 是逐帧；-2 是逐资源延迟；-1 是运行总计。每次 96 帧、32 条资源记录。`summary.csv` 保留逐轮 p50/p99/max、缺失资产×帧、初始化/启动/排空及两种延迟。`paired.csv` 是同轮 A/A、3/4 worker 和缓冲方式的比值。

- frame_ms：需求处理、安装和模拟基础工作；不包含最后用于 5ms 节奏的 sleep_until。
- demand_to_install_ms：首次需求到 owner 真正安装，包括等待容量、后台执行和每帧提交预算。
- request_to_install_ms：首次受理请求到安装，不含入队前容量重试；不是纯 I/O 完成时间。
- sync 一批 4 个依次加载，所以最后一个资源的需求延迟包含前面资源的时间。
- preload 的需求/受理时间发生在启动阶段；其延迟不当作游戏中缺失等待。startup 包括初始化线程/缓冲槽和预加载，setup 是其中初始化部分。
- 缺失资产×帧是占位累积量；延迟 p99 是每轮 32 资源的近尾顺序统计量，不是生产规模尾延迟保证。保留 max 和原始样本。

Linux 逐运行读取 /proc/stat，保存 steal_ticks / steal_ms / steal_pct 和 CPU pressure、loadavg。steal_ms 为所有 vCPU 累计 guest-reported steal ticks，百分比分母为同区间所有 vCPU 的总 tick；不是主线程被抢占时间，也不含 guest 内部任务调度竞争。0 只说明 guest 没报告 steal，不能据此排除虚拟化争用。loadavg 有历史记忆。3 vs 4 workers 是过量订阅敏感性对照；若要证明具体尖峰由主线程抢占造成，需要逐线程调度跟踪，不能只靠这组曲线。

## 数据和检查

新结果见 [2026-10-15 对照](../../docs/measurements/2026-10-15-l3/README.md)。旧 2026-10-13 数值仅对应当时版本，保留原始数据。Release 和动态检查结果、精确源文件和可执行文件 SHA256 见本轮记录；sanitizer 只覆盖运行到的路径，不构成全部时序证明。
