# Relinker usage

## Input and conversion

Use an clean ELF executable. Place its bundled ELF modules in `sce_module/` or `sce_modules/` beside the input executable. Exactly one of these directories must exist; both present or both absent is an error.

```text
source/
    input.elf
    sce_module/
        <bundled ELF modules>
```

```text
relinker [options] <input.elf> <output>
```

Linux output:

```sh
relinker source/input.elf app.elf
```

Windows output:

```sh
relinker --windows source/input.elf app.exe
```

Add `--to-intel` for Intel hosts. The output format defaults to Linux ELF regardless of the filename; `.exe` alone does not select Windows.

## Test directory preparation

Python 3 can assemble a separate test directory, validate input ELF headers, copy resources and host libraries, and record the conversion command, result and log:

```powershell
python tools/prepare_game.py --platform windows --to-intel --input D:/game/decrypted/eboot.bin --assets D:/game --relinker build/core/relinker/relinker.exe --libraries build/core/libs/libs --runtime-dll-dir D:/winlibs/mingw64/bin --output D:/game-test
```

For Linux, use `--platform linux`, the Linux relinker and libraries, and omit `--runtime-dll-dir`. Use `--to-intel` on Intel hosts.

The output directory must be new and outside the inputs. The asset root's input filename and `sce_module/` or `sce_modules/` directories are excluded from resource copying; modules are converted from beside `--input`. Windows runtime DLLs must be in `--libraries` or `--runtime-dll-dir`. Failed conversion retains `conversion.log` and `prepare.json` and returns `2`. Invalid preparation returns `1`. Successful preparation returns `0`; game compatibility still requires launching and testing the executable.

Add `--module D:/game/decrypted/prx/extra.prx` for an ELF module outside the standard module directory. Repeat for multiple modules. Use `--module ImportedName.prx=D:/game/decrypted/prx/extra.prx` when the executable imports a different filename, including different capitalization. These modules are copied with the main executable into `relink-input/` inside the new output directory and processed together, preserving the originals. Filename collisions are errors. This prepares startup dependencies; it does not rewrite paths passed by the game to a dynamic module loader.

## Options

All switches are disabled by default. `unused-filter` defaults to `0`; `--rpath` defaults to `$ORIGIN/libs`.

| Option                        | Effect                                                                                                                                                                                                                                                                                                                  |
|-------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `--windows`                   | Produce a Windows PE executable.                                                                                                                                                                                                                                                                                        |
| `--windows-diagnostics`       | Include startup dependency diagnostics. Requires `--windows`.                                                                                                                                                                                                                                                           |
| `--windows-gui`               | Select the Windows GUI subsystem instead of the console subsystem. Requires `--windows`.                                                                                                                                                                                                                                |
| `--to-intel`                  | Convert supported AMD-only instructions in the executable and bundled modules. Unsupported instructions or unreachable conversion stubs cause an error.                                                                                                                                                                 |
| `unused-filter=0`             | Keep all imported NID references.                                                                                                                                                                                                                                                                                       |
| `unused-filter=1`             | Filter unused non-PLT imports using control-flow and GOT access analysis; preserve PLT imports.                                                                                                                                                                                                                         |
| `unused-filter=2`             | Apply strict unused-import analysis and compact the PLT. Unsupported analysis cases cause an error.                                                                                                                                                                                                                     |
| `--registry`                  | Write `<output-stem>.registry.json` beside the output executable.                                                                                                                                                                                                                                                       |
| `--rpath <path>`              | Set the system library search path. Quote `$ORIGIN` to prevent shell expansion, for example `--rpath '$ORIGIN/libs'` in Bash or PowerShell. Linux guest modules require an absolute path or a path beginning with `$ORIGIN`. Windows requires a nonempty ASCII path and supports `$ORIGIN` as the executable directory. |
| `--autorun`                   | Run the output after conversion, print its exit code, and wait for Enter. Adds executable permissions for Linux output. Requires the target OS and prepared runtime layout.                                                                                                                                             |
| `--skip-sce-module`           | Deprecated. Skip all bundled module processing.                                                                                                                                                                                                                                                                         |
| `--exclude-sce-module <file>` | Deprecated. Exclude a bundled module by exact filename, not path. Repeat for multiple files; a missing filename is an error. Conflicts with `--skip-sce-module`.                                                                                                                                                        |
| `--skip-syscall-check`        | Deprecated. Disable syscall scanning in the executable and bundled modules.                                                                                                                                                                                                                                             |
| `--lazy-binding`              | Deprecated. Enable lazy symbol binding instead of eager binding. Incompatible with bundled ELF modules.                                                                                                                                                                                                                 |

Specify `unused-filter=0|1|2` without `--`, at most once. Unknown options and extra positional arguments are errors. There is no `--help` flag; invoking `relinker` without arguments prints the usage syntax and exits with an error.

The `--skip-sce-module`, `--exclude-sce-module <file>`, `--skip-syscall-check`, and `--lazy-binding` flags are deprecated. If the application runs with these flags enabled, it will be extremely unstable and unsuitable for general use. These flags are only for debugging.

## Runtime layout

Paths are relative to the output executable:

```text
app.elf (Linux) or app.exe (Windows)
libs/
    *.prx
app0/
    <app resources>
    sce_module/
        <converted modules>
```

Use `sce_modules/` instead of `sce_module/` if that is the input directory name. Relinker writes converted modules under `app0/` and prints their exact paths. Place app resources in `app0/` separately. Copy the built system libraries from `build/core/libs/libs/*.prx` into `libs/`; use libraries built for the target OS. A custom `--rpath` changes the system library location.

Linux:

```sh
chmod +x app.elf
./app.elf
```

Windows PowerShell:

```powershell
.\app.exe
```

## Exit codes

`0`: conversion succeeded. `1`: invalid arguments. `2`: conversion failed; the error is printed to stderr. With `--autorun`, successful conversion returns the launched application's exit code.
