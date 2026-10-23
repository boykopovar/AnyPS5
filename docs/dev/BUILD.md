# Build

## Relinker only

The relinker can be built without initializing submodules or configuring SDL, Vulkan, FFmpeg, FreeType or the shader recompiler. It requires CMake, a C++20 compiler and a build tool. Python 3.9 or newer enables the Python regression tests.

```sh
cmake -S . -B build-relinker -G Ninja -DCMAKE_BUILD_TYPE=Release -DANYPS5_RELINKER_ONLY=ON -DBUILD_TESTING=ON
cmake --build build-relinker --parallel
ctest --test-dir build-relinker --output-on-failure
```

Python must be found during CMake configuration for Python tests to be registered. List registered tests with `ctest --test-dir build-relinker -N` and look for `input_magic`, a Python-backed relinker test. If automatic discovery fails, add `-DPython3_EXECUTABLE="C:/path/to/python.exe"` to the configure command, using the path to your Python interpreter, then list the tests again.

This mode builds the conversion tool and its tests on Linux, Windows and macOS, including Apple Silicon. macOS uses AppleClang from the Xcode command-line tools (`xcode-select --install`); CMake and Ninja come from Homebrew (`brew install cmake ninja`). Windows uses MinGW-w64 GCC with C++20 support and accepts distributions with static or shared runtimes; the full build also requires the exception support described below. The executable is `build-relinker/core/relinker/relinker` (`relinker.exe` on Windows with Ninja).

The output is x86-64: Linux ELF by default, Windows PE with `--windows` or Mach-O with `--macos` ([usage](../user/USAGE.md#input-and-conversion)). Converted games need system libraries built for the target OS and a compatible x86-64 host. This mode does not build those libraries, and releases ship them for Linux and Windows only. Tests inspect all three output formats; execution checks run only on their compatible hosts.

Use a separate build directory for the full build.

## Fuzzing

`ANYPS5_BUILD_FUZZERS` builds `relinker_conversion_fuzzer`, a libFuzzer target that runs the whole conversion on its input, as the web converter does. It requires `ANYPS5_RELINKER_ONLY`, Clang on Linux and the compiler-rt sanitizer runtimes (`libclang-rt-18-dev` on Ubuntu 24.04). Every target is built with AddressSanitizer and UndefinedBehaviorSanitizer, without `-fsanitize=function`: that check reads a signature before the target of every indirect call and faults on the generated code the codegen tests call. The first input byte selects the target system (modulo 3: Linux, Windows, macOS), bit 2 `--to-intel` and the byte shifted right by 3, modulo 3, the `unused-filter` level; the remaining bytes are the executable. A conversion that throws a `std::exception` is a rejected input; a sanitizer report, an abort or a timeout is a finding.

```sh
cmake -S . -B build-fuzz -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DANYPS5_RELINKER_ONLY=ON -DANYPS5_BUILD_FUZZERS=ON -DBUILD_TESTING=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build-fuzz --target relinker_conversion_fuzzer
python3 tools/relinker_fuzz_seeds.py build-fuzz/fuzz-corpus
build-fuzz/tests/relinker_conversion_fuzzer build-fuzz/fuzz-corpus -fork=2 -max_total_time=1200 -timeout=20
```

`relinker_fuzz_seeds.py` writes the executables built by the relinker's Python tests as seeds, one per target system. With `BUILD_TESTING=ON`, CTest runs 20000 inputs from an empty corpus as `relinker_conversion_fuzzer`. Findings are written to the working directory as `crash-*`, `timeout-*` or `oom-*`; reproduce one with `build-fuzz/tests/relinker_conversion_fuzzer <file>`.

## Full build

```sh
git submodule update --init --recursive
```

## Requirements

- x86-64, Git, CMake 3.22.1 or newer, Ninja, C++20.
- Linux display refresh queries require SDL's XRandR support (`libxrandr-dev` on Debian/Ubuntu; `libXrandr-devel` on Fedora). Bundled SDL configuration fails without it. Keep `SDL_X11_XRANDR` enabled.
- Linux: GCC, G++, binutils. SDL's X11 backend requires X11 and Xext development headers (`libx11-dev` and `libxext-dev` on Debian/Ubuntu). SDL's Wayland backend requires the Wayland, xkbcommon and EGL development files (`libwayland-dev`, `libxkbcommon-dev` and `libegl-dev` on Debian/Ubuntu; `wayland-devel`, `libxkbcommon-devel` and `mesa-libEGL-devel` on Fedora), and `libdecor-0-dev` (`libdecor-devel`) for window decorations on GNOME. SDL loads both backends at run time and prefers Wayland when `WAYLAND_DISPLAY` is set; `SDL_VIDEODRIVER=x11` selects X11 (XWayland). Titles that bundle their own SDL read `SDL_VIDEODRIVER` too, and some of them fail to start with `x11`. Without the Wayland files configuration still succeeds, and the build uses X11 only. Audio output needs SDL's PipeWire, PulseAudio or ALSA backend, built from their development files (`libpipewire-0.3-dev`, `libpulse-dev` and `libasound2-dev` on Debian/Ubuntu; `pipewire-devel`, `pulseaudio-libs-devel` and `alsa-lib-devel` on Fedora). SDL loads them at run time. Without any of them SDL keeps only its silent dummy and disk drivers, so configuration fails.
- Fedora: if CMake reports `Could NOT find Threads`, install `glibc-static` and `libstdc++-static` with `sudo dnf install glibc-static libstdc++-static`, then rerun configuration. The project enables static executable linking, and FFmpeg's thread check needs these libraries.
- Windows: x86-64 MinGW-w64 GCC with POSIX threads. The compiler must provide DWARF exceptions with `-fno-asynchronous-unwind-tables`, or SEH exceptions for System V ABI functions; CMake checks these capabilities. WinLibs GCC 15.2.0 (`x86_64-ucrt-posix-seh`, release [`15.2.0posix-14.0.0-ucrt-r7`](https://github.com/brechtsanders/winlibs_mingw/releases/tag/15.2.0posix-14.0.0-ucrt-r7)) uses DWARF. [MSYS2 UCRT64 GCC](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-gcc) 16.2.0-6 uses SEH. Add the selected toolchain's `bin` directory to `PATH` before configuring.
- Windows antivirus software can block or quarantine the executables that the relinker tests create and run, which fails `optional_plt`, `empty_tls`, `windows_address_space`, `windows_bss`, `windows_fixed_base` and `windows_icon`. CTest sets `TMPDIR`, `TEMP` and `TMP` of the relinker tests to `core/relinker/test-temp` in the build directory, so setting them before running `ctest` does not move these files; add that directory to the scanner's exclusions instead.
- Bitdefender Advanced Threat Defense hooks Windows calls that `sceKernelRaiseException` makes while its target thread is suspended. When the target is suspended inside ntdll's exception dispatch while it holds an ntdll lock, the hook waits for that lock and `guest_raise_exception` hangs until its timeout. A scanning exception for the build directory does not prevent it; add `guest_raise_exception_tests.exe` to the Advanced Threat Defense exceptions.
- FFmpeg binaries are downloaded during configuration unless `FFMPEG_PREBUILT_DIR` is set. With the WinLibs CMake, the download fails with status 60 (`SSL peer certificate or SSH remote key was not OK`) unless `SSL_CERT_FILE` names a CA bundle, for example `C:\Program Files\Git\ucrt64\etc\ssl\certs\ca-bundle.crt` from Git for Windows 2.56 and later (`mingw64` instead of `ucrt64` before 2.56), as in CI.
- Windows: libusb's `version_describe.h` is generated by `build-aux/gen-describe.ps1`, which CMake runs with `-ExecutionPolicy Bypass`. An execution policy set by Group Policy (`MachinePolicy` or `UserPolicy` in `Get-ExecutionPolicy -List`) takes precedence over that switch ([about_Execution_Policies](https://learn.microsoft.com/powershell/module/microsoft.powershell.core/about/about_execution_policies?view=powershell-5.1)); with `Restricted` or `AllSigned` the build stops at `version_describe.h` with `running scripts is disabled on this system`. The script only starts `build-aux/gen-describe.sh` with the `bash.exe` of Git for Windows, so a `.cmd` file that runs the shell script directly can replace PowerShell for this step without changing the policy. CMake passes it the `.ps1` path as the fifth argument and the source and output paths as the sixth and seventh; configure with `-DLIBUSB_POWERSHELL_EXECUTABLE=<path of the .cmd file>`:

  ```bat
  @"C:\Program Files\Git\bin\bash.exe" "%~dpn5.sh" %6 %7
  ```

## Web converter releases

The Release workflow builds the web converter and the Linux and Windows libraries from the same commit. It tests the converter and publishes its frontend and WASM files as `web-<tag>.zip`, with the tag and commit recorded in `web-release.json`.

Pages downloads the converter and both library archives from one release selected through GitHub's latest-release API. `release.json` records the same tag and commit. Missing, corrupt or mismatched assets fail the Pages job before deployment. Releases made before the web asset was added need a new release; Pages does not combine them with a converter built from `main`.

Changes to the converter or its frontend reach Pages through a release. Progress reports still come from `main`. For local development, build `relinker_web` with Emscripten and run `python3 tools/test_web.py --build build-wasm`. To stage a complete published release without compiling, run `python3 tools/build_web.py --build build-wasm --release`; `--libraries` is an alias for this mode.

## Commands

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build --parallel
cmake --build build --target libs --parallel
```

`libs` is a custom target: every library under [core/libs/prx](../../core/libs/prx) is built with
`EXCLUDE_FROM_ALL`. With the default `BUILD_TESTING=OFF`, the default build does not produce them,
so the explicit `--target libs` step is required. With `BUILD_TESTING=ON`, the default full build
also builds `libs` through the `patched_libraries_tests` dependency. Titles load the patched `.prx`
files from `build/core/libs/libs`; running a title after a library change without rebuilding `libs`
therefore tests the previous binaries and can show no effect at all.

[Relinker usage and runtime layout](../user/USAGE.md).

## Windows setup and troubleshooting

The full build also requires Python 3 for glslang and Git for Windows with Bash for libusb's version-header generation. A minimal Git installation without Bash is insufficient for the full build.

For MSYS2, install `mingw-w64-ucrt-x86_64-gcc` with `pacman -S mingw-w64-ucrt-x86_64-gcc` and use `C:\msys64\ucrt64\bin` in PowerShell. Adjust the prefix for a Scoop installation of MSYS2. Use a separate build directory for each toolchain, and select both `gcc` and `g++` from the same installation.

The [Windows MSYS2 workflow](../../.github/workflows/windows-msys2.yml) builds and tests the full runtime nightly at 08:23 UTC and can be started manually with `workflow_dispatch`. The regular Windows build uses WinLibs.

Scoop's standalone GCC 15.2.0 package cannot build the runtime: disabling SEH selects SjLj exceptions, and its SEH backend cannot compile System V ABI catch handlers. Use MSYS2 UCRT64 or a DWARF-capable WinLibs distribution. This restriction does not apply to relinker-only builds.

In PowerShell, prepend the supported WinLibs toolchain and Git for Windows to the current session's `PATH`. Adjust the paths for the installed locations; a Portable Git installation can be used in place of `C:\Program Files\Git`.

```powershell
$env:PATH = "C:\tools\winlibs\mingw64\bin;C:\Program Files\Git\cmd;C:\Program Files\Git\usr\bin;$env:PATH"
```

If configuration reports `Could NOT find Python3` or selects an unusable interpreter, pass the absolute path to a working Python 3 executable:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DPython3_EXECUTABLE="C:/path/to/python.exe"
```

The documentation runs the Python tools as `python3`. With the python.org installer for Windows, which provides `python.exe` and the `py` launcher but no `python3.exe`, `python3` resolves to the Microsoft Store alias in `WindowsApps`, which prints `Python was not found` and exits with code 9009 (49 in Git Bash). Installs that provide `python3.exe`, such as the Python Install Manager, are not affected. In that case run these commands with `python` or `py -3`, for example `py -3 tools/check_conventions.py --base origin/main`.

If `gen-describe.ps1` fails while generating libusb's `version_describe.h`, check the Bash path printed in the build output. A WindowsApps or System32 `bash.exe` can launch WSL and fail with `WSL_E_DEFAULT_DISTRO_NOT_FOUND` when no distribution is installed. Install Git for Windows with Bash, or use Portable Git, and put its `cmd` and `usr\bin` directories on `PATH` before retrying the build. Use Bash and `cygpath.exe` from the same Git installation.

If the build stops with `fatal error: opening dependency file ...\*.cpp.obj.d: No such file or directory` for a few sources with long names, the path of an object or dependency file under `build` is at or near the 260-character Windows limit. A checkout whose path was 92 characters failed this way with `LongPathsEnabled` set to 1 in the registry; the same tree at `C:\a5` built. Clone the repository to a short path such as `C:\a5` and configure in a new build directory: a CMake build tree stores absolute paths, so moving an existing `build` directory does not help.

After `cmake --build build --target libs --parallel` succeeds, copy the toolchain's runtime DLLs beside the patched libraries:

```powershell
$toolchainBin = "C:\tools\winlibs\mingw64\bin"
foreach ($runtimeName in @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
    Copy-Item -LiteralPath (Join-Path $toolchainBin $runtimeName) -Destination "build/core/libs/libs"
}
```

When preparing a title's `libs/` directory, copy these DLLs with the patched `.prx` files. The Windows runtime loader does not search `PATH` for these dependencies; missing DLLs can cause error 126 even when the `.prx` is present. See [runtime layout](../user/USAGE.md#runtime-layout).

## CMake flags

Project switches accept `ON` or `OFF`:

| Flag                             | Default | Effect                                           |
|----------------------------------|---------|--------------------------------------------------|
| `-DBUILD_TESTING=ON`             | `OFF`   | Build and register tests.                        |
| `-DANYPS5_RELINKER_ONLY=ON`      | `OFF`   | Build only the relinker and its tests, without third-party dependencies. |
| `-DANYPS5_BUILD_WASM=ON`         | `OFF`   | Build only the browser relinker (`relinker_web` target). Requires `emcmake` from Emscripten and `BUILD_TESTING=OFF`. |
| `-DANYPS5_ENABLE_SPIRV_TOOLS=ON` | `OFF`   | Enable SPIR-V validation and optimization.       |
| `-DANYPS5_ENABLE_ASAN=ON`       | `OFF`   | Build with AddressSanitizer on Linux and move the guest arena out of ASan's shadow gap. |
| `-DANYPS5_ENABLE_TSAN=ON`       | `OFF`   | Build with ThreadSanitizer on Linux and link executables dynamically. Cannot be combined with `-DANYPS5_ENABLE_ASAN=ON`. |
| `-DANYPS5_USE_SYSTEM_DEPS=ON`    | `OFF`   | Resolve SDL2, freetype, Vulkan-Headers, SPIRV-Headers, glslang and SPIRV-Tools from the system instead of the submodules. |
| `-DAPS5_ENABLE_TIMING_LOG=ON`    | `OFF`   | Compile frame timing logging.                    |
| `-DAPS5_ENABLE_TRACY=ON`         | `OFF`   | Build the [Tracy](#tracy) profiler client into the agc driver. |
| `-DAPS5_AGC_CREATE_LOG=OFF`      | `ON`    | No effect: `sceAgcCreateShader` logs nothing on success. |
| `-DAGC_BUILD_VISUAL_TEST=ON`     | `OFF`   | Build the standalone AGC SPIR-V visual test.     |
| `-DANYPS5_REQUIRE_CPU_LEAD_TEST=ON` | `OFF` | With `BUILD_TESTING=ON`, also register `agc_driver_sparse_dma_buf_cpu_lead`, which runs `agc_driver_sparse_dma_buf_import_tests --require-cpu-lead` and fails instead of skipping when the nonzero-lead dma-buf CPU publish is not exercised, such as on Windows or without an accessible `/dev/udmabuf`. |
| `-DANYPS5_LIBRESSL_ORACLE=ON`    | `OFF`   | With `BUILD_TESTING=ON`, download LibreSSL 2.5.4 from ftp.openbsd.org at build time, check its SHA-256 and register `guest_libressl_oracle`, which compares `libSceLibreSsl` and `libSceLibreSsl3` exports with it and prints the cases and differences per function. Without the flag nothing is downloaded. |

GCC and Clang compile libc with `-ffp-contract=off` whatever the target CPU. Toolchains that default to x86-64-v3, such as GCC 15 on Ubuntu 26.04, would otherwise fuse `a * b + c` into FMA instructions, which changes the last bit of math results that must match the console.

The [Sanitizers workflow](../../.github/workflows/sanitizers.yml) builds the full runtime on Ubuntu with `-DANYPS5_ENABLE_ASAN=ON` and UBSan (`-fsanitize=undefined -fno-sanitize=vptr`) nightly at 03:41 UTC, runs every test and can be started manually with `workflow_dispatch`. It builds only the `F16ToF32Unpack`, `F64FmaSubnormals` and `RegistrationPrepareShutdown` AGC execution tests, like the Windows MSYS2 workflow. A test that fails because of the sanitizers themselves, or because of a bug whose fix is still open, is listed with the reason in [sanitizer-known-failures.txt](../../.github/sanitizer-known-failures.txt), one test per line with its sanitizers and the reason. The job summary, written by `tools/sanitizer_report.py`, separates unexpected failures from known ones and names listed tests that passed or did not run, so their entries can be removed. To repeat a run locally, set `ASAN_OPTIONS=detect_leaks=0` and `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`, so that undefined behavior fails the test.

Build configuration parameters:

| Flag                                   | Value                                                              |
|----------------------------------------|--------------------------------------------------------------------|
| `-DCMAKE_BUILD_TYPE=Release`           | `Debug`, `Release`, `RelWithDebInfo`, or `MinSizeRel`.             |
| `-DCMAKE_C_COMPILER=gcc`               | C compiler name or absolute path.                                  |
| `-DCMAKE_CXX_COMPILER=g++`             | C++ compiler name or absolute path.                                |
| `-DCMAKE_C_COMPILER_LAUNCHER=ccache`   | Optional C compiler cache; requires `ccache`.                      |
| `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache` | Optional C++ compiler cache; requires `ccache`.                    |
| `-DFFMPEG_PREBUILT_DIR=<path>`         | Unpacked FFmpeg package for the target platform; empty by default. |
| `-DANYPS5_AGC_EXECUTION_TESTS=<names>` | Semicolon-separated file names, without `.cpp`, of the `core/libs/prx/libSceAgcDriver/tests/execution` tests to build. Every other execution test is built from a stub that prints `skipped, this build leaves the test out with ANYPS5_AGC_EXECUTION_TESTS` and exits 77. A name with no source there fails the configure. Empty by default, which builds them all. |

SDL and FreeType settings forced by the root `CMakeLists.txt` cannot be overridden with `-D`.

With `ANYPS5_USE_SYSTEM_DEPS=ON` those libraries come from the system (on Debian and Ubuntu: `libsdl2-dev`, `libfreetype-dev`, `libvulkan-dev`, `spirv-headers`, `glslang-dev`, `glslang-tools` and `spirv-tools`). A missing package, Vulkan-Headers older than 1.4.310 (the agc driver uses `VK_KHR_maintenance8`), or an SDL2 without the HIDAPI joystick driver (sdl2-compat over SDL3, or a build with HIDAPI disabled, which breaks the pad and disk-audio tests), fails the configure with an error and never falls back to the submodule. The forced SDL and FreeType settings from the root `CMakeLists.txt` do not apply with the flag ON: the system SDL2 comes with whatever its distribution enabled. The prx libraries are shared objects, so SDL2 is linked from the system as a shared library; the default build keeps its own static SDL2. `ffmpeg-core`, `libjpeg-turbo`, `stb`, `VulkanMemoryAllocator`, `LibAtrac9`, `ogg` and `vorbis` always stay bundled and still need `git submodule update --init`.

## HTTP locale tests

The Linux HTTP URI locale regression needs an ISO-8859-1 locale. With tests enabled, CMake uses host `localedef` and installed locale sources to generate a private `en_US.ISO8859-1` under the HTTP build directory. This runs from the checked-out PR tree, including when CI dispatches the workflow from `main`, and changes no system locales. `-DANYPS5_HTTP_TEST_LOCALE_DIR=<directory>` overrides generation. Only `guest_http_uri_locale` receives the private directory through `LOCPATH`. If `localedef` or its source data is unavailable, configuration reports it and leaves the test to probe installed locales. Without an effective Latin-1 locale the test explicitly skips; the CI skip audit rejects that loss of coverage. Windows requires `.1252`.

## ThreadSanitizer

A ThreadSanitizer build runs its tests with `TSAN_OPTIONS=suppressions=$PWD/tools/tsan-suppressions.txt`, which silences reports from Mesa's lavapipe, the LLVM it compiles shaders with, and PulseAudio: those system libraries are not instrumented, so ThreadSanitizer cannot see their locking. Guest memory written by the emulated GPU while a guest thread polls it, such as labels and fences, is shared without C++ atomics on purpose, as on the console, so ThreadSanitizer also reports those accesses.

## Quick syntax checks

After configuring a build directory, use `tools/check_compile.py` to check the given source files. The tool reads their compile commands from `compile_commands.json` or Ninja, preserving compilation options while dropping object and dependency output options. It handles `-MF`, `-MT`, `-MQ` and `-MJ` with separate or attached values so that unused output options do not cause false failures under `-Werror`.

```sh
python3 tools/check_compile.py --build build-relinker core/relinker/main.cpp
```

Real compile errors still fail. Syntax checks do not replace a full build and test run.

## Repeated Vulkan validation

`tools/vulkan_validation_gate.py` builds two pinned clean source worktrees into a new output directory and alternates the same focused CTests between them. Execution currently supports POSIX hosts only. Linux hardware runs establish GPU behaviour; macOS synthetic tool tests do not establish native Linux/Windows or GPU correctness. Windows is refused before creating output.

```sh
python3 tools/vulkan_validation_gate.py \
  --baseline /path/to/main FULL_MAIN_COMMIT \
  --candidate /path/to/change FULL_CHANGE_COMMIT \
  --output /path/to/new-validation-run \
  --target agc_driver_recorder_tests --target libs \
  --tests '^agc_recorder_completion_labels$' --gpu 'expected device name' \
  --repeat 20 --runtime core/libs/libs
```

Both worktrees must have the expected full commit ID and no tracked or untracked source changes. Output must be outside both worktrees and must not already exist. Each build uses Ninja, Release and `BUILD_TESTING=ON`; shared cache options can be supplied as `--cmake-arg=-DNAME=VALUE`. Test targets are explicit; the tool does not infer them. Configure and build logs, selected CTest commands, compiler/build configuration, commit IDs and hashes of selected command files and explicit build-relative `--runtime` files/directories are retained. Other dynamically loaded libraries and ignored source files are not inventoried. A clean commit alone does not identify an arbitrary existing binary, so existing build directories are not accepted.

Each iteration runs serially with `ANYPS5_REQUIRE_VULKAN=1`, `ANYPS5_NO_SHADER_CACHE=1`, `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` and the requested `ANYPS5_GPU`. The tool rejects declared test properties that override those values or the recorded `VK_ICD_FILENAMES`, `VK_DRIVER_FILES` and `VK_LAYER_PATH` values. The GPU name is a selection request, not an independent hardware measurement. Every iteration retains verbose output and JUnit, with CTest's passed/failed output-size limits disabled so diagnostics near either end of a long output are retained. Missing, disabled or skipped selected tests, test failures and the exact missing-layer phrase fail the gate, even if CTest returns zero.

The gate compares VUID and synchronization-hazard IDs per test across all baseline and candidate iterations. A new candidate ID fails; occurrence counts are reported without treating a count increase as a new violation. Existing baseline diagnostics remain visible and do not establish a clean validation run. Parsing covers standard `Validation Error/Warning:` and `VUID/SYNC-HAZARD (ERROR/WARNING / ...)` messages in JUnit test-body output, not arbitrary application formats or verbose CTest configuration text. Retain the logs for manual review of other formats. Source identity and selected artifact hashes are checked again after testing.

`report.json` records the comparison and execution order. Exit 0 means the comparison completed with no detected regression, 1 means a test/coverage/layer or new diagnostic failure, 2 means setup, attribution or incomplete evidence, and 130 means interrupted. `--test-timeout` supplies CTest's default timeout; explicit test timeout properties still apply. `--timeout` bounds each configure/build/CTest command. The active POSIX process group is killed on completion, timeout or interruption. Repetitions report observed outcomes; they do not prove absence of intermittent faults or statistical significance.

## Write watch tests

The macOS write watch can be built and tested separately, without submodules or the prx libraries:

```sh
cmake -S core/libs/tests/write_watch -B build/write-watch -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/write-watch
ctest --test-dir build/write-watch --output-on-failure
```

With `CMAKE_OSX_ARCHITECTURES=x86_64` Apple silicon runs the tests through Rosetta, which is how relinked titles run; without it they run natively. The full `BUILD_TESTING` build on macOS also registers them.

## Tracy

`-DAPS5_ENABLE_TRACY=ON` builds the [Tracy](https://github.com/wolfpld/tracy) 0.14.1 client from `3rdparty/tracy` as a shared library next to the patched `.prx` files (`libTracyClient.dll`, `libTracyClient.so.0.14.1` on Linux); copy it with them. Every `PerformanceTimer` scope of libSceAgcDriver and libSceVideoOut is a zone, every presented flip ends a frame, and the queue workers and the presenter are named threads. Connect the Tracy 0.14.1 profiler to `127.0.0.1`: the client listens only on localhost and records only while a profiler is connected. Sampling, system tracing, call stacks, code transfer and Tracy's crash handler are disabled. Without the flag nothing of Tracy is built.

On Windows MinGW frees `thread_local` objects before their destructors run, so the Tracy client is built without `thread_local` destructors and never destroys its event queue: a thread that emitted a zone and ended keeps its event blocks (2 MiB each) until the process exits.

## Compute pipeline replay

Build `agc_compute_pipeline_replay` in the full build with `-DANYPS5_ENABLE_SPIRV_TOOLS=ON`; replay validates the captured module before calling Vulkan. Set `APS5_DUMP_COMPUTE_PIPELINES` to a dedicated empty directory before starting a title to save each uncached compute pipeline's final SPIR-V, descriptor layouts, push constant ranges, specialization values, subgroup size and creation flags before compilation. Files are named `compute-<identity>/capture.aps5cp`; identical records are deduplicated. The pipelines of the start-up f32 denormal flush probe are not captured. At 128 records or 256 MiB in total, capture turns itself off for the rest of the run after a `[compute-capture] optional capture disabled` line and the title keeps running; a single record over 36 MiB (at most 8,388,608 SPIR-V words, or 32 MiB of shader code) is an error. Capture files contain shader code and device metadata, but no guest buffers or textures; review them before sharing.

Set `APS5_DUMP_COMPUTE_MIN_WORDS` to capture only modules at or above that SPIR-V word count, for example `100000` when investigating large NVIDIA compilations. The default is `0` (all modules); values outside `0` through `8388608` fail. The filter applies only when capture is enabled.

```sh
cmake --build build --target agc_compute_pipeline_replay
build/core/libs/prx/libSceAgcDriver/agc_compute_pipeline_replay capture.aps5cp --cache-out pipeline.cache --output fresh.json
build/core/libs/prx/libSceAgcDriver/agc_compute_pipeline_replay capture.aps5cp --cache-in pipeline.cache --output imported.json
```

Use `APS5_PIPELINE_STATS=1 agc_compute_pipeline_replay capture.aps5cp --inspect --output inspection.json` to inspect the captured pipeline without dispatching it. This requires `VK_KHR_pipeline_executable_properties`, requests statistics for this replay, and prints the driver's executable and counter names, values and descriptions to stderr. Available counters depend on the driver. The JSON report marks `mode` as `inspection`, sets `pipeline_statistics` to true, records the effective `replay_pipeline_flags`, and sets `milliseconds` to null. The captured identity and original flags are retained. Inspection output is not a compilation benchmark. Replay without `--inspect` continues to reject enabled statistics or captures carrying the statistics flag. `--inspect` cannot be combined with `--write-fixture`.

On Windows use `agc_compute_pipeline_replay.exe` and prepend `build/core/libs/libs/unpatched` to the process `PATH`. `ANYPS5_GPU` selects a device by name as for the driver. The tool uses the driver's compute pipeline creation path, without recording commands, dispatching work or importing guest memory. Compile-only devices also enable supported partially-bound, variable-count and runtime-array descriptor features to replay captured layouts; ordinary runtime devices are unchanged. Binding flags are preserved independently of update-after-bind. Replay accepts the unconditional Vulkan 1.0 capabilities listed in the pinned [Vulkan registry](https://github.com/KhronosGroup/Vulkan-Headers/blob/2fa203425eb4af9dfc6b03f97ef72b0b5bcb8350/registry/vk.xml); other capabilities must be enabled by the device target. Non-Vulkan capabilities such as Kernel and unknown capabilities fail before pipeline creation. Unsupported features, non-highest variable bindings and counts exceeding layout support fail explicitly. Unsupported inputs and mismatched cache vendor, device or UUID fail explicitly. Output files must not already exist.

Each invocation creates one pipeline. With no `--cache-in`, its application pipeline cache starts empty; `--cache-in` imports a raw cache previously written by `--cache-out` (at most 64 MiB), and `--cache-out` exports the resulting cache. The title's wrapped `vk-pipelines-*.bin` format is not accepted. It does not load or save the title's usual pipeline cache. NVIDIA and other driver caches remain uncontrolled: a fresh application cache is not a cold driver cache. JSON reports the capture identity, GPU/driver identifiers, word count, cache policy and separate wall times for shader module, layouts, pipeline creation and cache work. Device initialization is outside these timings. Pipeline statistics captures and `APS5_PIPELINE_STATS` are refused because they change compilation cost.

`--fixture` compiles a tiny synthetic compute shader with specialization, sparse descriptor binding and push constant metadata; `--write-fixture DIRECTORY` saves it without opening Vulkan. It performs no GPU work and does not reproduce a title's compile workload. With `BUILD_TESTING=ON`, `agc_compute_pipeline_capture` checks serialization and rejected inputs. With SPIRV-Tools also enabled, `agc_compute_pipeline_replay` compiles the fixture with fresh and imported application caches, verifies cache rejection, and checks required subgroup support. `agc_compute_pipeline_capture_run` verifies the existing branch-past-endpgm compute fixture's output with capture disabled and enabled, then replays the captured pipelines and checks their identities and metadata. `agc_compute_pipeline_inspection` checks executable counter queries, diagnostic JSON, retained capture identity, and benchmark-mode guards; its timeout is 60 seconds. The other tests have 30-second timeouts. Runtime tests skip when Vulkan is absent; set `ANYPS5_REQUIRE_VULKAN=1` to require execution. There is no performance pass threshold.

## PM4 submission replay

Set `APS5_DUMP_SUBMITS` to a directory before starting a title to save every submission the AGC driver accepts, as it enters the driver. `index.txt` gets one line per submission: ordinal, queue, `graphics` or `compute`, serial, microseconds on the clock of the `APS5_TRACE_GPU` lines, dword count, kind, file name, the dword offsets of the waits whose value passed their compare when the submission entered the driver (`held`, which the driver then does not wait for) and the COND_EXEC and predicated indirect buffer ranges as `start:end` dword offsets (`-` for none). `submission_<ordinal>.bin`, with the ordinal zero-padded to six digits (`submission_000001.bin`), holds the dwords the driver copied, with indirect buffers inlined. A REWIND tail is listed as `rewind` with the serial of its submission, a suspend point as `suspend` without a file. The index is truncated on the first submission. A failed write puts the driver in its failed state: the call that made the submission throws after the submission was queued, and every later driver call throws the same error. Unset, the switch costs one test per submission; set, one copy of the commands per submission before the driver lock, the ordinal, time and held waits under it without allocating, and the files written after it. A capture holds command buffers only, no guest memory. An index written before the held and conditional columns (column line `ordinal queue type serial time_us dwords kind file`) is still read, with both unknown.

`agc_pm4_replay <directory> [--full]` decodes the capture offline with the driver's packet decoder, validator and store decode (`Pm4::DecodeStores`) and reports each WAIT_REG_MEM, WAIT_REG_MEM_64 and REWIND as released, unreleased or unknown. It does not model cross-queue timing: a wait is released only when the capture alone proves that the driver releases it, and unknown, with the reason, whenever its outcome depends on when the stores of another queue run, since a store of another queue can run before, between or after the stores of the waiting queue whatever order the submissions entered the driver in. A memory wait is released when its compare always passes; when it was held at submit; when the last store to the awaited bytes before it on its queue (in that submission or an earlier one) covers them with a value the compare accepts, is not conditional, and no other queue stores to any of those bytes anywhere in the capture (current value); or when a RELEASE_MEM or WRITE_DATA label earlier in the waiting submission, one the driver keeps in its label history (4-byte aligned, at most 64 bytes, not conditional), stores a value the compare accepts and fewer than 4 other stores to the awaited bytes can run between it and the wait (label history). The driver keeps the last 4 label stores per dword over all queues (`LabelHistory.cpp`) and finds one there only if it ran after the waiting submission entered the driver, which the capture proves only for a store earlier in that submission. The stores counted against the history are those between the label and the wait on the waiting queue and every store of another queue to the awaited bytes, whatever its submit order; stores later on the waiting queue run after the wait. A REWIND is released when its tail is in the capture. One that was valid at submit without a tail in the capture is unknown, because the driver reads the valid bit again when the queue reaches the REWIND. A memory wait is unreleased when it was not held at submit and no store in the capture that can run before it may write a value the compare accepts (a store of part of the awaited bytes or of a value known only at run time may): the GPU does not release it, but a CPU store could, and so could a shader store, since the capture holds neither. A REWIND without a tail that was not valid at submit is unreleased. Every other wait is unknown: `unknown: depends on cross-queue timing (stores on queue X at #...)` when another queue stores to the awaited bytes; otherwise when the last store before it on its queue is conditional (COND_WRITE, inside a COND_EXEC range or a predicated packet), stores a value known only at run time or only part of the awaited bytes, or overwrites an accepted store that the capture does not prove is still in the label history (one in an earlier submission, one the history does not keep, or one with 4 or more stores after it). When the index has no held column, a wait that would be unreleased is unknown, since its value may have passed the compare at submit; without the conditional column every packet after a COND_EXEC or predicated INDIRECT_BUFFER is conditional to the end of its submission. The report counts the waits per outcome and route and the unknown waits that depend on cross-queue timing, says when the index lacks the columns, lists up to 32 unreleased and then unknown waits, newest first, each with the packets around it and the stores to the awaited bytes that decide it (for another queue, the 4 nearest the waiting submission in submit order), then the last submissions of each queue. `--full` lists every wait and prints every packet. A packet the driver rejects means the capture is corrupt and fails the analysis. The exit code is 1 when a wait is unreleased and 2 on errors.

```sh
cmake --build build --target agc_pm4_replay
build/core/libs/prx/libSceAgcDriver/agc_pm4_replay submits
```

On Windows use `agc_pm4_replay.exe` and prepend `build/core/libs/libs/unpatched` to the process `PATH`. With `BUILD_TESTING=ON`, `agc_driver_pm4_replay` captures submissions made with the libSceAgc command builders through the driver and checks the index, the held waits, the conditional ranges, the dwords, the decode and the outcome of each wait, then checks hand-made captures: each release route; a store of another queue to the awaited bytes that entered before or after the store of the waiting queue (unknown); the label history limit, with the stores after the label on the waiting queue or on another queue; a fill the history does not keep; a store in an earlier submission overwritten later; later stores, rejected values and a missing rewind tail (unreleased); the six captures of the reviews (a two-queue deadlock, a store of 0 behind a released wait on another queue, with and without that wait held at submit, and labels pushed out of the history by stores of another queue), none of whose waits may be released except the one held at submit; five groups of waits in eight submission orders with identical outcomes; an index without the held and conditional columns; corrupt captures; then a failed write. `agc_driver_pm4_replay_tool` runs the tool on the driver capture and checks its exit codes and report.

## Frame capture and replay

On Windows and Linux, set `APS5_FRAME_CAPTURE` to a directory to record a few frames of a title at the AGC driver boundary, then replay them with `agc_frame_replay` through any build of the driver with no title running. From the start of the process the driver keeps every shader registration and ABI resolution, video output registration and EOP flip reservation. At the frame boundary picked by `APS5_FRAME_CAPTURE_AT_FLIP` (default 1), or by the appearance of the file `APS5_FRAME_CAPTURE_TRIGGER` names (a number in it replaces the frame count), it waits for the driver to go idle and writes every committed, readable allocation of the guest allocation registry, the title's image included, as zstd chunks without their all-zero 64 KiB blocks. A frame ends at a CPU flip or at a submission that flips (a flip packet, or the EOP release of a reserved flip token); the reservation itself only records its token. For the next `APS5_FRAME_CAPTURE_FRAMES` (10) frames it records each submission (queue, command buffer address and size), flip and suspend point with the thread that made it and the accepted and completed serials, each preceded by the guest pages the write tracker saw the CPU write since the previous call that handed work to the GPU (`GuestMemory::CollectWrittenBlocks`, per 4 KiB where the tracker kept page stamps), read once the submissions already accepted have completed (a recorded submission, flip or suspend point reaches the driver before the recorder takes its next call, so that wait covers every call recorded before the delta); an allocation the registry gained after the snapshot is recorded there as a new range with its nonzero 64 KiB blocks. A submission ends a frame only when it flips itself, not when another thread's submission flipped while it was being submitted. Shader registrations and ABI resolutions only collect the blocks of the shader header and the arrays it points to, and the pages of the title's image only when their content changed. At the end it stores each display buffer the title presented, decoded, as a reference image. Without the guest write watch (`APS5_NO_WRITE_WATCH`, or no write watch on the host) the capture throws when it starts, since it could not record CPU stores. A capture is `calls.bin`, `memory.bin` and `capture.json` (commit, switches, sizes, warnings), and the recorder stops with a warning when its volume has less than 4 GiB free. A 20-frame capture in the Demon's Souls Nexus is 5.3 GiB of snapshot and 11.5 GiB of deltas before compression, 3.9 GiB on disk.

`agc_frame_replay <capture> [--no-pace] [--no-release] [--release-after-ms <n>] [--no-synthesize] [--strict] [--dump <address>:<bytes>:<file>]... [--frames <dir>]` maps the captured ranges at their addresses and registers them with their access (a range the replaying process already maps writable, such as the driver's fixed memory, is only refilled, and a non-GPU range it uses itself is skipped and counted), repeats the registrations, then replays the calls of each captured thread on a thread of its own, started in captured order. Before a call that hands work to the GPU it waits until no more submissions are in flight than when the title made it (`--no-pace` turns this off). Titles release GPU waits from threads that never call the driver, so when the driver completes nothing for 3 ms the replay reads the waits its queues are blocked on (`AgcDriverAwaitedWaits`) and writes the next recorded CPU value of the awaited dword that satisfies the compare, else the last earlier one, else a value derived from the reference (`--no-synthesize` turns this last step off). It prints one JSON line with the frame count and times, the submissions, the pacing wait, the released waits and how many were derived, the submissions that never completed and, for captures with reference images, the share of identical pixels and the largest channel difference of the replayed display buffers (`--frames` writes both images). Effects that depend on timing (temporal antialiasing, particles) make pixels differ, so expect a visual match rather than identical frames. Replay times follow the GPU clock state: lock the clocks (`nvidia-smi -lgc`, `-lmc`) when comparing two drivers.

```sh
cmake --build build --target agc_frame_replay
build/core/libs/prx/libSceAgcDriver/agc_frame_replay capture
```

On Windows use `agc_frame_replay.exe` and prepend `build/core/libs/libs/unpatched` to the process `PATH`. With `BUILD_TESTING=ON`, `agc_frame_capture` checks the file format (round trip, zero-block elision, truncated and corrupt files), and `agc_driver_frame_capture_eop` reserves and submits six EOP flips with `APS5_FRAME_CAPTURE_FRAMES=3` and checks that the capture starts after the first flipping submission and holds three frames of one reservation and one submission each, then that a flipping submission on another thread leaves this thread's flip count unchanged, and `agc_driver_frame_replay` captures two frames of CPU writes followed by DMA copies, the second from a block committed during the capture, replays them in `agc_frame_replay` and compares the copies.

## Pipeline statistics

Set `APS5_PIPELINE_STATS=1` to capture and print driver statistics for each newly created graphics or compute pipeline. This requires `VK_KHR_pipeline_executable_properties` and `pipelineExecutableInfo`; an unsupported device fails with an error. Statistic names and units are driver-specific. Capturing statistics can increase pipeline compilation cost. The setting is disabled by default.

## GPU memory accounting

Set `APS5_TRACE_MEMORY_BUDGET=1` to track Vulkan allocations by device, heap and allocation category. Dynamic storage-cache budget queries print `[gpu-memory]` reports at most once every 10 seconds. Reports contain byte counts for active and retained memory, the driver's heap budget and usage, and the storage budget's sampled, cached, reserved, available and cap terms. Image reuse changes the category without counting another allocation; slab free slots and pooled buffers count as retained memory. Host fallback allocations count against the heap actually used.

`unattributed_usage` is reported heap usage above tracked allocations; `tracked_above_usage` is the reverse difference. Vulkan heap usage is an estimate and includes driver allocations, so neither difference identifies a specific allocation class. `complete=0` means accounting metadata could not be recorded consistently. The switch records allocation lifetimes and leaves cache limits and eviction unchanged. A forced `APS5_TEXTURE_CACHE_MIB` bypasses dynamic budget queries and their reports.

## Device loss reports

On a device that supports `VK_EXT_device_fault`, the first `VK_ERROR_DEVICE_LOST` prints the driver's fault description, the faulting addresses and the vendor fault codes as `[device-fault]` lines. Set `APS5_GPU_CHECKPOINTS=1` to also record a `VK_NV_device_diagnostic_checkpoints` marker with the program address before each compute dispatch; the report then lists the last markers the GPU reached. On a device without that extension but with `VK_AMD_buffer_marker` (RADV and AMD's own drivers), each compute dispatch instead writes a numbered marker into a host-visible buffer when the GPU reaches it and another when it finishes; the report then names the programs of the last dispatch reached, of the last one finished and of up to eight dispatches between them. Checkpoints are disabled by default and ignored on devices without the extension.

## Worker sampler

Set `APS5_SAMPLE_WORKER=<file>` to sample queue worker 0 every millisecond (in practice every timer tick). Every 20 s and at exit it writes `<file>`, one `module offset self inclusive` line per sampled address, and `<file stem>.collapsed.txt`, the worker's whole stacks in the collapsed format that speedscope.app and flamegraph.pl read: one `frame;...;frame count` line per distinct stack, root first, each frame as `module+0xRVA`. A walk that ends before the thread's root starts with `[truncated]`. The `#` lines give the samples, the walk depth (64 frames), the truncated walks, the mean and longest time the worker was suspended, and the size and PE timestamp of each module, to check the binary a file is symbolized against. A frame with `.pdata` is stepped by `RtlVirtualUnwind`; any other frame by its DWARF record in `.ehfram`, the unwind data the libraries are built with. Those steps read only inside the worker's stack, and a module is indexed after the worker resumes, the first time a walk meets it. `python tools/symbolize_samples.py <file stem>.collapsed.txt --libs <libs directory>` names each frame from the COFF symbols of the module that ran, or from its export table when it has none, and leaves a module unsymbolized when its size or timestamp differs from its `# module` line or when it has none (`--unverified` symbolizes it anyway); `--since <earlier copy>` keeps only the samples taken after that copy, `--out` writes the named stacks and `--demangle` uses `c++filt`. Given `<file>`, it lists functions by self and by inclusive samples, where a function's inclusive count is the sum of its addresses' and overcounts a function that is on one stack more than once.

`APS5_SAMPLE_COMMITTER=<file>` samples queue 0's draw committer the same way and writes the same two files. The committer records the draws the worker hands over (`APS5_PIPELINED_DRAWS`), so when the worker's stacks wait in `DrawPipeline::Enqueue`, its stacks show what bounds the frame. It starts with the first pipelined draw; with `APS5_PIPELINED_DRAWS=0` nothing is sampled.

`APS5_SAMPLE_THREADS=<file>` samples every thread of the process every 2 ms and writes the 40 hottest addresses of each busy thread to `<file>`, 12 frames deep and through `.pdata` only, under a `thread <id> samples <n>` line per thread and with the same `# module` lines. The samplers exist only on Windows; the addresses are symbolized offline with `tools/symbolize_samples.py`, which adds up all threads or, with `--thread <id>`, keeps one.

## Performance trace

Set `APS5_TRACE_FRAMES=<start>:<count>` to record `<count>` presents from present `<start>` (counted from 0), or `APS5_TRACE_SECONDS=<start>:<length>` to record from the first present at least `<start>` seconds after the first present until the first present `<length>` seconds later. Both values are whole numbers and the length is above 0; a malformed value, or both switches together, fails the first present. The window opens and closes at presents.

Set `APS5_TRACE_EVENTS=<count>` to change the buffers of the queue workers, the presenter, the draw committer of pipelined draws (`APS5_PIPELINED_DRAWS`) and the draw finisher (`APS5_DRAW_PIPELINE`) for a frame or seconds window. The count is a whole number from 1 through 4,194,304; the default is 262,144. An invalid value fails when one of these threads first binds to the trace, before recording. Each buffer uses `count * sizeof(TraceThread::Slice)` bytes (currently 40 bytes per event, since each slice also carries its recorder batch): 10 MiB at the default, 120 MiB at `3145728`, and 160 MiB at the maximum, per thread. The buffers are allocated even while waiting for the window to open, and formatting the trace uses additional memory. Shader preparation keeps its explicitly sized 32,768-event buffers, as does a title thread named `title` that binds one the first time it registers a shader or resolves a shader ABI. The GPU range limit stays 262,144. Summary-only sessions allocate no event buffers and do not read this setting. A larger buffer can still fill; use a shorter window or increase the count within the bound.

Inside the window the queue workers, the presenter and the draw committer and finisher record every `PerformanceTimer` scope and stage as a complete event named after its frame timing key: `Driver.WorkerWait`, `Driver.Packet`, `Driver.Packet.draw`, `Graphics.Draw`, `Driver.Present`, `Driver.Present.inflight_wait`, `Vulkan.Retire` and so on. A slice shorter than 20 us is only counted: its time is in the thread's counter `<thread> folded` (milliseconds per present) and in the summary. A thread writes a present's counter sample at its first slice after the next present, so the last present of the window has no sample; its time is still in the summary. By default, each of these threads allocates a buffer for 262,144 events when it starts; a full buffer stops that thread's recording, sets `trace_dropped` and puts an instant event `trace buffer full` on the thread's track at the start of the first lost slice (slices are appended when their outer scope ends, so that can precede the last recorded slice by up to one outer-scope length), with the number of lost slices in `args.events_lost`. When the window closes, the presenter formats the trace once and a `ProfileOutput` writer thread writes `aps5-trace-<commit>-<pid>.json.gz` (Chrome trace-event JSON, opened by ui.perfetto.dev or chrome://tracing) and appends one line to `aps5-summary.jsonl`, both in `APS5_TRACE_DIR` or the working directory. A window that is still open when the title exits writes nothing.

The summary line has `commit` and `dirty` of the build, `env` (every `APS5_*` variable; a value containing `/` or `\` is written as `<path>`), `gpu`, `driver`, `os`, `cpu` (the CPU brand string), `cpu_threads` and `memory_mib` (physical memory) of the host (all three absent on a host that is not x86 or reports no brand string), `title_id`, `title` and `content_version` from `app0/sce_sys/param.json` (only when the title has one; `content_version` is empty when the file has no `contentVersion`, or one that is not a string of at most 15 characters), `resolution` (the video output's), `window`, `presents`, `frame_ms` (p50, p90, p99 and max of the intervals between presents), `gpu_busy_ms_per_present` (the batch timestamps behind `[present]`), `worker_busy_pct` (share of the window each queue worker spends in `Driver.Packet`, `Driver.Suspend`, `Driver.EndSubmission`, `Driver.Rewind` and `Driver.Completion`), `top_phases_ms_per_frame` (the eight largest scopes and stages, without `Driver.WorkerWait`), `trace_dropped`, `trace_dropped_threads` and `trace`. `trace_dropped` is `true` when any thread lost a slice, any thread's totals table (512 scope and stage entries) was full, or the `gpu` track was full. `trace_dropped_threads` is present only then and has one entry per such thread name (threads that share a name, such as the shader prepare threads, share one entry: `events_lost` is summed, `events_full_after_s` is the earliest, `totals_full` is true if any is): `events_full_after_s` (seconds from the first present of the window to the start of the first lost slice; absent for `gpu` and, in every summary line (windowed `APS5_TRACE_FRAMES`/`APS5_TRACE_SECONDS` and `APS5_TRACE_SUMMARY_EVERY`), whenever the first lost slice starts outside the line's span of presents, including a slice whose scope began before the window opened or one lost after the window's last present), `events_lost` (slices not recorded; absent when 0; in `APS5_TRACE_SUMMARY_EVERY` lines it counts the thread buffer's losses since it started, not only those of the line's span) and `totals_full` (`true` when the thread's totals table overflowed, which makes the summary numbers incomplete; a thread with `events_lost` only has complete totals). A trace holds timings, scope names, counts and thread names; no guest memory, paths or strings of the title.

The shader preparation threads are traced as well, `shader prepare`, with a buffer for 32,768 events each. A wait for the GPU mutex on a traced thread is a `GpuMutex.wait.<site>` slice (`dispatch`, `draw`, `present`, `flush` and so on, as on the `[lock]` line).

The `gpu` track holds one `batch` slice per recorder batch, from the batch's timestamp pair. With `APS5_PROFILE_GPU=1` it also holds a slice per command class range, under the class names of the `[gputime]` line (`draw`, `copy`, `fill`, `present-blit`, ...); program ranges are left out. The track keeps up to 262,144 ranges; more set `trace_dropped`. The GPU clock is mapped onto the CPU clock with calibrated timestamps (`VK_KHR_calibrated_timestamps` or the EXT version, enabled only when a trace switch is set) taken when the window opens and when it closes, so the two clocks are matched at both ends of the window. A batch is added when the driver reads its timestamps, after its fence signals, so the batches of the last frames of the window, which complete after it closes, are missing. Without calibrated timestamps the trace has no `gpu` track, and the `[trace]` line says so.

With any of the three switches set, the recorder writes a timestamp pair per batch for the whole run, as with `APS5_PROFILE_DRAW`. Without them, a `PerformanceTimer`, a GPU mutex acquisition and a read of batch timestamps each cost one more predictable branch.

Set `APS5_TRACE_DRAW_PROGRAMS=1` (any value) to print one `[draw-programs]` line on stderr per draw, with the submission serial, the queue, and for each selected program its stage and role, the shader's code address and its entry address (`vertex/main=0x100000(entry=0x100040)`). It is for comparing the shaders a draw selects with shader-preparation failure logs; it reads no GPU state.

### Summaries for a long session

Set `APS5_TRACE_SUMMARY_EVERY=<seconds>` (a whole number above 0; a malformed value fails the first present) to profile a long session in the background without a trace. The first present opens the first interval. Every `<seconds>` seconds, at the first present at least that long after the interval opened, the presenter appends one line to `aps5-summary.jsonl` in `APS5_TRACE_DIR` or the working directory and opens the next interval at that same present, so the intervals follow each other without a gap and their `frame_ms` cover every interval between presents. The line has the fields of the window summary line for that interval only, plus `interval_start_s` and `interval_end_s` (seconds since the first present). Its `window` is `{"spec":"summary_every <seconds>","first_present":<present the interval opened at>,"ms":<span of the interval's presents>}` and its `trace` is the empty string. No event buffer is allocated: each thread keeps only the per-scope totals it already folds (a few kilobytes), and the presenter subtracts the totals of the previous interval. An interval that is still open when the title exits writes nothing.

The switch combines with `APS5_TRACE_FRAMES` or `APS5_TRACE_SECONDS`. The intervals run for the whole session and the window records its full trace on top: the event buffers are allocated and filled only while the window is open, the window's own summary line (with `trace` set and no `interval_*` fields) holds only the time inside the window, and the interval lines continue unchanged around it. Without a window switch no buffer is allocated at all. The two switches do not share a clock origin: `interval_*` is relative to the first present, as the window's `APS5_TRACE_SECONDS` start is.

Set `APS5_TRACE_BENCH=1` together with `APS5_TRACE_SUMMARY_EVERY` to also keep every interval's frame times for a benchmark report. At each interval the presenter hands the kept intervals to a writer thread, which formats and compresses the file and rewrites `aps5-bench-<commit>-<pid>.json.gz` in `APS5_TRACE_DIR` or the working directory: one JSON object with `format` (`aps5-bench/1`), the identity fields of the summary line (`commit`, `dirty`, `env`, `gpu`, `driver`, `os`, `cpu`, `cpu_threads`, `memory_mib` and, with `param.json`, `title_id`, `title` and `content_version`), `intervals_dropped` and `intervals`. Each interval has `start_s`, `end_s`, `first_present`, `resolution`, `gpu_busy_ms` (the interval's total) and `frame_us`, the whole microseconds between consecutive presents. The file keeps the last 240 intervals; `intervals_dropped` counts older ones it no longer holds. Any other value, or the switch without `APS5_TRACE_SUMMARY_EVERY`, fails the first present. The file is written to `<name>.tmp` and then moved over the previous one, so a crash or kill during a rewrite leaves the last complete report. Only the newest report waits for the writer: one handed over while an older one is still pending replaces it. The frame times cost 4 bytes per present in memory.

`python tools/trace_digest.py aps5-summary.jsonl` prints the interval lines of such a file as one row per interval (frame p50, p90, p99, max, GPU busy per present, the busiest queue worker and the largest phase) and a text sparkline of the frame p99 and of the GPU busy time; lines of window captures in the file are skipped, and changes to build, environment, host, resolution or title metadata between intervals are reported. `--json` prints the rows, the first interval's identity and the indices of identity changes as JSON.

With `APS5_TRACE_SUMMARY_EVERY` set, the recorder writes a timestamp pair per batch for the whole run and every `PerformanceTimer` scope adds its time to the thread's totals, two clock reads per scope. A thread's totals are read while it runs, so a scope that straddles an interval boundary is counted in one interval or the other.

## GPU time per command class

Set `APS5_PROFILE_GPU=1` (any value) to time the recorder's GPU work with timestamp queries and print one `[gputime]` line on stderr. `APS5_PROFILE_DRAW` alone does not print it: a batch then gets only its whole-batch timestamp pair. A timed range is a pair of bottom-of-pipe stamps that both wait for all earlier work, so a range is the time its own work adds after everything recorded before it. A batch holds at most 512 ranges, its whole-batch range included. The line is written when a batch's timestamps are read after its fence, at least 10 s after the previous line, and covers every batch read since then.

The header fields: `<n> ms of GPU time` is the sum of the program-keyed ranges only (dispatches and buffer copies, see `by program`); draws and the driver's own commands are not in it. `in <n> batches` counts the batches read. `batch <n> ms first to last command` sums the whole-batch ranges, from the stamp written when the batch opens (it waits for the batches before it) to the stamp written at `Submit`. `classes <n> ms` is the plain sum of every class range, including `present-blit`, which runs outside the batches. `all ranges <n> ms` is, per batch, the union of its class and program ranges, with overlapping time counted once; `present-blit` is not in it. `untimed <n> ms` is `batch` minus `all ranges`: batch time that no range covers, including the work of dropped ranges. `<n> presents` counts the `VulkanDevice::present` calls since the previous line. `<n> ranges dropped at the 512 cap` counts ranges refused because their batch was full.

A class range covers a command together with its own barriers. In `VulkanDevice::CopyBuffer` the `copy` class range spans the leading barrier, the transfer and the trailing barrier, and the program-keyed range sits inside it around the transfer alone, so the transfer counts in both `GPU time` and `classes` but once in `all ranges`. A dispatch's leading and trailing barriers are ranges of their own classes, placed before and after its program range, not around it. `GPU time` plus `classes` can therefore exceed `batch`; compare `all ranges` with `batch` instead.

`by program` lists up to 12 keys, highest time first, as `0x<key> x<ranges> <ms>ms`. A dispatch's key is its guest program address, or the program's `variantId` when the address is 0. A buffer copy's key is the program address of the copy kernel it replaces, or 0 for a PM4 DMA copy (opcode 0x50). `by class` lists every class with at least one range since the previous line as `<name> x<ranges> <ms>ms <MiB>MiB (per present x<ranges> <ms>ms <MiB>MiB)`; the per-present figures are 0 when there was no present. MiB is the byte count each range passes to `EndGpuTiming`. The classes, with the function that opens each range:

- `dispatch-lead`: a dispatch's leading barrier, when it is recorded rather than merged into the previous trailing barrier (`VulkanDevice::recordDispatch`). 0 MiB.
- `dispatch-trail`: a dispatch's trailing barrier (`recordDispatch`). 0 MiB.
- `indirect-args`: the barrier that makes an indirect dispatch's host-imported group counts visible to the indirect read (`recordDispatch`). 12 bytes per range.
- `label-run`: a run of GPU label stores (`vkCmdUpdateBuffer`, plus the queued occlusion dump folds of a per-batch run) with its barriers (`Recorder::RecordStore` for inline runs, `Recorder::closeStoreRun` for per-batch runs). Stored bytes.
- `fill`: a guest fill of host-imported memory, as `vkCmdFillBuffer` or pattern copies, with its barriers (`VulkanDevice::FillBuffer`). Filled bytes.
- `fill-clear`: a fill done as a `vkCmdClearColorImage` of a storage image, between two layout barriers (`StorageTexture::FillClear`). Filled bytes.
- `copy`: the buffer copy of `VulkanDevice::CopyBuffer`, a whole-surface image copy (`StorageTexture::CopyFrom`) and the depth plane copies to and from storage images through a staging buffer (`copyPlane` and the layered copy in `DepthSurface.cpp`). Copied bytes.
- `staging-in`: a build's copies of guest buffer regions into staging or host-copy buffers, between two barriers (`GuestBufferMemory::recordGpuCopies`). Copied bytes.
- `staging-out`: the copies of shader-written sub-ranges back into the import (`GuestBufferMemory::RecordCopyBacks`). Copied bytes.
- `draw`: for a lean draw, from its first barrier to the end of its render pass, so draws that continue the pass fall inside one range (`recordDraw`, ended by `Recorder::endOpenRenderPass`); for a recorded draw with layout transitions, from its first barrier to its download barrier (`Draw`). 0 MiB.
- `storage-upload`: detiles of guest bytes into a storage image, whole (`StorageTexture::upload`) or by changed runs (`StorageTexture::uploadWindows`), and pending units copied from an alias image (`StorageTexture::borrowUnits`). Uploaded bytes.
- `storage-writeback`: retiles of a storage image back to guest memory, by windows (`StorageTexture::writeBackWindows`) or by layers (`StorageTexture::writeBackLayers`). Stored bytes.
- `dcc-clear`: a `vkCmdClearColorImage` of a storage image whose DCC keys read as a clear, at upload (`StorageTexture::upload`) or for a fill of its key bytes (`StorageTexture::clearByKeysFill`, with `APS5_KEYS_FILL_CLEAR=1`). Surface bytes.
- `dcc-keys`: queued DCC key stores, as `0xff` fills and seed copies between two barriers (`Recorder::recordKeyStores`). Key bytes.
- `present-blit`: the presentation command buffer from its first to its last command, timed in its own two-query pool (`VulkanDevice::present`) and added when its slot retires (`Recorder::AddGpuTiming`). Width x height x 4 bytes of the swapchain extent.
- `shadow-publish`: copies of unit shadow slabs into the import, with their barriers (`recordCopies` in `UnitShadow.cpp`, reached from `PublishShadow`). Copied bytes.
- `template-refresh`: the `vkCmdUpdateBuffer` updates of a cached dispatch's changed data words, as one range from the first update to the last (`ShaderResources::refreshData`, called from `recordDispatch`). Refreshed bytes.

## Guest clock scale

Set `APS5_TIME_SCALE=<factor>` to scale `sceKernelGetProcessTime`, `sceKernelGetProcessTimeCounter` and `sceKernelReadTsc` from their first observed value. A positive factor below 1 slows guest time; a factor above 1 speeds it up. An unset, zero, negative or non-numeric value uses `1.0` (normal speed). On Windows, `sceKernelClockGettime` monotonic clock ids are scaled too; on Linux they return the native host monotonic clock unchanged. A title's own `rdtsc` instruction and `sceKernelGetTscFrequency` are unscaled, so dividing scaled TSC deltas by the reported real frequency gives guest-time intervals. Host sleeps and waits still take real time, so this is a diagnostic aid for titles whose frame or timeout logic depends on guest time; it does not speed up the emulator or GPU work. The setting is read once when the process starts.

`APS5_PAD_INPUT_FILE` command durations and reload polling use `sceKernelGetProcessTime`, so they follow the scale. At `0.02`, a 1-second injected press lasts about 50 seconds of real time, and a new command can take up to 5 seconds to be noticed.

## Driver profile lines

With `APS5_PROFILE_DRAW` set (any value), the driver prints a profile line on stderr at most every 10 s. A line either counts the last 10 s (its counters are read and cleared when it is printed; the text says `(10 s)` or `over 10 s`) or counts from the start of the process (the text says `(cumulative)`). A rate is the difference of two cumulative lines divided by the time between them.

| Line | Counts |
|---|---|
| `[rescache] revalidate` | last 10 s |
| `[texture-cache]` | last 10 s |
| `[pipecache]` | last 10 s |
| `[rescache]` (hits, misses, entries) | from the start |
| `[vk]` | from the start |
| `[indirect]` (gpu-side, cpu-side) | from the start |
| `[flip]` | from the start |
| `[fill-sync]` | from the start |
| `[buffers] descriptor elements bound` | from the start |

`[gputime]` is described above and needs `APS5_PROFILE_GPU`. Other profile lines state their window in their text or are not listed here because their counters were not checked.

## Driver thread priority

Set `APS5_THREAD_PRIORITY=high` to raise the AGC driver's queue workers and the video output presenter above the title's threads. On Linux the threads get nice `APS5_THREAD_NICE` (default -10) through rtkit; on Windows they get `THREAD_PRIORITY_HIGHEST`. `APS5_THREAD_PRIORITY=rt` asks rtkit for real-time priority `APS5_THREAD_RT` (default 10, at most rtkit's `MaxRealtimePriority`) under rtkit's policy, `SCHED_RR` by default; on Windows it sets `THREAD_PRIORITY_TIME_CRITICAL`. rtkit only grants it when the process's hard `RLIMIT_RTTIME` is at most its `RTTimeUSecMax` (200000 us by default), so a hard limit above `APS5_THREAD_RTTIME_US` (default 200000, capped at `RTTimeUSecMax`) is lowered to it and the soft limit to at most half of it. The hard limit cannot be raised again and stays lowered for the whole process even if rtkit then refuses; see [technical debt](TechnicalDebt.md#functional). There is no fallback between the modes: when rtkit or `SetThreadPriority` refuses, a queue worker reports the refusal as a driver failure, which the next submission, wait or shutdown rethrows, and the presenter's start throws it. Each raised thread logs its priority once as `[thread-priority]`. The setting is `off` by default; a value other than `off`, `high` or `rt` is reported the same way as a refusal.

## Shader recompiler

The shader recompilation logic in [core/shader/recompiler](../../core/shader/recompiler) is isolated from the rest of the project and is a pure function of its input data, designed for integration into any other project. The current CMake target also includes cache support and links a supplied runtime target, glslang, and optionally SPIRV-Tools.

## Register context tests

The x86-64 register capture and restore tests can be built separately on Linux and macOS, without submodules or the prx libraries. On macOS:

```sh
cmake -S core/libs/tests/register_context -B build/register-context -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/register-context
ctest --test-dir build/register-context --output-on-failure
```

On Linux x86-64, omit `CMAKE_OSX_ARCHITECTURES`; the full `BUILD_TESTING` build also registers these tests. Apple silicon runs the x86-64 tests through Rosetta. They cover the unwinder's register transfer, not the complete macOS runtime or guest exception handling.
