#Requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ProjectPath,
    [Parameter(Mandatory)][string]$EngineRoot,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]+$')][string]$Target,
    [ValidateSet('Development', 'DebugGame', 'Shipping')][string]$Configuration = 'Development'
)
$ErrorActionPreference = 'Stop'
$projectFile = (Resolve-Path -LiteralPath $ProjectPath).Path
$engineDirectory = (Resolve-Path -LiteralPath $EngineRoot).Path
$buildScript = Join-Path $engineDirectory 'Engine/Build/BatchFiles/Build.bat'
if (!(Test-Path -LiteralPath $buildScript)) { throw "Missing Unreal build script: $buildScript" }
& $buildScript $Target Win64 $Configuration "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw "Unreal build failed with exit code $LASTEXITCODE." }
