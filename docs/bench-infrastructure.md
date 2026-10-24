# 阶段 A：公共结果协议与运行器

本设施只采集实验程序已计算的指标，不在运行器中计时，不调整系统、swap、亲和性或实验实现。阶段 A 的示例为 `stub=1` 合成数字，不是性能结果。仅使用 Python 3.12 标准库。

## 入口与验证

```bash
python3 bench/run.py /path/to/config.json
python3 bench/summarize.py /path/to/result-directory --baseline base
python3 bench/device.py
python3 -m unittest discover -s bench/tests -v
```

`bench/tests/test_runner.py` 在系统临时目录生成配置、负载描述，调用 `run.py` 和 `summarize.py`，检查输出后清理；不会向 `docs/measurements/` 写示例结果。`bench/tests/fake_bench.py` 是可控的 CSV 假程序。

## 运行配置

配置示例（负载文件路径相对于配置文件所在目录；命令从仓库根运行）：

```json
{
  "story": "S-test-stub",
  "baseline": "base",
  "implementations": [
    {"name": "base", "command": ["{python}", "{repo}/bench/tests/fake_bench.py", "--value", "100"]},
    {"name": "candidate", "command": ["{python}", "{repo}/bench/tests/fake_bench.py", "--value", "130"]}
  ],
  "rounds": 10,
  "warmup_rounds": 1,
  "threads": [1, 2],
  "workload_files": ["small.json"],
  "mode": "na",
  "output_dir": "temporary-stub-results",
  "compiler": ["c++", "--version"],
  "compile_options": ["-O2"],
  "dataset_checksums": {},
  "external_libraries": [],
  "timeout_seconds": 60
}
```

`small.json` 在阶段 A 只需包含 `{"name":"small"}`。运行器只读取描述、计算规范化 JSON（UTF-8、排序键、紧凑分隔符）的 SHA256 前 16 位；阶段 A 不生成负载数据。后续生成器沿用这一指纹方式。

- 必填：`story`、`baseline`、`implementations`、`threads`、`workload_files`。`rounds` 默认 10、下限 5；`warmup_rounds` 默认 1；`mode` 默认 `na`。
- 命令必须为参数数组，不经 shell。可替换字段：`python`、`repo`、`threads`、`workload`（描述文件绝对路径）、`workload_name`、`workload_fingerprint`、`round`、`mode`、`impl`（A/A 命令中仍为基线真实名称，raw.csv 另记 `@aa`）。字面花括号需写成 `{{` / `}}`。预热时 `round <= 0`；测量轮从 1 开始。
- 自动复制基线命令为 `<baseline>@aa`；保留 `@aa` 名称。每轮执行所有实现和 A/A，按轮次循环移动首个实现。每组负载、线程数组合分别预热，预热不写 raw.csv。
- `output_dir` 相对配置目录；省略时写 `docs/measurements/<北京时间日期>-<故事编号>-<机器名>/`。目录已存在即拒绝，不覆盖历史结果。
- 输出 `raw.csv`、`summary.csv`、`device.json`、`README.md`，另留配置快照 `config.json`。README 提供原始配置位置和重跑命令；重跑前选择新的输出目录。
- `compiler` 是查询实际编译器版本的参数数组；编译选项、数据集校验和、外部库名/版本/配置由调用方提供，运行器不推测真实构建选项。外部库建议格式 `[{"name":"gperftools/tcmalloc","version":"...","configuration":{...}}]`。

## 输出与失败处理

实验 stdout 只输出 CSV（诊断信息请写 stderr）：

```csv
metric,value,unit,better,valid,invalid_reason,stub
throughput,100,tasks/s,higher,1,,0
latency_p99,12,us,lower,1,,0
latency_samples,1000,count,higher,1,,0
```

`better` 为 `higher` / `lower`，`valid`、`stub` 为 0/1，指标值必须有限且为数值；同一执行不允许重复指标。任何指标校验失败或进程非零退出，会将该执行的所有指标标无效；Linux swap 已用量前后不同也使整个执行无效。非零退出但 CSV 完整时仍记录 CSV 与退出码。输出格式损坏、启动失败、超时会终止运行，README 标记 failed；保留部分 raw.csv 供排查，不生成成功汇总。

CSV 公共列固定在 `bench/schema.py`。`pinned=0` 表示运行器未请求绑核；`pinned_cpus` 在 Linux 记录进程继承的允许 CPU 集合，不宣称实验工作线程已逐个绑定。程序自行绑核时，本阶段运行器不能探测其每个线程的绑定情况。Windows 此列留空。

## 汇总口径

判定阈值与决策只在 `bench/verdict.py` 定义。统计值只来自 raw.csv 中 `valid=1` 的行；device.json 只提供记录机器的逻辑线程数，缺少该文件时用当前机器的 `os.cpu_count()`，因此跨机器汇总必须携带 device.json。

按 `(workload, threads, mode, metric, impl)` 分组；计算有效轮数、值的中位数和范围。比值始终为同轮实现值 / 基线值，零分母不生成比值。显著性要求实现有效轮与可配对轮均至少 5；A/A 也需至少 5 个有效配对，否则不能给显著结论。A/A 不足 5 个配对但非空时标 `insufficient_aa_rounds`。无可用 A/A 比值时标 `no_aa_group`。5–9 个有效轮标 `below_10_rounds`。

配对比值中位数距 1 不超过 10%（含边界），或比值范围与 A/A 范围相交（含恰好接触），判 `no_significant_difference`；否则按 better 方向判提升或退化。`*_p99` 每个有效轮必须有同实现、同轮次的 `*_samples >= 100`，对基线、A/A 的有效轮也检查；不足则优先判 `insufficient_samples`。无效行不贡献统计，完全无效的组仍显示为零有效轮。

线程数大于**物理核数**标 `oversubscribed`（方案 A 节；device.json 缺物理核数时退回逻辑线程数）。混合架构机器上，线程数大于性能核逻辑线程数标 `hybrid_cores`（方案 B 节，luowindows 为 16）。`stub=1` 加标 `stub`。同组或比较双方混合 stub/真实结果、单位/方向不一致、同名负载多指纹、重复有效轮次，均报错，避免混算。

## 环境指纹与平台边界

`device.json` 记录机器、内核、CPU、物理核/逻辑线程、总内存、编译器查询结果、调用方给定构建信息、git HEAD/dirty、swap、THP、Windows 电源计划与核类型。

源码清单覆盖仓库 `experiments/`、`bench/`、`tools/` 下 C/C++、Python、CMake、shell、PowerShell 源文件；不含缓存和生成数据。先算每文件 SHA256，按相对路径排序，将 `UTF-8路径 + NUL + 十六进制SHA256 + 换行` 拼接后算整体 SHA256。每文件哈希同时保留，新增、改名、内容变化均反映在整体值中。

Linux 读取 `/proc/cpuinfo`、`/proc/meminfo`、THP sysfs；无法获得物理核时保留 null，不假定逻辑线程等于物理核。Windows 使用 GetLogicalProcessorInformationEx 的 EfficiencyClass 和处理器组掩码，较大 class 为性能核、较小 class 为能效核；保留每核原始 class，单一 class 时能效核列表为空。内存读取 GlobalMemoryStatusEx，pagefile 总量/已用量读取 EnumPageFilesW，电源计划读取 powercfg。Windows 路径已实现，阶段 A 未在 Windows 运行验证；本阶段没有新增 C++ 平台代码。
