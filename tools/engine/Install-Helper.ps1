param([string]$ApplicationDirectory, [string]$InstallDirectory, [switch]$NoStartup, [switch]$NoBrowser)
$ErrorActionPreference = 'Stop'
$bundle = $PSScriptRoot
if (-not $ApplicationDirectory) {
    $candidate = Get-Item -LiteralPath $bundle
    while ($candidate -and -not (Test-Path -LiteralPath (Join-Path $candidate.FullName 'compose.yaml'))) { $candidate = $candidate.Parent }
    if ($candidate) { $ApplicationDirectory = $candidate.FullName }
}
if (-not $ApplicationDirectory) {
    Add-Type -AssemblyName System.Windows.Forms
    $picker = New-Object Windows.Forms.FolderBrowserDialog
    $picker.Description = 'Choose the AnyPS5 source folder containing compose.yaml'
    if ($picker.ShowDialog() -ne 'OK') { throw 'Installation cancelled. Existing helper retained.' }
    $ApplicationDirectory = $picker.SelectedPath
}
$ApplicationDirectory = [IO.Path]::GetFullPath($ApplicationDirectory)
if (-not (Test-Path -LiteralPath (Join-Path $ApplicationDirectory 'compose.yaml'))) { throw 'Choose the source folder containing compose.yaml' }
$manifest = Get-Content -Raw -LiteralPath (Join-Path $bundle 'manifest.json') | ConvertFrom-Json
$helper = Join-Path $bundle 'helper\AnyPS5DesktopHelper.exe'
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $helper).Hash.ToLowerInvariant() -ne $manifest.helper_build) { throw 'Helper checksum mismatch. Download the engine bundle again.' }
& "$bundle\tools\launcher\Setup-WebHelper.ps1" -ApplicationDirectory $ApplicationDirectory -HelperExecutable $helper -InstallDirectory $InstallDirectory -NoStartup:$NoStartup -LibraryFolder ''
if (-not $NoBrowser) {
    $stable = if ($InstallDirectory) { $InstallDirectory } else { Join-Path $env:LOCALAPPDATA 'AnyPS5\DesktopHelper' }
    & "$bundle\tools\launcher\Pair-Browser.ps1" -InstallDirectory $stable
}
