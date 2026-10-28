# Windows验证范围

- configure.log/build.log：MSVC19.44.35228、VS2022 x64、Release C++20，无需改变实验实现。
- test.log：独立phase4构建的S8契约1/1，不是全仓38项；五组12布局变体、1/4workers×绑核开关，验证计数/逐ID/独立摘要、CPU资格与掩码读回和非法CPU拒绝。
- measure.log：五组正式矩阵完成。audit.json核对1360进程、8160行有效，每指标配置10轮，5组源码指纹与tx完全相同。性能不属于正确性测试断言；A/A长尾全部保留。
- CMakeCache.txt保存具体编译/链接选项及工具链；当前PDB/EXE保存在Windows的E:\ecl-scratch\phase4-20261028\build\Release，未将二进制构建产物提交仓库。
- original-power-plan.txt与restored-power-plan.txt一致；采集结束后的独立SSH命令亦读回平衡方案。Windows没有采Linux perf事件，没有用0冒充未采集数据。
- 转存时仅将PowerShell日志从UTF-16转为UTF-8；ZIP中的Windows反斜杠路径按目录拆解，未改CSV数据。源archive保存在tx的ECL_DATA/phase4-windows-20261028/results.zip。
- Visual Studio VSDiagnostics/CPUUsageHigh配置及TraceEvent3.1.9已确认存在；实际CPU采样和热点分析尚未完成，待稳定连接后补齐。
