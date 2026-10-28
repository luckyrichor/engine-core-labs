$ErrorActionPreference='Stop'
$d='F:\VS\IDE\Common7\IDE\PrivateAssemblies'
[Reflection.Assembly]::LoadFrom("$d\Microsoft.Diagnostics.Tracing.TraceEvent.dll") | Out-Null
$root='E:\ecl-scratch\phase4-20261028\profiling'
foreach($case in @('positive','queue','counters','stats')) {
if($case -ne 'positive'){ & 'F:\VS\IDE\Team Tools\DiagnosticsHub\Collector\VSDiagnostics.exe' expandDiagSession "$root\$case.diagsession" }
$etl=(Get-ChildItem "$root\$case" -Recurse -Filter '*.etl' | Select-Object -First 1).FullName
$log=[Microsoft.Diagnostics.Tracing.Etlx.TraceLog]::OpenOrConvert($etl,$null)
$sw=[IO.StringWriter]::new()
$reader=[Microsoft.Diagnostics.Symbols.SymbolReader]::new($sw,'E:\ecl-scratch\phase4-20261028\build\Release',$null)
$reader.SecurityCheck=[Func[string,bool]] {param($p) $p.StartsWith('E:\ecl-scratch\phase4-20261028\',[StringComparison]::OrdinalIgnoreCase)}
$mod=$log.ModuleFiles | Where-Object Name -eq 'phase4_bench' | Select-Object -First 1
$log.CodeAddresses.LookupSymbolsForModule($reader,$mod)
$proc=$log.Processes | Where-Object Name -eq 'phase4_bench' | Select-Object -First 1
$self=@{};$incl=@{};$total=0;$stacks=0;$unresolved=0;$project=0
foreach($evt in $proc.EventsInProcess){
 if($evt -isnot [Microsoft.Diagnostics.Tracing.Parsers.Kernel.SampledProfileTraceData]){continue}
 $total++;$stack=$log.GetCallStackForEvent($evt)
 if($null -eq $stack){continue}
 $stacks++;$seen=@{};$leaf=$true
 while($null -ne $stack){
  $ca=$stack.CodeAddress
  $fn=$ca.FullMethodName
  if([string]::IsNullOrEmpty($fn)){$fn=$ca.ModuleName+'!0x'+$ca.Address.ToString('x')}
  $fn=$ca.ModuleName+'!'+$fn
  if($leaf){$self[$fn]++;if($ca.ModuleName -eq 'phase4_bench'){$project++;if($null -eq $ca.Method){$unresolved++}};$leaf=$false}
  if(-not $seen.ContainsKey($fn)){$incl[$fn]++;$seen[$fn]=$true}
  $stack=$stack.Caller
 }
}
$selfrows=@($self.GetEnumerator() | Sort-Object Value -Descending | ForEach-Object {@{function=$_.Key;samples=$_.Value;percent=100.0*$_.Value/$total}})
$inclrows=@($incl.GetEnumerator() | Where-Object Key -match 'phase4_bench' | Sort-Object Value -Descending | ForEach-Object {@{function=$_.Key;samples=$_.Value;percent=100.0*$_.Value/$total}})
$result=@{case=$case;process=$proc.Name;pid=$proc.ProcessID;exit_status=$proc.ExitStatus;total_samples=$total;stack_samples=$stacks;project_leaf_samples=$project;unresolved_project_leaf=$unresolved;events_lost=$log.EventsLost;sample_interval_ms=$log.SampleProfileInterval.TotalMilliseconds;pdb=$mod.PdbName;pdb_guid=$mod.PdbSignature.ToString();pdb_age=$mod.PdbAge;self=$selfrows;inclusive_project=$inclrows;symbol_log=$sw.ToString()}
[IO.File]::WriteAllText("$root\$case-hotspots.json",($result | ConvertTo-Json -Depth 8))
$result | Select-Object case,total_samples,stack_samples,project_leaf_samples,unresolved_project_leaf,events_lost | ConvertTo-Json
$reader.Dispose();$log.Dispose()
}
