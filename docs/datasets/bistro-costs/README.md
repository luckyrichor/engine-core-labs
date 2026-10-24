# Bistro 网格成本采集（2026-10-25，tx）

1186真实mesh instance；ufbx 0.23.1，geometry_to_world变换所有顶点并求紧包围盒。每实例2次预热+5次测量取中位数，mesh-costs.csv保存顶点数、中位ns与换算工作量。
collection.json给出原FBX SHA256、采集规则、校准和16倍时间压缩。校准整数循环中位数1.8844345ns/unit，仅用于约略粒度标签，不是跨机硬件常数。
histogram.json冻结work_units分布；workloads/phase3/的checksum必须匹配。重新采集产生不同指纹，不与本轮混用。

```sh
"$ECL_DATA/build-phase3/experiments/l1-job-system/phase3/phase3_bench" --action calibrate
"$ECL_DATA/build-phase3/experiments/l1-job-system/phase3/phase3_cost_collector" "$ECL_DATA/bistro/extracted/Bistro_v5_2/BistroInterior.fbx" "$ECL_DATA/phase3/costs-new" <上一步ns/unit> 16
```

该工具只用于采集预处理，ufbx不链接调度核心。原始FBX和生成的负载二进制不入仓库。
