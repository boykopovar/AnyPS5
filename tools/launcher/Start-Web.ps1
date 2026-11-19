param(
    [switch]$Uninstall,
    [switch]$NoBrowser,
    [switch]$NoStartup,
    [string]$CollectionDirectory,
    [string]$StateDirectory,
    [string]$InstallDirectory,
    [string]$ProjectName = 'anyps5-launcher',
    [int]$Port = 8766
)
$ErrorActionPreference = 'Stop'
$repository = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location -LiteralPath $repository
if ($Uninstall) {
    & "$PSScriptRoot\Setup-WebHelper.ps1" -Uninstall -InstallDirectory $InstallDirectory -NoStartup:$NoStartup
    Write-Output 'To stop this app, run docker compose down in its folder. Games and user data are retained.'
    exit 0
}
if (-not (Get-Command docker -ErrorAction SilentlyContinue)) { throw 'Install Docker Desktop, start it, then run Start-AnyPS5.cmd again.' }
$containerSystem = & docker info --format '{{.OSType}}'
if ($LASTEXITCODE -ne 0) { throw 'Start Docker Desktop, wait until its engine is ready, then retry.' }
if ($containerSystem.Trim() -ne 'linux') { throw 'Switch Docker Desktop to Linux containers, then retry.' }
& docker compose version | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Docker Compose v2 is required. Update Docker Desktop.' }
$env:ANYPS5_WEB_PORT = $Port
if ($StateDirectory) { $env:ANYPS5_WEB_STATE_DIR = [IO.Path]::GetFullPath($StateDirectory) }
if (-not (Test-Path -LiteralPath "$repository\tools\engine\Dockerfile")) {
    & "$PSScriptRoot\Setup-WebHelper.ps1" -Port $Port -CollectionDirectory $CollectionDirectory -StateDirectory $StateDirectory -InstallDirectory $InstallDirectory -NoStartup:$NoStartup -LibraryFolder ''
}
& docker compose -p $ProjectName up -d --build --wait
if ($LASTEXITCODE -ne 0) { throw 'App startup failed. Check Docker Desktop and the Compose output above, then retry.' }
$url = "http://127.0.0.1:$Port"
$ready = $false
$reported = ''
$deadline = (Get-Date).AddMinutes(30)
while (-not $ready -and (Get-Date) -lt $deadline) {
    try {
        $response = Invoke-WebRequest -UseBasicParsing -Uri "$url/ready" -TimeoutSec 5
        $ready = $response.StatusCode -eq 200
    } catch {
        $status = $null
        try { $status = ($_.ErrorDetails.Message | ConvertFrom-Json).engine } catch { }
        if ($status.status -eq 'failed') { throw $status.message }
        if ($status.message -and $status.message -ne $reported) { Write-Output $status.message; $reported = $status.message }
    }
    if (-not $ready) { Start-Sleep -Seconds 2 }
}
if (-not $ready) { throw 'The app did not become ready. Run docker compose logs to see what is still running.' }
if (Test-Path -LiteralPath "$repository\tools\engine\Dockerfile") {
    $current = Get-Content -Raw -LiteralPath "$repository\.local\engine-artifacts\current.json" | ConvertFrom-Json
    if ($current.id -notmatch '^[0-9a-f]{64}$') { throw 'Engine export identity is invalid' }
    $helper = Join-Path $repository ('.local\engine-artifacts\bundles\' + $current.id + '\helper\AnyPS5DesktopHelper.exe')
    & "$PSScriptRoot\Setup-WebHelper.ps1" -HelperExecutable $helper -Port $Port -CollectionDirectory $CollectionDirectory -StateDirectory $StateDirectory -InstallDirectory $InstallDirectory -NoStartup:$NoStartup -LibraryFolder ''
}
Write-Output "Ready: $url"
if (-not $NoBrowser) {
    $stable = if ($InstallDirectory) { $InstallDirectory } else { Join-Path $env:LOCALAPPDATA 'AnyPS5\DesktopHelper' }
    & "$PSScriptRoot\Pair-Browser.ps1" -InstallDirectory $stable
}
