# Windows失联排查（2026-10-28，Codex）

核对远端hostname=luowindows、当前身份LUOWINDOWS\shnh2后，只读查询System电源事件及XML、资源耗尽事件、当前CPU/内存/进程、服务状态与电源配置。原始查询保存在tx仓库外数据目录；不公开系统其他进程的明细。

## 证据（北京时间）

- 08:57:56.579，Kernel-Power506，进入Modern Standby，原因明确为Idle Timeout；BootId126，ScenarioInstanceIdV2=51。
- 16:10:42.182，Kernel-Power507结束该session：SleepEntered=true，SleepDurationInUs=25965555371，低功耗驻留约13550秒；同毫秒又有506进入下一session。该长session与上午SSH/Tailscale不可达时间范围相交；待机期间仍可能有短暂网络/执行活动，不能把全部时间看作彻底断电。
- 17:37:26.436，507，退出原因Power Button；此session的SleepEntered=false，说明不能仅凭所有506/507标题就一概称深度睡眠。
- 查询时19:43，CPU总利用率4%，可用物理内存3376128KiB（约3.22GiB），总14366200KiB；sshd/Tailscale服务均运行，无遗留phase4_bench/编译/采集进程。Get-Process.CPU是累计CPU秒，未当作实时占用率。
- 对前14小时System日志的Resource-Exhaustion-Detector查询未返回事件；无对应重启/意外关机记录，BootId一致。原始日志未保留连续CPU/内存时序，不能反推每一秒资源都充足。
- powercfg /a支持S0联网待机。当前原平衡方案交流睡眠/休眠超时均0；这不否定历史事件，也不能证明当时所有电源/屏幕策略相同。

## 结论与处理

〔事实〕发生过空闲触发的Modern Standby及实际低功耗驻留。〔推断〕待机是本轮长时间远程不可达的主要解释，证据比“任务过多导致CPU/内存满”更直接；未查到资源耗尽证据，但缺少历史采样，不能完全排除短暂高负载/网络因素。不把16:10未知数字原因16777220擅自翻译为内存/CPU故障。

本轮采样仅使用进程生命周期内SetThreadExecutionState(ES_CONTINUOUS|ES_SYSTEM_REQUIRED|ES_DISPLAY_REQUIRED)，finally释放；没有修改永久睡眠、Tailscale、SSH、防火墙设置。四份采样及符号分析完成，原平衡电源方案另行读回确认。正式positive8T两个长样本的具体起止没有同一时钟的电源/资源标记，仍保留“原因未证实”，没有因本次查到待机就把它们删除或归因。

资料：[微软系统电源状态](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/system-power-states)、[睡眠与SetThreadExecutionState](https://learn.microsoft.com/en-us/windows/win32/power/system-sleep-criteria)、[资源耗尽事件2004](https://learn.microsoft.com/en-us/troubleshoot/windows-server/performance/troubleshoot-application-service-memory-leaks)。


## 用户确认补充（2026-10-28）

用户随后确认：性能测试切换电源设置后变为10分钟熄屏、10分钟睡眠，导致SSH不可达。此项为用户核实后提供的信息；前文保留的是当时只读排查结果与不确定性，不回写成当时已采集到10分钟值。

后续测试按用户要求：接电时不熄屏、不睡眠，先保存原活动方案以及原/测试方案中将被修改的交流显示、睡眠超时，切高性能后显式设置为0并读回；测试结束或失败时恢复全部修改值及原方案，再逐项读回。SetThreadExecutionState仅作补充，不能替代超时设置；只恢复GUID也不足以确认完整恢复。历史runner/采样脚本尚未加入超时值保存恢复，再次执行前须先补齐。此次仅更新约定，没有远程修改当前电源设置。
