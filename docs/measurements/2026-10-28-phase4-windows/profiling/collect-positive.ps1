$ErrorActionPreference='Stop'
$root='E:\ecl-scratch\phase4-20261028'
$diag='F:\VS\IDE\Team Tools\DiagnosticsHub\Collector\VSDiagnostics.exe'
$cfg='F:\VS\IDE\Team Tools\DiagnosticsHub\Collector\AgentConfigs\CpuUsageHigh.json'
Add-Type 'using System;using System.Runtime.InteropServices;public class Awake{[DllImport("kernel32.dll")]public static extern uint SetThreadExecutionState(uint flags);}'
[Awake]::SetThreadExecutionState(2147483651) | Out-Null
$original=[regex]::Match((powercfg /getactivescheme),'[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}').Value
[IO.File]::WriteAllText("$root\profiling\original-power-plan.txt",$original)
try {
 powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c
 $case='positive'
 $sid=[guid]::NewGuid().ToString()
 & $diag start $sid "/launch:$root\build\Release\phase4_bench.exe" "/launchArgs:--meta $root\profiling\$case\s8-profile-$case.meta.json --impl packed --threads 4 --pin 1 --cpus 0,2,4,6,8,10,12,14 --require-ac 1" "/loadConfig:$cfg"
 if($LASTEXITCODE -ne 0){throw 'start failed'}
 Start-Sleep -Seconds 4
 $limit=(Get-Date).AddMinutes(3)
 while(Get-Process phase4_bench -ErrorAction SilentlyContinue){if((Get-Date) -gt $limit){throw 'Target exceeded 3 minutes'};Start-Sleep -Seconds 1}
 & $diag stop $sid "/output:$root\profiling\$case.diagsession"
 if($LASTEXITCODE -ne 0){throw 'stop failed'}
 Get-Item "$root\profiling\$case.diagsession" | Select-Object Name,Length | ConvertTo-Json
} finally {
 powercfg /setactive $original
 $restored=[regex]::Match((powercfg /getactivescheme),'[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}').Value
 [IO.File]::WriteAllText("$root\profiling\restored-power-plan.txt",$restored)
 [Awake]::SetThreadExecutionState(2147483648) | Out-Null
 if($restored -ne $original){throw 'Power mismatch'}
}
