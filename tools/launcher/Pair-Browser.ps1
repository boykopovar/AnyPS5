param([string]$InstallDirectory = (Join-Path $env:LOCALAPPDATA 'AnyPS5\DesktopHelper'))
$ErrorActionPreference = 'Stop'
$helper = Join-Path $InstallDirectory 'AnyPS5DesktopHelper.exe'
$config = Join-Path $InstallDirectory 'helper.conf'
if (-not (Test-Path -LiteralPath $helper)) { throw 'Run Start-AnyPS5.cmd first to install the desktop helper.' }
$temp = Join-Path ([IO.Path]::GetTempPath()) ([IO.Path]::GetRandomFileName())
try {
    $process = Start-Process -FilePath $helper -ArgumentList @('--pair', ('"' + $config + '"')) -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $temp
    if ($process.ExitCode -ne 0) { throw 'Pairing is unavailable. Run Start-AnyPS5.cmd and check Docker Desktop.' }
    $code = [IO.File]::ReadAllText($temp).Trim()
    if ($code -notmatch '^\d{8}$') { throw 'Could not create a pairing code. Check Docker Desktop.' }
    Add-Type -AssemblyName System.Windows.Forms
    [Windows.Forms.MessageBox]::Show("Enter this code in your AnyPS5 browser:`n`n$code`n`nIt expires in two minutes and works once. Only pair a browser you opened yourself.", 'AnyPS5 - Pair browser', 'OK', 'Information') | Out-Null
} finally { if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp } }
