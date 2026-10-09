# 2026-10-09 验证与数据索引

来源：engine-core-labs@b8be686 + 本轮未提交修改；各 dataset 的 device.json 保存实际源文件 SHA256 和可执行文件 SHA256。最终核对全部所录源文件 SHA256 与提交前文件一致。文档修改不属于被测二进制。

- coarse / overhead：各 320 次，32 配置 × 10 轮；raw、summary、带标准差 SVG、逐次后台负载。
- allocators：80 行，clock.csv 的 100000 对读时钟控制，fragmentation.csv 的失败 / 合并成功验证。
- l1-legacy-profile / stat / legacy-profile-source：旧 executor 的 CPU 样本，不与不同方法的新曲线算前后比。
- release-tests.txt：Release 8/8。
- tsan.txt：Debug -fsanitize=thread -fno-pie / -no-pie，TSAN_OPTIONS=halt_on_error=1，进程级 setarch x86_64 -R，8/8。首轮未关 ASLR 时映射冲突，非 race 报告。
- asan-ubsan.txt：Debug address,undefined，detect_leaks=0 / halt_on_error=1，8/8。
- leak-sanitizer.txt：同 ASan 构建，sudo 测试进程 detect_leaks=1 / halt_on_error=1，8/8。普通用户 detect_leaks=1 的首轮全部因权限 / ptrace 失败，未当代码错误；权限修正后的结果单独记录。

CPU 四核虚拟机，后台 loadavg 0.60–4.10；独立运行测量集，未同时编译 / 运行其他本项目基准，但其他项目活动不受本任务控制。批次性能非单次尾延迟，sanitizer 通过非所有路径证明。数值摘要见 ../../status.md。
