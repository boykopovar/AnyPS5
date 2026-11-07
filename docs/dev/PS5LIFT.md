# ps5lift

A static recompiler for PS5 executables. `ps5lift` lifts guest x86-64 code to LLVM IR and compiles it to native code for the host, so a title can run on aarch64 Linux machines such as the NVIDIA DGX Spark (GB10) without an x86 emulator. The system libraries are AnyPS5's own, and AnyPS5's ELF reader and `.eh_frame` scanner are the lifter's frontend.

Status: **milestone 2 in progress**. Guest code built for Zen 2, including SSE, AVX2, FMA, F16C and x87 code, lifts and runs on aarch64 with the same output as on x86-64. AnyPS5's PS5-style test executables (NID imports, static TLS) relink, lift and run, and so does [EVO Player](https://github.com/sainsaji/EVO-PLAYER-PS5), a homebrew media player, as far as its home screen. Commercial titles do not run yet; see [the design](PS5LIFT_DESIGN.md) and [known gaps](TechnicalDebt.md#ps5lift).

## Build

Requirements: CMake 3.22+, Ninja, Clang and LLD, LLVM 18 or newer with development files (`llvm-18-dev libzstd-dev zlib1g-dev` on Ubuntu 24.04 / DGX OS), Python 3. The build also builds AnyPS5 for a lifted guest, which has [its own requirements](BUILD.md#requirements), among them SDL's X11, Wayland and audio development files. The bridge generator, and with it the tests, needs the libclang Python bindings (`python3-clang-18`); without them, configure with `-DBUILD_TESTING=OFF`.

The build compiles AnyPS5's libraries for a lifted guest (`ANYPS5_GUEST_LIFTED`) from the tree around `ps5lift/`. From the repository's root:

```sh
git submodule update --init --recursive
cd ps5lift
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm
ninja -C build
ctest --test-dir build --output-on-failure
```

When the tests run lifted code natively they lift it and build the runtime for the host CPU (`--cpu native`), since the `neoverse-v2` default uses SVE, which CPUs such as Apple's do not have; `tools/ps5lift.py fixture --cpu` lifts for another CPU. AnyPS5's libraries are built for `neoverse-v2`, so the tests that use them need a CPU that runs `neoverse-v2` code. On an aarch64 host the test lifts the fixture and runs it natively; building the x86-64 guests there needs an x86-64 cross toolchain and libraries (`g++-x86-64-linux-gnu` on Ubuntu). On an x86-64 host it also runs it natively and, configured with `-DPS5LIFT_QEMU_TESTS=ON` and with `qemu-aarch64` and an aarch64 sysroot installed (`qemu-user g++-aarch64-linux-gnu`), lifts it for aarch64 and runs it under qemu. AnyPS5 is built for the host, so the tests that use its libraries run for the host's architecture only. The libc tests reserve the PS5 user address range, about 1 TiB of address space, so they fail under a `ulimit -v` limit.

On an Apple Silicon Mac `ps5lift` builds and lifts for Linux, but what it lifts still runs on Linux only. Xcode's command-line tools and Homebrew's `llvm` and `lld` are the toolchain. The tests lift every fixture for aarch64 and x86-64 Linux without running the result, and leave out AnyPS5's build and the tests that use its libraries. The x86-64 guests that use threads or libc build against an x86-64 Linux sysroot: a directory holding `usr/x86_64-linux-gnu` and `usr/lib/gcc-cross/x86_64-linux-gnu` as Ubuntu's `g++-x86-64-linux-gnu` installs them, copied from such a machine.

```sh
git submodule update --init --recursive 3rdparty/zydis 3rdparty/softfloat
cd ps5lift
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR=$(brew --prefix llvm)/lib/cmake/llvm -DPS5LIFT_X86_64_SYSROOT=<sysroot>
ninja -C build
ctest --test-dir build --output-on-failure
```

`tools/ps5lift.py runtime` builds the runtime and [Berkeley SoftFloat](https://github.com/ucb-bar/berkeley-softfloat-3) (a submodule, for x87) into a shared library per target and CPU (`--cpu`, the lifter's default CPU when left out), and the program's `main` into an archive; the tests build and cache them in `build/runtime-<triple>` for the default CPU and `build/runtime-<triple>-<cpu>` for another.

## Lifter

The runtime is a shared library, which AnyPS5's libraries link too, so they reach its `GuestCpu` functions; the program's `main`, from `libps5lift_main.a`, hands it the lifted program. The lifter, `build/lifter/ps5lift`, takes these options:

| Option | Default | Effect |
|---|---|---|
| `--jobs` | the host's threads | Threads that lift and compile parts of the program at once. |
| `--triple` | `aarch64-unknown-linux-gnu` | Target for the generated code, aarch64 or x86-64. |
| `--cpu` | `neoverse-v2` on aarch64 | Target CPU; `native` uses the host CPU and features, and needs a triple of the host's architecture. |
| `--features` | none | LLVM target features, such as `-rcpc-immo`; they override the host's under `--cpu native`. An unknown CPU or feature stops `ps5lift` before it reads the input. |
| `--memory tso\|relaxed` | `tso` | `tso` keeps x86 ordering with acquire loads and release stores; `relaxed` uses plain accesses and is only correct for single-threaded guests. |
| `--float exact\|fast` | `exact` | `exact` reproduces x86 NaN, min/max, DAZ and conversion results bit for bit; `fast` leaves NaN payloads to the host. |
| `--preserve-flags` | off | Keeps the status flags across calls and returns, for hand-written assembly that relies on them. |
| `--import-nids` | off | Imports a plain symbol name as its NID, so a guest built for Linux calls AnyPS5's exports. |
| `--emit-ir prefix` | none | Writes the optimized LLVM IR of each part to `prefix.<n>.ll`. |
| `-O0` | optimized | Skips the LLVM optimization pipeline. |

`ps5lift` also lifts the modules the executable names in `DT_NEEDED` as `$ORIGIN/<path>`, such as the `app0/sce_module/*.guest.prx` files the relinker writes beside `app.elf`, and the runtime initializes them before the executable starts; see [the design](PS5LIFT_DESIGN.md#images). The output is a static archive with one object per part of the program: the functions in address order, about 50000 instructions to a part, and a last part with the image's segments and tables. Each part is lifted, optimized and compiled on a thread of its own, and calls between parts are relocations the linker resolves, with range-extension thunks where a call is too far for a direct branch. The parts depend only on the program, so the output does not depend on `--jobs`.

The lifter prints how many functions and instructions it lifted, how many jump tables it recovered and how many of them it sized by the data around them, and lists every unsupported instruction it found. Reaching one at run time stops the program with its address.

## Use

`tools/ps5lift.py` takes a title from its extracted files to a running game. `game` builds it from the title's `app0` directory, the one holding `eboot.bin`, into a directory of its own, and `run` starts it:

```sh
python3 tools/ps5lift.py game path/to/app0 out
python3 tools/ps5lift.py run out
```

`game` uses the build in `build` (`--build` names another) and takes these steps:

1. It prepares the title. The `eboot.bin` and the modules it ships, in `sce_module`, `prx` and the other directories AnyPS5's `audit_dump.py` knows, are SELF containers: it turns each decrypted one into a plain ELF with AnyPS5's `unwrap_self.py` and converts them with the AnyPS5 relinker. PS5 executables are position-independent, and the relinker places this one at 1008 GiB, where every guest image goes ([the design](PS5LIFT_DESIGN.md#machine-model)); the modules it loads go beside it under `app0`. The imports AnyPS5's libraries do not export are listed in `out/audit.json`; the game stops if it reaches one.
2. It generates the import bridges for the libraries the title links.
3. It lifts the title into `out/app.a`, and lifts again only when the lifter or the title is newer, or with `--relift`.
4. It builds the runtime and links `out/game`.

`run` starts the game in `out/run`, where `app0` is the title's directory, with `APS5_PLAYGO_UNLISTED_CHUNKS=installed` since every chunk of an extracted title is there. Settings such as AnyPS5's `APS5_` options follow as `NAME=VALUE`, and `game --run` builds and starts in one go. The steps are commands of their own too (`prepare`, `bridges` and `runtime`); `python3 tools/ps5lift.py --help` lists every command.

## Import bridges

`tools/ps5lift.py bridges` reads an AnyPS5 library's sources with libclang, with the compile commands of AnyPS5's build, and writes one bridge per export: a function named after the export's NID that takes the arguments from the guest's SysV registers and stack and calls the native implementation through its C prototype, so the host compiler produces the native call.

```sh
python3 tools/ps5lift.py bridges libc --anyps5-build build/anyps5 -o libc_bridges.cpp
```

Function-pointer parameters become native thunks that call back into the guest (see [the design](PS5LIFT_DESIGN.md#callbacks)), `long double` values cross between the guest's x87 format and the host's, and a variadic export calls its `v` sibling with a SysV `va_list` built from the guest's registers (see [variadic imports](PS5LIFT_DESIGN.md#variadic-imports)). It prints how many exports it could not bridge and why; `--report` lists them. A record passed or returned by value crosses as its SysV eightbytes, through a copy of its layout that `static_assert`s check against the library's. For libc it writes 1532 bridges and leaves 27 exports: records that are C++ classes rather than plain old data (18) or that hold a function pointer (2), complex values (6) and one that passes a function pointer to a callback. `setjmp` and `longjmp`, which need the guest's own registers, are the runtime's.

Several libraries can go into one file (`ps5lift.py bridges libc libkernel --anyps5-build build/anyps5 -o bridges.cpp`); a NID that more than one exports gets the first one's bridge. `tools/ps5lift.py survey --anyps5-build build/anyps5` generates the bridges of every AnyPS5 library and reports how many exports each covers (`--json` keeps the details).

The bridges call AnyPS5's libraries as AnyPS5's own CMake build makes them with `ANYPS5_GUEST_LIFTED`: the test build builds AnyPS5 in `build/anyps5`, whose `core/libs/libs/unpatched` holds each library as a shared object before NID patching. `tools/ps5lift.py libc-tests` runs upstream's libc, socket and resolver tests against those libraries, and `tools/ps5lift.py libc-fixture` lifts a guest that calls the libraries through the generated bridges; ctest runs both.

## Benchmark

`tools/ps5lift.py bench` times five memory-heavy kernels (streaming, pointer chasing, struct copies, a hash table and heap sort) built natively, natively without vectorization, and lifted with each memory model, and checks that every build computes the same results:

```sh
python3 tools/ps5lift.py bench --ps5lift build/lifter/ps5lift
```

`tso/relaxed` is the cost of keeping x86 ordering, `tso/native` the cost of lifting against the same code compiled natively without vectorization. On an x86-64 host the ordering is free and the numbers show the lifting overhead only; the Spark shows both. `--scale` changes the working set (16, the default, uses about 128 MiB).
