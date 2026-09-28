param(
    [Parameter(Mandatory = $true)]
    [string] $PayloadLibrariesDir,
    [string] $BuildDir = (Join-Path $PSScriptRoot '..\build')
)

$ErrorActionPreference = 'Stop'
$BuildDir = (Resolve-Path -LiteralPath $BuildDir -ErrorAction Stop).Path
& cmake --build $BuildDir --target libs --parallel 4
if ($LASTEXITCODE -ne 0) { throw "NID-patched library build failed: $LASTEXITCODE" }
$patchedDir = Join-Path $BuildDir 'core\libs\libs'
if (!(Test-Path -LiteralPath $patchedDir -PathType Container)) {
    throw "Patched library directory does not exist: $patchedDir. Build the libs target first."
}

$libraries = @(Get-ChildItem -LiteralPath $patchedDir -File -Filter '*.prx')
if ($libraries.Count -eq 0) {
    throw "No NID-patched PRX files found in $patchedDir. Do not deploy from its unpatched subdirectory."
}

New-Item -ItemType Directory -Path $PayloadLibrariesDir -Force | Out-Null
$destination = (Resolve-Path -LiteralPath $PayloadLibrariesDir).Path
$source = (Resolve-Path -LiteralPath $patchedDir).Path
if ($destination -eq $source) { throw 'Source and destination must be different directories.' }

foreach ($library in $libraries) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($library.Name)
    Copy-Item -LiteralPath $library.FullName -Destination (Join-Path $destination "$name.prx") -Force
    Copy-Item -LiteralPath $library.FullName -Destination (Join-Path $destination "$name.sprx") -Force
}
Write-Host "Deployed $($libraries.Count) NID-patched libraries as .prx and .sprx to $destination"
