param([int]$Port = 8766)
$repository = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$script:problems = 0
function Pass([string]$Message) { Write-Host "[ OK ] $Message" -ForegroundColor Green }
function Note([string]$Message) { Write-Host "[INFO] $Message" }
function Fix([string]$Message, [string]$Remedy) {
    $script:problems++
    Write-Host "[FIX ] $Message" -ForegroundColor Yellow
    Write-Host "       $Remedy"
}
Write-Host 'AnyPS5 checks this PC before you run docker compose up.'
Write-Host ''
if ([Environment]::Is64BitOperatingSystem) { Pass '64-bit Windows' } else { Fix 'This Windows is 32-bit.' 'AnyPS5 needs 64-bit Windows 10 or 11.' }
if (Get-Command wsl.exe -ErrorAction SilentlyContinue) {
    & wsl.exe --status *> $null
    if ($LASTEXITCODE -eq 0) { Pass 'Windows Subsystem for Linux (WSL) is installed' } else { Fix 'WSL is not set up.' 'Open PowerShell as administrator, run: wsl --install, then restart. Guide: https://learn.microsoft.com/windows/wsl/install' }
} else { Fix 'WSL is not installed.' 'Open PowerShell as administrator, run: wsl --install, then restart. Guide: https://learn.microsoft.com/windows/wsl/install' }
$dockerReady = $false
if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    Fix 'Docker Desktop is not installed.' 'Install it from https://docs.docker.com/desktop/setup/install/windows-install/ then open it once and wait until it says the engine is running.'
} else {
    $system = & docker info --format '{{.OSType}}' 2>$null
    if ($LASTEXITCODE -ne 0) {
        Fix 'Docker Desktop is installed but not running.' 'Open Docker Desktop and wait until it says the engine is running, then run this check again. If it never starts, turn on virtualization in your PC''s BIOS or UEFI settings.'
    } elseif ("$system".Trim() -ne 'linux') {
        Fix 'Docker Desktop is using Windows containers.' 'Right-click the Docker icon near the clock and choose "Switch to Linux containers".'
    } else {
        $dockerReady = $true
        Pass 'Docker Desktop is running Linux containers'
    }
    $version = "$(& docker compose version --short 2>$null)".Trim().TrimStart('v')
    $parsed = $null
    if ([version]::TryParse(($version -replace '[^0-9.].*$', ''), [ref]$parsed) -and $parsed -ge [version]'2.17') { Pass "Docker Compose $version" } else { Fix 'Docker Compose 2.17 or newer was not found.' 'Update Docker Desktop to the latest version.' }
}
if ($dockerReady) {
    $resources = "$(& docker info --format '{{.NCPU}} {{.MemTotal}}' 2>$null)".Trim() -split ' '
    if ($resources.Count -eq 2) {
        $processors = [int]$resources[0]
        $memory = [double]$resources[1] / 1GB
        Note ("Docker can use $processors processors and {0:N1} GB of memory." -f $memory)
        if ($processors -lt 4) { Note "Only for building from source: the engine build uses 4 jobs by default. Before the build, run: `$env:ANYPS5_BUILD_JOBS = '$processors'" }
        if ($memory -lt 10) { Note "Only for building from source: a measured engine build with 4 jobs peaked at 8.5 GB of memory. With less than 10 GB for Docker, raise its memory limit or, before the build, run: `$env:ANYPS5_BUILD_JOBS = '2'" }
    }
}
$drive = Get-PSDrive -Name ((Get-Item -LiteralPath $repository).PSDrive.Name) -ErrorAction SilentlyContinue
if ($drive) { Note ("{0:N0} GB free on drive {1}:, where this folder is. The downloaded images and exported engine need about 1.5 GB. Building the engine from source instead used about 20 GB of Docker disk and 40 minutes on a 4-processor machine. Docker Desktop keeps its data on the drive set in its settings." -f ($drive.Free / 1GB), $drive.Name) }
$listener = $null
try {
    $listener = New-Object Net.Sockets.TcpListener ([Net.IPAddress]::Loopback, $Port)
    $listener.Start()
    Pass "Port $Port is free for the web app"
} catch {
    $running = if ($dockerReady) { "$(& docker compose --project-directory $repository ps -q web 2>$null)".Trim() } else { '' }
    if ($running) { Pass "Port $Port is in use by AnyPS5, which is already running" } else { Fix "Port $Port is used by another program." "Close that program, or choose another port before docker compose up: `$env:ANYPS5_WEB_PORT = '8767'" }
} finally { if ($listener) { $listener.Stop() } }
Write-Host ''
if ($script:problems) {
    Write-Host "$($script:problems) thing(s) to fix. Fix them, then run Check-My-PC.cmd again." -ForegroundColor Yellow
    exit 1
}
Write-Host 'This PC is ready. Run Start-AnyPS5.cmd, or docker compose up in this folder.' -ForegroundColor Green
