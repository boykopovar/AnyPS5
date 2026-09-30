# About

Tool for automatic executables porting to Linux and Windows.

Includes a [relinker](core/relinker) that converts executable to the target system's native format and implementations of [system prx libraries](core/libs/prx) suitable for dynamic linking. No emulation or separate runtime process.



## Status

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/) [![shaders](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/)

<sub>* System libraries: percentage of the functions known to the project so far (declared in [core/libs/prx](core/libs/prx)), not of every PS5 system function. The total grows as more functions are declared.</sub>

[List of verified games](docs/user/COMPATIBILITY.md)

Dreaming Sarah (2D platformer) runs at a stable 60 fps on a GTX 1050 Ti / i5-7500 3.4GHz.

Unsupported or unexpected states strictly throw `std::runtime_error`. `what()` is printed to stderr and the process terminates.

The [shader recompiler](core/shader/recompiler/Recompiler.cpp) successfully produces SPIR-V (validated via [Spirv-Tools](3rdparty/SPIRV-Tools) when built with `ANYPS5_ENABLE_SPIRV_TOOLS`).

[Technical debt of the project](docs/dev/TechnicalDebt.md), [code style conventions](docs/dev/CONVENTIONS.md), [contributing](CONTRIBUTING.md)

## Build

Third-party code is a submodule under `3rdparty/`, built from source instead of being taken from the system. FFmpeg is the exception: configure downloads a prebuilt package for the current submodule revision, so it needs network access.

The commands below need CMake 3.20 or later and Ninja.

```
git submodule update --init
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build
cmake --build build --target libs
ctest --test-dir build --output-on-failure
```

`libs` is not part of the default target. It must be built separately to produce the patched system libraries in `build/core/libs/libs`.

| CMake option | Default | Effect |
|---|---|---|
| `BUILD_TESTING` | `OFF` | Build and register tests. Python 3 is optional; without it some relinker tests are not registered. |
| `ANYPS5_ENABLE_SPIRV_TOOLS` | `OFF` | Validate and optimize recompiled shaders with [Spirv-Tools](3rdparty/SPIRV-Tools). |
| `APS5_ENABLE_TIMING_LOG` | `OFF` | Log frame timings. |

The relinker uses only the C++20 standard library and should build with any conforming compiler.

[libc.prx](core/libs/prx/libc) implementations contain compiler-specific code. Linux builds work with GCC; on Windows, MinGW-w64 GCC 15.2.0 (`winlibs-gcc15`, `x86_64-ucrt-posix-seh`) is currently required.

The project targets maximum compiler portability. Support for additional compilers will be addressed after the first successful game launch.

## Usage

`relinker` converts a PS5 executable to a native Linux ELF or Windows PE image and replaces its references to system functions with calls into the [system prx libraries](core/libs/prx). Nothing is emulated and no separate runtime process is started.

```
build/core/relinker/relinker [options] <input.elf> <output.elf>
```

The Windows build produces `build/core/relinker/relinker.exe`.

The input executable must be accompanied by a `sce_module` or `sce_modules` directory. Every ELF file in it is converted and written as a guest module under `app0/<directory name>/` next to the output.

| Option | Effect |
|---|---|
| `--windows` | Write a Windows PE image instead of a Linux ELF image. |
| `--windows-gui` | Mark the image as a GUI application; requires `--windows`. |
| `--windows-diagnostics` | Print dependency load diagnostics; requires `--windows`. |
| `--to-intel` | Lower supported AMD-only instructions in the executable and the bundled `sce_module`/`sce_modules` PRX files. Unsupported instructions or stub jumps outside the x86-64 relative branch range produce an error. |
| `unused-filter=0\|1\|2` | Unused-NID filtering level: `0` off, `1` filtering of non-PLT references, `2` strict reachability filtering with PLT compaction. |
| `--registry` | Also write `<output stem>.registry.json` beside the output, listing the external PRX references. |
| `--rpath <path>` | Library search path stored in the output. Defaults to `$ORIGIN/libs`. |
| `--lazy-binding` | Resolve imported functions lazily. Guest modules require eager binding, so this needs `--skip-sce-module`. |
| `--skip-syscall-check` | Skip the forbidden-syscall scan of the code section. |
| `--skip-sce-module` | Do not convert `sce_module`/`sce_modules`; only for a title that runs without these modules. |
| `--exclude-sce-module <file>` | Do not convert the named file in `sce_module`/`sce_modules`. Repeatable; conflicts with `--skip-sce-module`. |
| `--autorun` | Run the output executable once the relink finishes; on Linux it is given the execute permission first. |

The expected runtime layout, relative to the output executable:

```
<output executable>
libs/
    *.prx
app0/
    <game resources>
    <sce_module directory>/
        <module>.guest.prx
```

`build/core/libs/libs/*.prx` belongs in `libs/`. The game resources are not produced by the relinker and must be placed in `app0/` separately. The layout is printed after every successful run, together with the number of external PRX references.

## Compatibility

See the [game compatibility list](docs/user/COMPATIBILITY.md) for tested games and known issues.

## Input mapping

SDL-mapped game controllers are supported, including analog sticks and triggers. Keyboard and mouse controls can be configured with an `anyps5-input.ini` file. See [input mapping](docs/user/INPUT_MAPPING.md) for the supported devices and configuration format.

## Disclaimer

This project is intended for interoperability, research, preservation, and compatibility purposes. It does not include, distribute, or require copyrighted software, firmware, cryptographic keys, or proprietary libraries. Users are responsible for ensuring that any binaries used with this project are obtained and used in accordance with applicable laws and their respective license terms.

## License

This project is licensed under the GNU General Public License version 2 only.
