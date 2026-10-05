#Requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ProjectPath,
    [Parameter(Mandatory)][string]$EngineRoot
)
$ErrorActionPreference = 'Stop'
$projectFile = (Resolve-Path -LiteralPath $ProjectPath).Path
$projectRoot = Split-Path -Parent $projectFile
$editor = Join-Path (Resolve-Path -LiteralPath $EngineRoot).Path 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$logDir = Join-Path $projectRoot 'Saved/TabletopPrototype'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
& $editor $projectFile -run=TTRPGCreateMap -unattended -nop4 -NullRHI -nosound "-abslog=$logDir/Prepare.log" *> (Join-Path $logDir 'PrepareConsole.log')
if ($LASTEXITCODE -ne 0) { throw "Tabletop map creation failed. See $logDir/Prepare.log" }
$configPath = Join-Path $projectRoot 'Config/DefaultEngine.ini'
$config = Get-Content -LiteralPath $configPath -Raw
$mapPath = '/TTRPGCore/Maps/TabletopPrototype.TabletopPrototype'
$updated = $config -replace '(?m)^GameDefaultMap=.*$', "GameDefaultMap=$mapPath"
$updated = $updated -replace '(?m)^EditorStartupMap=.*$', "EditorStartupMap=$mapPath"
if ($updated -ne $config) {
    $backupDir = Join-Path (Split-Path -Parent $PSScriptRoot) '.local/HostBackups'
    New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
    $backupPath = Join-Path $backupDir ('DefaultEngine.ini.' + [Guid]::NewGuid().ToString('N') + '.bak')
    Copy-Item -LiteralPath $configPath -Destination $backupPath
    Set-Content -LiteralPath $configPath -Value $updated -Encoding utf8 -NoNewline
    Write-Host "Set tabletop startup map. Original config: $backupPath"
}
Write-Host 'Tabletop ready. Open the project and press Play, or use Launch-Tabletop.ps1.'
