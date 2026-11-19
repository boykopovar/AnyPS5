$ErrorActionPreference = 'Stop'
$repository = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$current = Get-Content -Raw -LiteralPath "$repository\.local\engine-artifacts\current.json" | ConvertFrom-Json
if ($current.id -notmatch '^[0-9a-f]{64}$') { throw 'Run docker compose up to build/export the Windows engine first.' }
$launcher = Join-Path $repository ('.local\engine-artifacts\bundles\' + $current.id + '\AnyPS5Launcher.exe')
Start-Process -FilePath $launcher -ArgumentList @('--data-root',('"' + (Join-Path $repository '.local\launcher') + '"')) -WorkingDirectory $repository -WindowStyle Hidden
