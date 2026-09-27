# Windows payload build and deployment

The fork's `fix/integration-upstream-payload-runtime` branch contains the
MinGW PRX export fixes, patched library deployment script, and the experimental
guest kernel model. Build from this branch rather than the older
`integration/core-engine` checkout. In particular, that checkout's edited
`core/libs/prx/libc/CMakeLists.txt` names `Libc.cpp`, which does not exist.
The CMake generation failure prevents every later build and copy command.

Use PowerShell to create a separate checkout next to your current project.
This leaves any uncommitted local work in the old checkout intact:

```powershell
cd C:\path\to\ps5translation
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
`.sprx` copies, with the provided script:

```powershell
.\scripts\Deploy-PatchedLibraries.ps1 -BuildDir (Join-Path (Get-Location) 'build') -PayloadLibrariesDir 'C:\path\to\CrispyDoom\payloads\libs'
python .\scripts\audit-elf-imports.py 'C:\path\to\crispy-doom.elf' .\build\core\libs\libs
```

Do not copy from `build\core\libs\libs\unpatched`: those binaries lack the
NID patching needed by converted ELF imports. Run the audit against the
original ELF, replacing its example path with the actual file path. A missing
import report requires an implementation and another build; the audit passing
alone does not establish runtime compatibility.

If `LoadLibraryExA` still fails on `libSceVideoOut.sprx` with Windows error 127,
capture the complete import tables and the files actually deployed. The error
means a dependent module could not provide a requested procedure; the GDB
trace alone does not identify which procedure or module. From the payloads
directory, run:

```powershell
& C:\WinLibs\mingw64\bin\objdump.exe -p .\libs\libSceVideoOut.sprx | Select-String 'DLL Name:|vma:|ordinal:'
Get-ChildItem .\libs -File | Where-Object { $_.Extension -in '.sprx','.prx','.dll' } | Select-Object Name,Length,LastWriteTime
Get-FileHash .\libs\libSceVideoOut.sprx, .\libs\libSceVideoOut.prx -Algorithm SHA256
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
