param([string]$InstallDirectory, [switch]$NoStartup, [switch]$NoBrowser)
$ErrorActionPreference = 'Stop'
$current = Get-Content -Raw -LiteralPath "$PSScriptRoot\.local\engine-artifacts\current.json" | ConvertFrom-Json
if ($current.id -notmatch '^[0-9a-f]{64}$') { throw 'Run docker compose up to export the engine first.' }
$installer = Join-Path $PSScriptRoot ('.local\engine-artifacts\bundles\' + $current.id + '\Install-Helper.ps1')
& $installer -ApplicationDirectory $PSScriptRoot -InstallDirectory $InstallDirectory -NoStartup:$NoStartup -NoBrowser:$NoBrowser
