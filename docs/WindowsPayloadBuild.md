# Windows payload build and deployment

The fork's `fix/integration-upstream-payload-runtime` branch contains the
MinGW PRX export fixes, patched library deployment script, and the experimental
guest kernel model. Build from this branch rather than the older
`integration/core-engine` checkout. In particular, that checkout's edited
`core/libs/prx/libc/CMakeLists.txt` names `Libc.cpp`, which does not exist.
The CMake generation failure prevents every later build and copy command.

Use PowerShell to create a separate checkout next to your current project.
Replace the example parent directory below with a directory that exists on
your computer. This leaves any uncommitted local work in the old checkout
intact. If you already cloned the fork, do not clone it again:

```powershell
cd "$env:USERPROFILE\Downloads\ps5translation"
git clone --recurse-submodules --branch fix/integration-upstream-payload-runtime https://github.com/nahshongraham97/AnyPS5.git AnyPS5-fork
cd .\AnyPS5-fork
git remote -v
git status --short --branch
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target libs relinker --parallel 4
```

The branch's GitHub Actions build uses WinLibs MinGW GCC 16.2.0, POSIX threads,
SEH, and the MSVCRT runtime. Use that toolchain when reproducing CI; keep the
compiler and any copied MinGW runtime DLLs from the same installation.
After a successful build, deploy the **patched** libraries, including their
`.sprx` copies. Set `$payloadRoot` to your real `CrispyDoom\payloads` path.
The path below matches the Crispy Doom v1.0-7.1.0 download under your user
Downloads folder. These PowerShell commands do not require changing the
script execution policy:

```powershell
$repoRoot = (Get-Location).Path
$payloadRoot = Join-Path $env:USERPROFILE 'Downloads\crispy-doom-ps5-v1.0-7.1.0\CrispyDoom\payloads'
if (!(Test-Path -LiteralPath $payloadRoot -PathType Container)) { throw "Payload directory not found: $payloadRoot" }
$patchedDir = Join-Path $repoRoot 'build\core\libs\libs'
$librariesDir = Join-Path $payloadRoot 'libs'
$libraries = @(Get-ChildItem -LiteralPath $patchedDir -Filter '*.prx' -File)
if ($libraries.Count -eq 0) { throw "No patched PRX files found in $patchedDir" }
New-Item -ItemType Directory -Path $librariesDir -Force | Out-Null
foreach ($library in $libraries) {
    $name = [IO.Path]::GetFileNameWithoutExtension($library.Name)
    Copy-Item -LiteralPath $library.FullName -Destination (Join-Path $librariesDir "$name.prx") -Force
    Copy-Item -LiteralPath $library.FullName -Destination (Join-Path $librariesDir "$name.sprx") -Force
}
Write-Host "Deployed $($libraries.Count) patched libraries to $librariesDir"
```

Do not copy from `build\core\libs\libs\unpatched`: those binaries lack the
NID patching needed by converted ELF imports. Find the original ELF under
Downloads and run the audit against it:

```powershell
$elf = Get-ChildItem -LiteralPath (Join-Path $env:USERPROFILE 'Downloads') -Recurse -File -Filter 'crispy-doom.elf' -ErrorAction SilentlyContinue | Select-Object -First 1
if (!$elf) { throw 'crispy-doom.elf was not found under Downloads; set $elf to its actual location.' }
python .\scripts\audit-elf-imports.py $elf.FullName $patchedDir
```

If the ELF is outside Downloads, locate it with File Explorer and assign
`$elf = Get-Item 'its real path'`. A missing import report requires an
implementation and another build; the audit passing alone does not establish
runtime compatibility.

## Regenerate a converted executable after rebuilding the libraries

The converted `.exe` contains its own startup stub. Deploying new PRXs does
not update a previously converted executable. A trace that prints
`Loading module:` comes from an older relinker build; the current stub prints
`Loading PRX:` and tries the NID export before the original symbol name. For
example, `sceSystemServiceHideSplashScreen` is implemented by
`libSceSystemService` and its patched NID is `Vo5V8KAwCmk`.

From the fork checkout, with `$elf` and `$payloadRoot` set as above, generate
a separate executable and test that file:

```powershell
$converted = Join-Path $payloadRoot 'crispy-doom-current.exe'
& .\build\core\relinker\relinker.exe --windows --windows-diagnostics --skip-syscall-check $elf.FullName $converted
if ($LASTEXITCODE -ne 0) { throw "Relinker failed: $LASTEXITCODE" }
& C:\WinLibs\mingw64\bin\gdb.exe -batch -q -x (Join-Path $payloadRoot 'trace.gdb') $converted
```

`--skip-syscall-check` only allows conversion of this known payload with raw
syscall instructions. Those instructions are not yet routed to the guest
kernel model. A later startup error or crash must be diagnosed separately;
this command does not make the payload safe to run past that point.

If `LoadLibraryExA` still fails on `libSceVideoOut.sprx` with Windows error 127,
capture the complete import tables and the files actually deployed. The error
means a dependent module could not provide a requested procedure; the GDB
trace alone does not identify which procedure or module. With the variables
from the deployment block still set, run:

```powershell
& C:\WinLibs\mingw64\bin\objdump.exe -p (Join-Path $librariesDir 'libSceVideoOut.sprx') | Select-String 'DLL Name:|vma:|ordinal:'
Get-ChildItem -LiteralPath $librariesDir -File | Where-Object { $_.Extension -in '.sprx','.prx','.dll' } | Select-Object Name,Length,LastWriteTime
Get-FileHash (Join-Path $librariesDir 'libSceVideoOut.sprx'), (Join-Path $librariesDir 'libSceVideoOut.prx') -Algorithm SHA256
```

Check the named dependent libraries in the same payload directory for the
requested exports and record the full `objdump -p` output if the brief view
does not expose the failing symbol. Do not infer ELF startup success from a
successful Windows DLL load: the PS5 SDK `payload_args` startup contract and
guest machine-code syscall routing are still outstanding.

If you prefer to update the old checkout after saving your local edits, set
its `origin` to the fork and retain upstream as a separate remote:

```powershell
git remote set-url origin https://github.com/nahshongraham97/AnyPS5.git
git remote add upstream https://github.com/boykopovar/AnyPS5.git
git fetch origin
```

If `upstream` already exists, skip `git remote add upstream`. Inspect
`git status --short --branch` before switching branches; a fresh clone above
avoids overwriting the edited CMake files.
