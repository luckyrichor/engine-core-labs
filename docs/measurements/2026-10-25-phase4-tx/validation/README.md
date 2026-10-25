# 验证证据

- 最终Release CTest38/38，无跳过，release.log。
- 最终TSan定向4/4：pool_lifecycle、phase3_queue_contract、phase3_benchmark_contract、phase4_layout_contract，tsan.log。仅测试进程setarch x86_64 -R，不改全局ASLR。
- phase4契约覆盖五组12布局变体、1/4workers×绑核开关，计数/逐ID恰好一次/独立摘要、实际亲和性读回、非法CPU拒绝。没有性能阈值断言。
- 默认布局探针的修改前/后JSON完全相同，JobSystem392字节/align8，ChaseLev192/align64，WorkerStats48字节；candidate偏移在上一级layout-audit.json。探针用C++20、pthread及include/phase3头文件路径编译。
- 正式采集日志measure.log；五组源码SHA256与最终实现一致，audit.json核对1360次/9520行有效。raw.csv中的软件迁移计数由受控窗口perf实际采集，不能把未支持事件改写为0。
- TSan是自研路径的验证；不将外部库Release一致性包装成TSan覆盖。Windows/MSVC与VS采样尚未验证。

```sh
cmake -S . -B "$ECL_DATA/build-phase4-tsan" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENGINE_SANITIZER=thread -DECL_DATA=
cmake --build "$ECL_DATA/build-phase4-tsan" --target phase4_bench phase3_contract phase3_bench pool_test -j 2
setarch x86_64 -R ctest --test-dir "$ECL_DATA/build-phase4-tsan" -R '^(pool_lifecycle|phase3_queue_contract|phase3_benchmark_contract|phase4_layout_contract)$' --output-on-failure
```
