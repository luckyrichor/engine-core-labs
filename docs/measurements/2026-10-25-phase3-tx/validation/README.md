# 验证范围

- 完整外部 Release CTest：37/37，无跳过（release.log）。
- TSan：Chase-Lev回绕/末元素竞争/4个thief；自研static/one/half/chase、async、legacy/sleepers/serial的统一实验入口；原执行器两种偷取策略与条件唤醒开关的多生产者/20轮暂停恢复/关闭拒绝测试。命令均退出0，没有TSan诊断。空的tsan-pool.log代表该测试成功且无stdout，并非未执行。
- TSan构建为RelWithDebInfo、ENGINE_SANITIZER=thread、ECL_DATA为空；仅测试进程使用setarch x86_64 -R。外部oneTBB/Taskflow未以TSan构建，不把其Release一致性测试写成已通过TSan。
- 计时后每个元素ID恰好一次，工作摘要逐项与串行结果核对；meta SHA256与成本总和也验证。5940次正式进程运行无校验/换页判无效，41,940个指标样本行，所有吞吐配置10个有效轮次，见audit.json。
- 测量完成后源码指纹仍与13组device.json一致，收尾只更改docs/README/交接记录，不重复测量。

```sh
cmake -S . -B "$ECL_DATA/build-phase3-tsan" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENGINE_SANITIZER=thread -DECL_DATA=
cmake --build "$ECL_DATA/build-phase3-tsan" --target phase3_contract phase3_bench pool_test -j 2
setarch x86_64 -R "$ECL_DATA/build-phase3-tsan/experiments/l1-job-system/phase3/phase3_contract"
setarch x86_64 -R "$ECL_DATA/build-phase3-tsan/experiments/l1-job-system/pool_test"
# 用一个包装脚本 exec setarch x86_64 -R .../phase3_bench "$@"，再交给 check_benchmark.py。
```
