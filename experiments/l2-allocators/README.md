# L2：pool / stack / arena（W8，W10维护）

Codex，2026-10-07。实验64KiB容量，pool固定128字节块，stack后进先出，
arena批量回收。pool/stack实现已验收；**用户核心 Arena::allocate/reset 保留TODO**。
测量用 `CodexReferenceArena` 明确署名独立参考类，不能当作用户完成的代码。
按用户授权先临时实现推进验证，再清空用户函数；参考类仅作为验证夹具。
L2记工程基本完成、用户手写和讲解复验待办，不记全部完成。

W10维护：禁用拥有内部缓冲区的分配器拷贝/移动，避免指针所属存储改变；
验收越界、耗尽、重复释放、非法对齐、栈非LIFO释放、arena1000次回收。
Release 7/7 CTest；ASan/UBSan 7/7。首次 sanitizer 因 ptrace 环境不能运行
LeakSanitizer 而全部报错；禁用 leak 检测后通过，**没有完成泄漏检测**。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
build/experiments/l2-allocators/allocator_bench > docs/measurements/w8-allocators.csv
```

TX AMD EPYC 7K62虚拟4CPU，gcc13.3/CMake3.28。每分配器10轮×256次，
32/64/96字节循环、对齐16，记录单次分配耗时的p50/p95/p99，时钟成本未扣除。
三种×10轮是30行、7680次分配。stack元数据用预留vector，不声称完全无堆分配。

内部浪费=已消耗空间减请求有效字节，pool本负载浪费16416字节，
stack/arena的这个对齐负载为0。pool隔块释放后，外部碎片=1-最大连续空闲/总空闲，
约1/3。stack/arena只支持符合生命周期的释放：全部回收后为0；
这三项不是同一任意释放模式，不据此宣称某分配器普遍更好。
CSV原始分位数保留，不作为生产性能或用户手写成绩。
来源engine-core-labs@11c42ae + 本轮修改，精确源码SHA256见测量元数据。
