#Requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ProjectPath
)
$ErrorActionPreference = 'Stop'
$projectFile = (Resolve-Path -LiteralPath $ProjectPath).Path
if ([IO.Path]::GetExtension($projectFile) -ne '.uproject') {
    throw 'ProjectPath must point to an existing .uproject file.'
}
$repoRoot = Split-Path -Parent $PSScriptRoot
$pluginRoot = Join-Path $repoRoot 'Plugins'
$descriptor = Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json -AsHashtable
$relativePlugins = [IO.Path]::GetRelativePath((Split-Path -Parent $projectFile), $pluginRoot).Replace('\', '/')
$changed = $false
$directories = @($descriptor.AdditionalPluginDirectories | Where-Object { $_ })
if ($relativePlugins -notin $directories) {
    $descriptor.AdditionalPluginDirectories = @($directories) + @($relativePlugins)
    $changed = $true
}
$plugins = @($descriptor.Plugins | Where-Object { $_ })
$core = @($plugins | Where-Object { $_.Name -eq 'TTRPGCore' })
if ($core.Count -eq 0) {
    $descriptor.Plugins = @($plugins) + @(@{ Name = 'TTRPGCore'; Enabled = $true })
    $changed = $true
} else {
    foreach ($entry in $core) {
        if ($entry.Enabled -ne $true) {
            $entry.Enabled = $true
            $changed = $true
        }
    }
}
if ($changed) {
    $backupDirectory = Join-Path $repoRoot '.local/HostBackups'
    New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null
    $backupName = '{0}.{1}.bak' -f [IO.Path]::GetFileName($projectFile), [Guid]::NewGuid().ToString('N')
    $backupPath = Join-Path $backupDirectory $backupName
    Copy-Item -LiteralPath $projectFile -Destination $backupPath
    $descriptor | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $projectFile -Encoding utf8
    Write-Host "Connected TTRPGCore. Original descriptor: $backupPath"
} else {
    Write-Host 'TTRPGCore is already connected; no changes made.'
}
