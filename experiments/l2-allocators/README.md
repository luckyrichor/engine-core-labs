# L2：分配器生命周期与批量测量

最后更新：2026-10-09；实现：Codex。Arena 已完成，不再保留用户 TODO。64KiB 缓冲，最大对齐 64。

## 算法

- Pool：128 字节固定块，空闲块内存储下一块索引，O(1) 分配 / 回收；保留 live bitmap 检测重复释放和外来指针。元数据包含 bitmap 和对象补白，不能代表没有检查的生产池。
- Stack：对齐 bump + 逆序释放，记录旧 offset；记录 vector 预留 4096 项，预留空间明确计入 bookkeeping。这里用可验证记录表，非零开销。
- Arena：对齐 bump，`std::align` 检查空间，O(1) reset；无逐对象释放，reset 不调用对象析构。批次 / 帧生命周期才适用。
- malloc：通用分配基线，内部元数据和实际保留空间未查询，CSV 留空而非报 0。

这些分配器均非线程安全；不能复制 / 移动拥有缓冲的对象。安全检查不能识别旧指针指向后来同地址的新对象，调用方仍负责生命周期。

## 测量

```bash
build/experiments/l2-allocators/allocator_bench > .local/l2.csv
build/experiments/l2-allocators/fragmentation_demo > .local/fragmentation.csv
```

4 分配器 × 2 尺寸模式 × 10 轮 = 80 行；轮转分配器顺序，50 批预热，每批 256 次，至少 2000 批且每轮至少持续 100ms。aligned 为 32/64/96；odd 为 3/17/33/65/95，对齐 16，实际触发 stack / arena 补白。

一对时钟计量整批分配与每块 1 字节写入，结果除以 256。p50/p95/p99 是“批均摊耗时”的分布，不是单次分配尾延迟。验证每块数据在分配计时外；回收单独计时。Pool / malloc 逐块释放，stack 逆序释放，arena 一次 reset，回收语义不同。构造、预留与校验不在分配时段；系统分配器缓存状态、后台负载仍影响结果。时钟控制数据见本轮报告，摊薄时钟开销不等于完全扣除。

内部浪费为已用缓冲减请求总字节。固定 pool 的空闲块连续性不影响合法请求，因此不再以 `1-largest/total` 宣称 pool 的外部碎片。

单独的 VariableHeap 是字节粒度 first-fit / 相邻合并示例，元数据在外部 map，不是 malloc 替代品。先填满所有空间再隔块释放，256 字节请求在总空闲 32768、最大块 128 时失败；随后释放剩余块合并到 65536，请求成功。这时 `1-largest/total` 才说明变长连续请求的碎片。历史 w8 CSV 保留，其单次时钟和旧固定池指标已被本方法替代。

## 本轮验证

Release、TSan、ASan/UBSan、启用泄漏检测均 8/8 CTest。普通用户 LeakSanitizer 曾全部因 ptrace / 权限报错，最终以 sudo 测试进程启用 detect_leaks=1 通过，日志在本轮目录；未更改系统配置。复验方式：

```bash
cmake -S . -B .local/asan -DCMAKE_BUILD_TYPE=Debug -DENGINE_SANITIZER=address
cmake --build .local/asan -j2
sudo env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ctest --test-dir .local/asan --output-on-failure
python3 experiments/l2-allocators/tools/measure.py --output .local/l2-results
```

## 单次观测与时钟底噪

新增 `allocator_tail [window_csv]`，10轮 × 两尺寸模式 × 四分配器及一个 clock+write 控制 =100行，每行102400次逐次计时、400个窗口。每次计时包含 allocate、1字节写入和读时钟成本，保存未均摊的 p50/p99/p99.9/max；另存每256次的最大值原始CSV，避免稀释孤立慢操作。控制组采用相同读时钟+写入方式，辨认共同噪声；不能从分位数直接减去控制组分位数，也不能据此分解调度中断与分配器内部停顿。逐次计时会扰动缓存和时序，它是另一个实验，不与batch曲线合并排序。

Arena 每256次分配后实际只有一次 reset。旧的0.117ns来自约30ns计时区域除以256，不是一次reset的真实时间；新版 Arena 的 `reclaim_batch_p50_ns_per_op` 留空，`reclaim_operations=1`，并标注 `unresolved_single_reset`。区域时长只作含时钟的观测值，当前方法未解析出reset本身成本。

不对 Pool / Stack / Arena 作跨机器排序，也不把“均比malloc快”当普遍结论。malloc场景单线程、热身后循环小尺寸（aligned三种、odd五种），每批持有256块再释放，常见分配器缓存容易复用；它不是单一尺寸逐次立即释放，也没有覆盖跨线程释放、长期存活或大对象。通用分配器承担的语义不同，倍数限定到本机此场景。

### Linux 冷内存路径（单独于小块热路径）

```bash
python3 experiments/l2-allocators/tools/cold_measure.py --build build --output .local/l2-cold
```

十轮，每模式启动新进程；512 个 64 KiB 区域全部保持存活，按系统页大小逐页写入，共 32 MiB。匿名 mmap 新映射首次触页与计时前预触页作控制；malloc_growth 测分配加触页、存活集逐步增长，不保证 sbrk 或 malloc 内部扩容发生。构建/映射不在首次触页时间内，malloc 调用包含在 growth 时间内。每次读钟插桩，rusage 在计时外读取，记录 minor/major faults；原始 CSV 保留个体尾部。不能把此结果称为自定义分配器的固有冷尾延迟或 CPU 冷缓存数据。
