# L3 资源加载流水线

真实文件读取 + 合成CPU解码 + 主线程有限提交。复用L1 JobSystem，工作线程读文件/解码，future携带结果，主线程只安装完成的资产。最多8个在途任务、重复id合并、每帧最多2次安装，失败传播且不安装失败资产。单owner调用Pipeline；源文件在任务期内不得修改。退出等待在途任务，不宣称可中断正在运行的I/O。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
python3 experiments/l3-resource-loading/tools/measure.py
```

三方式对照：sync需求出现后主线程直接加载；async需求出现后后台读取并在后续帧安装，未完成时使用缺失占位；preload在帧循环前加载完全部资源，单独报告startup_ms。不要只看预加载帧时间而忽略启动成本。

32个64KiB文件，每8帧请求4个、96帧、帧目标间隔5ms，模拟基础工作sleep200us；CPU解码500000次整数混合，返回校验值。真实文件是刚写入夹具，OS缓存热，不是冷磁盘场景；不是PNG/mesh解码，不含GPU上传、真实渲染与屏幕帧率。输出逐帧耗时、资源缺失、在途数、启动/排空时间、最终校验和。每模式十轮、顺序轮换，完整工作量相同，最终资产与独立串行参考求和一致。

本机十轮p99跨轮中位数约sync6.498ms/async1.739ms/preload0.297ms；预加载启动约17.679ms。async缺失资产×帧数中位数48，sync/preload为0；不是“更快且没有代价”。原始帧、极值、源码SHA256见docs/measurements/2026-10-13-l3/。动态检查Release/TSan/ASan/UBSan/LeakSanitizer11/11通过，不代表所有时序证明。
