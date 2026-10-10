# Build

## Relinker only

The relinker can be built without initializing submodules or configuring SDL, Vulkan, FFmpeg, FreeType or the shader recompiler. It requires CMake, a C++20 compiler and a build tool. Python 3 enables the Python regression tests.

```sh
cmake -S . -B build-relinker -G Ninja -DCMAKE_BUILD_TYPE=Release -DANYPS5_RELINKER_ONLY=ON -DBUILD_TESTING=ON
cmake --build build-relinker --parallel
ctest --test-dir build-relinker --output-on-failure
```

Python must be found during CMake configuration for Python tests to be registered. List registered tests with `ctest --test-dir build-relinker -N` and look for `input_magic`, a Python-backed relinker test. If automatic discovery fails, add `-DPython3_EXECUTABLE="C:/path/to/python.exe"` to the configure command, using the path to your Python interpreter, then list the tests again.

This mode builds the conversion tool and its tests on Linux, Windows and macOS, including Apple Silicon. macOS uses AppleClang from the Xcode command-line tools; Windows uses the MinGW-w64 toolchain below. The executable is `build-relinker/core/relinker/relinker` (`relinker.exe` on Windows with Ninja).

The output remains x86-64 Linux ELF or Windows PE. Converted games need system libraries built for the target OS and a compatible x86-64 host. This mode does not build those libraries or provide macOS game execution. Tests inspect both output formats; execution checks run only on their compatible hosts.

Use a separate build directory for the full build.

## Full build

```sh
git submodule update --init --recursive
```

## Requirements

- x86-64, Git, CMake 3.22.1 or newer, Ninja, C++20.
- Linux: GCC, G++, binutils. SDL's X11 backend requires X11 and Xext development headers (`libx11-dev` and `libxext-dev` on Debian/Ubuntu).
- Fedora: if CMake reports `Could NOT find Threads`, install `glibc-static` and `libstdc++-static` with `sudo dnf install glibc-static libstdc++-static`, then rerun configuration. The project enables static executable linking, and FFmpeg's thread check needs these libraries.
- Windows: only MinGW-w64 GCC 15.2.0 (WinLibs `x86_64-ucrt-posix-seh`, release `15.2.0posix-14.0.0-ucrt-r7`) is currently supported. Add its `mingw64/bin` directory to `PATH` before configuring.
- Windows antivirus software can quarantine the executables that the relinker tests create in the temp directory, which fails `optional_plt`, `empty_tls`, `windows_address_space` and `windows_icon`. Set `TEMP` and `TMP` to a directory the scanner excludes before running `ctest`.
- FFmpeg binaries are downloaded during configuration unless `FFMPEG_PREBUILT_DIR` is set. With the WinLibs CMake, the download fails with status 60 (`SSL peer certificate or SSH remote key was not OK`) unless `SSL_CERT_FILE` names a CA bundle, for example `C:\Program Files\Git\mingw64\etc\ssl\certs\ca-bundle.crt` from Git for Windows, as in CI.

## Commands

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build --parallel
cmake --build build --target libs --parallel
```

`libs` is a custom target: every library under [core/libs/prx](../../core/libs/prx) is built with
`EXCLUDE_FROM_ALL`, so the first command alone does not produce them. Titles load the patched `.prx`
files from `build/core/libs/libs`, which only that second step refreshes. Running a title after a
library change without it therefore tests the previous binaries and can show no effect at all.

[Relinker usage and runtime layout](../user/USAGE.md).

## CMake flags

Project switches accept `ON` or `OFF`:

| Flag                             | Default | Effect                                           |
|----------------------------------|---------|--------------------------------------------------|
| `-DBUILD_TESTING=ON`             | `OFF`   | Build and register tests.                        |
| `-DANYPS5_RELINKER_ONLY=ON`      | `OFF`   | Build only the relinker and its tests, without third-party dependencies. |
| `-DANYPS5_ENABLE_SPIRV_TOOLS=ON` | `OFF`   | Enable SPIR-V validation and optimization.       |
| `-DAPS5_ENABLE_TIMING_LOG=ON`    | `OFF`   | Compile frame timing logging.                    |
| `-DAPS5_AGC_CREATE_LOG=OFF`      | `ON`    | Disable successful `sceAgcCreateShader` logging. |
| `-DAGC_BUILD_VISUAL_TEST=ON`     | `OFF`   | Build the standalone AGC SPIR-V visual test.     |

Build configuration parameters:

| Flag                                   | Value                                                              |
|----------------------------------------|--------------------------------------------------------------------|
| `-DCMAKE_BUILD_TYPE=Release`           | `Debug`, `Release`, `RelWithDebInfo`, or `MinSizeRel`.             |
| `-DCMAKE_C_COMPILER=gcc`               | C compiler name or absolute path.                                  |
| `-DCMAKE_CXX_COMPILER=g++`             | C++ compiler name or absolute path.                                |
| `-DCMAKE_C_COMPILER_LAUNCHER=ccache`   | Optional C compiler cache; requires `ccache`.                      |
| `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache` | Optional C++ compiler cache; requires `ccache`.                    |
| `-DFFMPEG_PREBUILT_DIR=<path>`         | Unpacked FFmpeg package for the target platform; empty by default. |

SDL and FreeType settings forced by the root `CMakeLists.txt` cannot be overridden with `-D`.

## Pipeline statistics

Set `APS5_PIPELINE_STATS=1` to capture and print driver statistics for each newly created graphics or compute pipeline. This requires `VK_KHR_pipeline_executable_properties` and `pipelineExecutableInfo`; an unsupported device fails with an error. Statistic names and units are driver-specific. Capturing statistics can increase pipeline compilation cost. The setting is disabled by default.

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
