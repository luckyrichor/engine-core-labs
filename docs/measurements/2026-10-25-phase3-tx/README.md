# 2026-10-25 阶段 3：tx 测量

来源：engine-core-labs@2c29cd2 + 本轮实现；精确源码 SHA256 **5d01e5cbb103ef40ecd03c6bb44bb19a44c2562c15a75bcaabad5bfe1e9081af**，逐文件清单见各组 device.json。
13组、5940次正式进程运行，每配置预热1轮后正式10轮，含各组基线@aa。41940指标行，invalid=0，stub=0，所有吞吐配置10个有效轮次。

- [自动生成的结果表](RESULTS.md)、[完整吞吐比较](throughput-comparisons.csv)、[审计](audit.json)。
- [S3曲线](s3-speedup.svg)、[曲线数据](s3-curve.csv)、[理想并行时间偏差估计](s3-efficiency-gap.csv)。估计 1-serial_time/(T*parallel_time) 合并同步、入队、内存效应与不均衡，不是纯调度器开销；负值未裁剪。自研search_fraction另为实测选任务/转移耗时占可用worker时间的比例，二者不可混为一谈。
- [S2软件定位材料](profiles/README.md)、[验证范围与日志](validation/README.md)。
- [S1九项故事](../../stories/S1.md)、[S2九项故事](../../stories/S2.md)、[S3九项故事](../../stories/S3.md)、[方法和局限](../../stories/phase3-method.md)。

S1成本是实际Bistro网格包围盒操作的耗时分布，经16倍时间压缩并量化/重采样后的CPU代理；不是实际引擎任务。S3约121ns～0.988ms，固定2^23整数工作循环。不同口径/库分片模型的代价见方法文档，8线程一律超订。
进程快照只记录pid/comm/CPU/内存，不包含命令行凭据。共享虚拟机有常驻服务，未声称隔离；A/A波动很大时仍按统一判定保留“无显著提升”。Windows/UE未测，按方案阶段8补测。

## 重跑

```sh
export ECL_DATA="${ECL_DATA:-$HOME/data/engine-core-labs}"
cmake -S . -B "$ECL_DATA/build-phase3" -DCMAKE_BUILD_TYPE=Release -DECL_DATA="$ECL_DATA"
cmake --build "$ECL_DATA/build-phase3" -j 2
python3 -m bench.phase3 prepare
python3 -m bench.phase3 measure --suite all --output docs/measurements/<新目录>-phase3-tx
python3 -m bench.phase3_report docs/measurements/<新目录>-phase3-tx
# 追加 --plots 需要 matplotlib；本图版本3.11.2，使用独立用户目录环境。
python3 docs/measurements/2026-10-25-phase3-tx/analyze.py
```

原始raw.csv不覆盖、不删除。每个组目录保留生成配置、raw.csv、summary.csv、device.json和该组复现说明。
重新采集成本会改变指纹；复现本轮应使用仓库中冻结的 docs/datasets/bistro-costs/histogram.json。
