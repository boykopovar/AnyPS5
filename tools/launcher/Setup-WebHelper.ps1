param(
    [switch]$Uninstall,
    [string]$BuildDirectory = 'b',
    [string]$LibraryFolder = (Join-Path $env:USERPROFILE 'Games\AnyPS5-Portable\Games'),
    [int]$Port = 8766,
    [string]$CollectionDirectory,
    [string]$StateDirectory,
    [string]$InstallDirectory,
    [string]$HelperExecutable,
    [string]$ApplicationDirectory,
    [switch]$NoStartup
)
$ErrorActionPreference = 'Stop'
$repository = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if ($ApplicationDirectory) { $repository = [IO.Path]::GetFullPath($ApplicationDirectory) }
$stable = if ($InstallDirectory) { [IO.Path]::GetFullPath($InstallDirectory) } else { Join-Path $env:LOCALAPPDATA 'AnyPS5\DesktopHelper' }
$executable = Join-Path $stable 'AnyPS5DesktopHelper.exe'
$shortcut = Join-Path ([Environment]::GetFolderPath('Startup')) 'AnyPS5 Desktop Helper.lnk'
if ($Uninstall) {
    Get-Process -Name AnyPS5DesktopHelper -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $executable } | Stop-Process
    if (-not $NoStartup -and (Test-Path -LiteralPath $shortcut)) { Remove-Item -LiteralPath $shortcut }
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        try {
            if (Test-Path -LiteralPath $executable) { Remove-Item -LiteralPath $executable }
            break
        } catch {
            if ($attempt -eq 19) { throw }
            Start-Sleep -Milliseconds 250
        }
    }
    Write-Output 'Desktop helper startup and executable removed. Configuration, tokens, logs, games and saves were retained.'
    exit 0
}
if ($Port -lt 1024 -or $Port -gt 65535) { throw 'Choose a loopback port from 1024 to 65535' }
$source = Join-Path (Join-Path $repository $BuildDirectory) 'tools\launcher\AnyPS5DesktopHelper.exe'
if (Test-Path -LiteralPath (Join-Path $repository 'helper\AnyPS5DesktopHelper.exe')) { $source = Join-Path $repository 'helper\AnyPS5DesktopHelper.exe' }
if ($HelperExecutable) { $source = [IO.Path]::GetFullPath($HelperExecutable) }
if (-not (Test-Path -LiteralPath $source)) { throw "Build desktop_helper first: $source" }
$engine = ''
$engineSource = Split-Path (Split-Path $source -Parent) -Parent
if (Test-Path -LiteralPath (Join-Path $engineSource 'preparer\python\python.exe')) {
    $engineManifest = Get-Content -Raw -LiteralPath (Join-Path $engineSource 'manifest.json') | ConvertFrom-Json
    if ($engineManifest.id -notmatch '^[0-9a-f]{64}$') { throw 'Invalid engine cache identity' }
    $engine = Join-Path $stable ('engines\' + $engineManifest.id)
    New-Item -ItemType Directory -Force -Path (Split-Path $engine -Parent) | Out-Null
    if (-not (Test-Path -LiteralPath $engine)) {
        $staging = Join-Path (Split-Path $engine -Parent) ('.pending-' + [guid]::NewGuid().ToString('N'))
        Copy-Item -LiteralPath $engineSource -Destination $staging -Recurse
        $verify = Start-Process -FilePath (Join-Path $staging 'preparer\python\python.exe') -ArgumentList @('-I','-S','-B',('"' + (Join-Path $staging 'preparer\prepare.py') + '"'),'--verify-engine',('"' + $staging + '"')) -WindowStyle Hidden -Wait -PassThru
        if ($verify.ExitCode -ne 0) { throw "Engine validation failed. Existing helper retained. Staging: $staging" }
        Move-Item -LiteralPath $staging -Destination $engine
    }
}
$collection = if ($CollectionDirectory) { [IO.Path]::GetFullPath($CollectionDirectory) } else { Join-Path $repository '.local\launcher' }
$state = if ($StateDirectory) { [IO.Path]::GetFullPath($StateDirectory) } else { Join-Path $repository '.local\launcher-web' }
$token = Join-Path $state 'auth.token'
New-Item -ItemType Directory -Force -Path $stable,(Join-Path $state 'backend') | Out-Null
if (-not (Test-Path -LiteralPath $token)) {
    $bytes = New-Object byte[] 32
    $rng = [Security.Cryptography.RandomNumberGenerator]::Create()
    $rng.GetBytes($bytes)
    $rng.Dispose()
    [IO.File]::WriteAllText($token, (($bytes | ForEach-Object { $_.ToString('x2') }) -join ''), (New-Object Text.UTF8Encoding($false)))
}
$identity = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
& icacls.exe $token /inheritance:r /grant:r "*${identity}:(F)" '*S-1-5-18:(R)' | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Could not restrict pairing token permissions' }
$existing = @(Get-Process -Name AnyPS5DesktopHelper -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $executable })
$config = Join-Path $stable 'helper.conf'
$pending = Join-Path $stable 'helper.pending.conf'
$content = @('AnyPS5 Desktop Helper 1',$collection,$state,$token,"http://127.0.0.1:$Port") -join "`n"
if ($engine) { $content += "`n$engine" }
[IO.File]::WriteAllText($pending, $content, (New-Object Text.UTF8Encoding($false)))
if ($HelperExecutable) {
    $validation = Start-Process -FilePath $source -ArgumentList @('--validate-config',('"' + $pending + '"')) -WindowStyle Hidden -Wait -PassThru
    if ($validation.ExitCode -ne 0) { throw 'New helper validation failed. Existing helper retained.' }
}
if (Test-Path -LiteralPath $executable) { Copy-Item -LiteralPath $executable -Destination ($executable + '.previous') -Force }
if (Test-Path -LiteralPath $config) { Copy-Item -LiteralPath $config -Destination ($config + '.previous') -Force }
if ($existing.Count) {
    $configFile = Join-Path $stable 'helper.conf'
    if (Test-Path -LiteralPath $configFile) {
        $oldConfig = [IO.File]::ReadAllLines($configFile)
        if ($oldConfig.Count -ge 2 -and $oldConfig[1] -ne $collection) { throw 'This helper belongs to another app folder. Use that folder to update it, or uninstall its helper first. User data is retained.' }
    }
    $existingIds = @($existing | ForEach-Object { $_.Id })
    $running = Get-CimInstance Win32_Process | Where-Object { $_.ParentProcessId -in $existingIds }
    if ($running) { throw 'Close running games and finish or cancel preparation before updating the helper.' }
    $existing | Stop-Process
    $existing | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
}
try {
for ($attempt = 0; $attempt -lt 20; $attempt++) {
    try { Copy-Item -LiteralPath $source -Destination $executable -Force; break } catch {
        if ($attempt -eq 19) { throw }
        Start-Sleep -Milliseconds 250
    }
}
$config = Join-Path $stable 'helper.conf'
$content = @('AnyPS5 Desktop Helper 1',$collection,$state,$token,"http://127.0.0.1:$Port") -join "`n"
if ($engine) { $content += "`n$engine" }
[IO.File]::WriteAllText($config, $content, (New-Object Text.UTF8Encoding($false)))
if (-not (Test-Path -LiteralPath (Join-Path $collection 'data/library.txt')) -and $LibraryFolder -and (Test-Path -LiteralPath $LibraryFolder -PathType Container)) {
    $init = Start-Process -FilePath $executable -ArgumentList @('--initialize',('"' + $collection + '"'),('"' + $LibraryFolder + '"')) -WindowStyle Hidden -Wait -PassThru -RedirectStandardError (Join-Path $state 'setup-error.log')
    if ($init.ExitCode -ne 0) { throw "Close the native launcher and retry setup. Initialization failed; see $state\setup-error.log" }
}
if (-not $NoStartup) {
$shell = New-Object -ComObject WScript.Shell
$link = $shell.CreateShortcut($shortcut)
$link.TargetPath = $executable
$link.Arguments = '--config "' + $config + '"'
$link.WorkingDirectory = $stable
$link.Description = 'AnyPS5 signed-in Windows desktop helper for the local web launcher'
$link.Save()
}
Start-Process -FilePath $executable -ArgumentList @('--config',('"' + $config + '"')) -WorkingDirectory $stable -WindowStyle Hidden
Start-Sleep -Seconds 2
if (-not (Get-Process -Name AnyPS5DesktopHelper -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $executable })) { throw 'New helper exited during startup' }
} catch {
    Get-Process -Name AnyPS5DesktopHelper -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $executable } | Stop-Process
    if (Test-Path -LiteralPath ($executable + '.previous')) { Copy-Item -LiteralPath ($executable + '.previous') -Destination $executable -Force }
    if (Test-Path -LiteralPath ($config + '.previous')) {
        Copy-Item -LiteralPath ($config + '.previous') -Destination $config -Force
        Start-Process -FilePath $executable -ArgumentList @('--config',('"' + $config + '"')) -WorkingDirectory $stable -WindowStyle Hidden
    }
    throw
}
Write-Output "Installed: $executable"
Write-Output 'Pair your browser with Pair-Browser.cmd.'
Write-Output "Browser: http://127.0.0.1:$Port"
Write-Output 'Run docker compose up -d from the repository. The helper reconnects automatically.'
