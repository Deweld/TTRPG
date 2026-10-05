#Requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ProjectPath,
    [Parameter(Mandatory)][string]$EngineRoot
)
$ErrorActionPreference = 'Stop'
$projectFile = (Resolve-Path -LiteralPath $ProjectPath).Path
$editor = Join-Path (Resolve-Path -LiteralPath $EngineRoot).Path 'Engine/Binaries/Win64/UnrealEditor.exe'
# This is the user-facing interactive prototype window.
Start-Process -FilePath $editor -ArgumentList @("`"$projectFile`"", '/TTRPGCore/Maps/TabletopPrototype', '-game', '-windowed', '-ResX=1440', '-ResY=900', '-nosplash', '-log=TabletopPrototype.log')
