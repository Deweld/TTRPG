#Requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ProjectPath,
    [Parameter(Mandatory)][string]$EngineRoot
)
$ErrorActionPreference = 'Stop'
$projectFile = (Resolve-Path -LiteralPath $ProjectPath).Path
$engineDirectory = (Resolve-Path -LiteralPath $EngineRoot).Path
$editor = Join-Path $engineDirectory 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
if (!(Test-Path -LiteralPath $editor)) { throw "Missing Unreal editor command executable: $editor" }
$runId = [Guid]::NewGuid().ToString('N')
$reportDirectory = Join-Path (Split-Path -Parent $projectFile) "Saved/TTRPGAutomation/$runId"
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
& $editor $projectFile '-ExecCmds=Automation RunTests TTRPG.' '-TestExit=Automation Test Queue Empty' `
    "-ReportExportPath=$reportDirectory" "-abslog=$reportDirectory/Editor.log" `
    -unattended -nop4 -NullRHI -nosound -nosplash -stdout -FullStdOutLogOutput
if ($LASTEXITCODE -ne 0) { throw "Unreal tests failed with exit code $LASTEXITCODE. See $reportDirectory" }
$reportPath = Join-Path $reportDirectory 'index.json'
if (!(Test-Path -LiteralPath $reportPath)) { throw "No automation report was produced. See $reportDirectory" }
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
if ($report.succeeded -lt 1 -or $report.failed -gt 0 -or $report.notRun -gt 0) {
    throw "Automation tests did not all pass. See $reportPath"
}
Write-Host "Passed $($report.succeeded) tests. Report: $reportPath"
