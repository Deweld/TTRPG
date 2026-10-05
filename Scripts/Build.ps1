#Requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ProjectPath,
    [Parameter(Mandatory)][string]$EngineRoot,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_]+$')][string]$Target,
    [ValidateSet('Development', 'DebugGame', 'Shipping')][string]$Configuration = 'Development',
    [switch]$RefreshModules
)
$ErrorActionPreference = 'Stop'
$projectFile = (Resolve-Path -LiteralPath $ProjectPath).Path
$engineDirectory = (Resolve-Path -LiteralPath $EngineRoot).Path
$buildScript = Join-Path $engineDirectory 'Engine/Build/BatchFiles/Build.bat'
if (!(Test-Path -LiteralPath $buildScript)) { throw "Missing Unreal build script: $buildScript" }
# UE 5.8 can reuse its cached module list after an external plugin descriptor changes.
# A descriptor timestamp refresh invalidates that cache without deleting build data.
if ($RefreshModules) { (Get-Item -LiteralPath $projectFile).LastWriteTime = Get-Date }
& $buildScript $Target Win64 $Configuration "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw "Unreal build failed with exit code $LASTEXITCODE." }
