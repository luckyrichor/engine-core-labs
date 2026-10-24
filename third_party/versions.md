# 外部依赖固定版本

2026-10-23 第 1 阶段固定（tx 上 `git ls-remote` 解出的提交号）。源码与二进制不进仓库，放在 `$ECL_DATA/third_party`（默认 `~/data/engine-core-labs/third_party`）。
两台机器都**从源码按同一标签构建**，不用 apt 包，保证 Linux 与 Windows 被比较的是同一个版本。每组结果的 device.json 必须写入这里的标签与提交号。

| 库 | 用途 | 标签 | 提交 | 许可 | 说明 |
|---|---|---|---|---|---|
| oneTBB | S1、S3 调度基线 | v2023.1.0 | 3046c8b0c29d | Apache-2.0 | 地址为 uxlfoundation/oneTBB；apt 版 2021.11.0 不用 |
| Taskflow | S1、S3 调度基线 | v4.1.0 | 45366fe5bc4f | MIT | 纯头文件，需 C++20（本仓库已是 C++20） |
| jemalloc | S4 分配器基线 | 5.4.0 | 7a34f18502e7 | BSD-2-Clause | apt 版 5.3.0 不用 |
| mimalloc | S4 分配器基线 | v3.5.4 | f8401befa675 | MIT | Linux 用 LD_PRELOAD，Windows 用重定向 DLL |
| tcmalloc（gperftools） | S4 分配器基线（默认） | gperftools-2.19.1 | d8b459be774d | BSD-3-Clause（构建时核对 COPYING） | 结果表头写 `tcmalloc-gperftools` |
| tcmalloc（google/tcmalloc） | S4 分配器基线（可选，仅 tx） | 无标签 | 7aaa4776b419 | Apache-2.0 | 是否采用待所有者决定，见 docs/phase1-verification.md |
| ufbx | Bistro 预处理（只在 tools/） | v0.23.1 | 26a482ae6687 | MIT 或公有领域 | 第 1 阶段选定，取代 Assimp |
| Box2D | S7 的 2D 对照 | v3.2.0 | 178495ba3569 | MIT | |
| box2d-benchmarks | S7 的 2D 场景 | 无标签 | 2d2f9c39f849 | MIT | |
| mimalloc-bench | S4 trace 采集程序来源 | 无标签 | 391382606681 | MIT（bench/ 各自许可） | 仅 Unix |
| Tracy | luowindows 剖析（可选） | v0.14.1 | 30997d5ca6bb | 构建时核对 | |

数据集：

| 数据集 | 版本 | 校验 |
|---|---|---|
| Amazon Lumberyard Bistro | Bistro_v5_2.zip，894,377,473 字节 | SHA256 0d50e3c724c6c5da19f8eb99ad3f53e36fec37ffa2df9621f9ccf0603f3934e1（`scripts/data/fetch_bistro.sh` 内置校验） |
| glTF Sample Assets | 提交 8f492390ddc7a2ae221a1369ef560589cf8c70fb | 统计见 docs/datasets/gltf-sample-assets/ |
