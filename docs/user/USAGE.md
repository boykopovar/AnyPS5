# Relinker usage

## Input and conversion

Use a clean ELF executable. Place its bundled ELF modules in `sce_module/`, `sce_modules/`, or `prx/` beside the input executable. `prx/` can coexist with either `sce_module/` or `sce_modules/`. Both `sce_module/` and `sce_modules/` present, or all three absent, is an error.

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
| `--module-dir <dir>`          | Relink the ELF modules of a directory relative to the input, in addition to `sce_module`/`sce_modules`/`prx`; the outputs keep the relative path, and each module is initialized when the title loads it with `sceKernelLoadStartModule` (for example Unity plugins in `Media/Plugins`). Repeat for multiple directories.        |
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

Use `sce_modules/` or `prx/` instead of `sce_module/` if that is the input directory name. Relinker preserves each module's directory under `app0/` and prints its exact path. Place app resources in `app0/` separately. Copy the built system libraries from `build/core/libs/libs/*.prx` into `libs/`; use libraries built for the target OS. A custom `--rpath` changes the system library location.

Use the generated files printed as `Guest module:` for bundled title modules. `libs/` is for AnyPS5 system libraries, not the original PS5 `.prx` files. Placing an original PS5 module in `libs/` on Windows makes Windows try to load it as a DLL and can fail with error 193 (not a valid Win32 application).

On Windows, direct memory (`sceKernelAllocateDirectMemory`, up to 13824 MiB per title) is committed in full when the title allocates it, not when its pages are first used. The system commit limit (installed memory plus page file size, the second value of Committed in Task Manager) must cover it together with all other committed memory. Otherwise the allocation throws `create direct memory backing of 0x<n> bytes (<m> MiB)` with the Windows error; enlarge the page file or close other applications.

Linux:

```sh
chmod +x app.elf
./app.elf
```

Windows PowerShell:

```powershell
.\app.exe
```

### System fonts

Games that open the console's system font sets (`sceFontOpenFontSet`) need font files in an `anyps5-fonts/` directory beside the output executable; set `ANYPS5_SYSTEM_FONTS` to use another directory. Files dumped from the console are used under their own names (`SST-Roman.otf`, `SST-Bold.otf`, `SSTJpPro-Regular.otf`, ...). Without them, these openly licensed substitutes are used when present: `NotoSans-{Light,Regular,Medium,Bold}.ttf` and `NotoSans-{LightItalic,Italic,MediumItalic,BoldItalic}.ttf` (Latin and Vietnamese), `NotoSansMono-{Light,Regular,Medium,Bold}.ttf` (typewriter), `NotoSansThai-{Light,Regular,Medium,Bold}.ttf` (Thai) and `NotoSansCJK-{Light,Regular,Medium,Bold}.ttc` (Japanese and Chinese). Without either, opening a system font set fails and the game shows no text in those fonts.

### GPU selection

The game runs on the first Vulkan 1.1 device with graphics and compute queues and swapchain presentation, preferring a discrete GPU over an integrated one. Set `ANYPS5_GPU` to a part of a device name, compared without regard to case, to run on another device; the names are printed at start-up in the `Physical device candidate` lines. When no usable device contains the text, the start fails and the error lists the device names.

## Exit codes

`0`: conversion succeeded. `1`: invalid arguments. `2`: conversion failed; the error is printed to stderr. With `--autorun`, successful conversion returns the launched application's exit code.

## Import audit

[`tools/import_audit.py`](../../tools/import_audit.py) lists the system functions a converted game imports and whether the built libraries provide them, before the game is launched. It needs Python 3 and nothing else.

```sh
relinker --registry source/input.elf app.elf
python3 tools/import_audit.py app.registry.json --libs build/core/libs/libs --modules source/sce_module
```

`--registry` writes `app.registry.json` beside the output. `--libs` is the directory the `libs` target fills ([build instructions](../dev/BUILD.md)); the exports are read from the built `.prx` files, so the result matches what the loader finds. Imports are counted once per NID and library, and every reference lands in exactly one class:

| Class | Meaning |
|-------|---------|
| `implemented` | A built library exports the NID and its function is not a throwing stub. |
| `stub` | Exported, but the whole function calls `NotImplemented_nid_no_patch`: the game loads and throws when it calls it. |
| `absent` | No built library exports the NID: the loader fails. |
| `module` | The import names a file found in `--modules` (the title's own library); its exports are not checked. |

The report also lists needed libraries that have no file in `--libs` or `--modules`, and imports exported only by a library other than the one they name; those resolve on Linux and can fail on Windows.

| Option | Effect |
|--------|--------|
| `--libs <dir>` | Built `.prx` directory. Required; repeat for several. |
| `--modules <dir>` | Directory of the title's own modules; repeat for several. |
| `--source <dir>` | `core/libs/prx` tree that is searched for throwing stubs. Defaults to this repository's. |
| `--names <file>` | NID and name per line, as in `aerolib.csv` from [`tools/nid_names.py`](../../tools/nid_names.py); names the absent imports. Never downloaded; a name that does not hash to its NID is marked. |
| `--json <file>` | Full result, stamped with the repository's commit and whether its tree was modified. |

Exit codes: `0` nothing blocks loading, `1` there are `absent` imports or needed libraries without a file, `2` an input could not be read; the message names the file and the value.

The registry lists the imports of the executable, not of its bundled modules.
