# 第 1 阶段核验报告

2026-10-23，Claude Code，tx 与 luowindows 实测。对应《engine-core-labs 面试级完善方案》F 节第 1 阶段与附录 C 的“核验结果”列。
标注沿用方案 A 节：〔事实〕已实测；〔推断〕未经实验；〔未验证〕尚未核实。

## 基线

- 〔事实〕仓库 HEAD 为 c7ca348，与方案附录 A 一致，无需更新基线。方案提到的 9134c1e 是 c7ca348 的父提交。
- 〔事实〕tx 上 Release 构建，ctest 11/11 通过。

## 与方案不一致的事实

| 方案中的说法 | 实测 | 影响 |
|---|---|---|
| Bistro 下载约 45 KB/s，需 5–6 小时 | 直连约 1.5 MB/s，经代理约 2.1 MB/s，整包 6.5 分钟下完 | 无需长时间后台下载 |
| BistroInterior_Wine 三角形 1,293,691（页面） | ufbx 与 Assimp 都读出 1,320,323 | 以文件实测为准；另两个文件与页面一致 |
| tx 根分区可用约 83 GB | 可用约 54 GB（下载前）| 10 GB 数据预算仍够 |
| tx swap 已用约 739 MB | 约 1.2–1.3 GB | 正式实验前仍须所有者决定 swap |
| mimalloc 最新 v3.4.5、gperftools 最新 2.18.1 | v3.5.4、2.19.1 | 已按新版本固定 |
| glTF Sample Assets 提交 edc7c9e | HEAD 8f49239 | 已按新提交统计 |

## 数据集

**Bistro**
- 〔事实〕`scripts/data/fetch_bistro.sh` 每次从 /bistro 跳转取签名地址，断点续传，校验大小、ZIP CRC（`unzip -t` 通过）和 SHA256。
- 〔事实〕894,377,473 字节，SHA256 `0d50e3c7…3934e1`，644 个条目，清单见 `docs/datasets/bistro/manifest.txt`。包内 LICENSE.txt 为 CC BY 4.0 全文。
- 〔事实〕FBX 解析结果见 `docs/datasets/bistro/fbx-probe.csv`：

| 文件 | 三角形（两库一致） | ufbx 网格数 / 加载 / 峰值 RSS | Assimp 网格数 / 加载 / 峰值 RSS |
|---|---|---|---|
| BistroInterior | 1,046,609 | 1186 / 0.42 s / 185 MiB | 2062 / 0.83 s / 437 MiB |
| BistroInterior_Wine | 1,320,323 | 1415 / 0.48 s / 207 MiB | 2289 / 1.03 s / 521 MiB |
| BistroExterior | 2,832,120 | 1296 / 1.09 s / 586 MiB | 1591 / 2.17 s / 1121 MiB |

- **选定 ufbx v0.23.1**：两库三角形数一致；ufbx 是单个 C 文件、MIT 或公有领域，加载约快一倍，内存约一半；网格数与 FBX 原始对象一一对应（Assimp 按材质拆网格）。只在 `tools/` 下的预处理程序中使用。探测程序见 `tools/fbx-probe/`。

**glTF Sample Assets**（`scripts/data/gltf_inventory.py`，结果在 `docs/datasets/gltf-sample-assets/`）
- 〔事实〕不使用 LFS（`.gitattributes` 只有换行规则）。浅克隆检出 1.43 GB，另有 .git 985 MB。
- 〔事实〕150 个模型目录、2482 个文件、1,433,321,625 字节。模型大小 p50 1.8 MB、p90 32.4 MB、最大 89.2 MB；单文件 p50 20 KB、p99 9.0 MB、最大 43.0 MB。
- 〔事实〕150 个模型在 REUSE.toml 中都有逐模型许可。其中 9 个是受限或非标准许可：BrainStem（Poser EULA）、DamagedHelmet（含 CC-BY-NC）、DragonAttenuation 与 DragonDispersion（Stanford）、Duck（SCEA）、EnvironmentTest（Adobe Stock）、PlaysetLightTest（CC-BY-NC-SA）、Sponza（CRYENGINE 协议）、VirtualCity（3DRT Testing）。
- 建议：资源清单的**大小分布**用全部 150 个模型（只统计，不分发）；需要实际加载内容的实验只用 CC0 / CC-BY 系列。待所有者确认。

## tx 环境

- 〔事实〕页缓存清理有效：读入 256 MiB 文件后 `Cached` 4,275,276 kB，`drop_caches` 后 963,488 kB；同一文件冷读 0.487 s，热读 0.035 s。正式冷盘实验仍须每轮记录前后值。
- 〔事实〕strace 6.8 可用；fio 3.36 已安装（sudo apt）。
- 〔推断〕不需要 clang：gcc 13 的 TSan/ASan/UBSan/LSan 此前已 11/11 通过，Chase-Lev 的 TSan 验证用 gcc 即可。若 TSan 对独立 fence 报误报再引入 clang。
- 〔事实〕外部库版本已固定，见 `third_party/versions.md`。全部从源码构建，不用 apt 包，以保证两台机器版本一致。

**google/tcmalloc**（HEAD 7aaa4776b419，仓库无版本标签）
- 〔事实〕Bazel 9.3.1（bazelisk 默认）：依赖 aspect_bazel_lib 与 Bazel 9 不兼容，分析阶段失败。
- 〔事实〕Bazel 8.4.2：protobuf 与 abseil 的兼容级别冲突，依赖解析失败。
- 〔事实〕实验性 CMake：可以配置，会下载并编译 abseil、protobuf、antlr 等整套依赖，构建结果见下方“未决”。

## luowindows

- 〔事实〕SSH 第一次尝试超时（Tailscale 显示 rx 0，只发不收）；同日稍后重试恢复，之后的连接都正常。原因未查明。
- 〔事实〕i9-13900HX，24 核 32 逻辑线程，13.7 GB 内存，Windows 11 build 26200。
- 〔事实〕核类型（GetLogicalProcessorInformationEx 的 EfficiencyClass）：**性能核 8 个，逻辑线程 0–15（开超线程，每核两线程）；能效核 16 个，逻辑线程 16–31**。所以“线程数不超过性能核”的实验，最多用 8 线程，每核一线程（绑 0,2,…,14）；或 16 线程（绑 0–15），此时带超线程。
- 〔事实〕剩余空间：C 171.8 / D 197.2 / E 175.9 / F 94.4 GB。E 大于 F，UE 对照目录建在 `E:\engine-core-labs-ue5-comparison`（已创建）。
- 〔事实〕CMake：系统无独立 CMake，也无 winget；VS2022 自带 `F:\VS\IDE\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`，3.31.6-msvc6。采用它，不另装，与 MSVC 工具链同源。
- 〔事实〕直连下载：GitHub release、developer.nvidia.com/bistro、raw.githubusercontent.com 的 HEAD 请求均为 200。
- 〔事实〕S5 的缺页统计方法：`GetProcessMemoryInfo` 的 `PageFaultCount`。触碰 64 MiB 计数增加 16416（预期 16384 页），可用。注意它不区分软、硬缺页。
- 〔事实〕大页：`VirtualAlloc(MEM_LARGE_PAGES)` 失败，错误 1314（缺 SeLockMemoryPrivilege），最小大页 2 MiB。需要所有者决定是否授予该权限（Home 版没有 secpol.msc）。
- 〔事实〕当前电源计划为“平衡”。按方案只在测量期间切到高性能，结束后恢复。

## 待所有者决定

1. google/tcmalloc 是否纳入（结果见下）。
2. glTF 的 9 个受限许可模型：是否只用于大小统计。
3. Windows 大页：是否授予 SeLockMemoryPrivilege；不授予则 S5 的 Windows 大页组列为不可行。
4. tx swap：第 5 阶段正式实验前再确认关闭时机（本阶段不需要）。

## 未决

- google/tcmalloc 的 CMake 构建结果：见 docs/progress.md 本阶段条目的最终记录。
