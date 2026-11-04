# ps5lift

A static recompiler for PS5 executables. `ps5lift` lifts guest x86-64 code to LLVM IR and compiles it to native code for the host, so a title can run on aarch64 Linux machines such as the NVIDIA DGX Spark (GB10) without an x86 emulator. The system libraries are AnyPS5's own, and AnyPS5's ELF reader and `.eh_frame` scanner are the lifter's frontend.

Status: guest code built for Zen 2, including SSE, AVX2, FMA, F16C and x87 code, lifts and runs on aarch64 with the same output as on x86-64. Commercial titles do not run yet; see [the design](PS5LIFT_DESIGN.md) and [known gaps](TechnicalDebt.md#ps5lift).

## Build

Requirements: CMake 3.22+, Ninja, Clang and LLD, LLVM 18 or newer with development files (`llvm-18-dev libzstd-dev zlib1g-dev` on Ubuntu 24.04 / DGX OS), Python 3.

From the repository's root:

```sh
git submodule update --init --recursive 3rdparty/zydis 3rdparty/softfloat
cd ps5lift
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm
ninja -C build
ctest --test-dir build --output-on-failure
```

When the tests run lifted code natively they lift it for the host CPU (`--cpu native`), since the `neoverse-v2` default uses SVE, which CPUs such as Apple's do not have; `tools/ps5lift.py fixture --cpu` lifts for another CPU. On an aarch64 host the test lifts the fixture and runs it natively; building the x86-64 guests there needs an x86-64 cross toolchain and libraries (`g++-x86-64-linux-gnu` on Ubuntu). On an x86-64 host it also runs it natively and, configured with `-DPS5LIFT_QEMU_TESTS=ON` and with `qemu-aarch64` and an aarch64 sysroot installed (`qemu-user g++-aarch64-linux-gnu`), lifts it for aarch64 and runs it under qemu.

On an Apple Silicon Mac `ps5lift` builds and lifts for Linux, but what it lifts still runs on Linux only. Xcode's command-line tools and Homebrew's `llvm` and `lld` are the toolchain. The tests lift every fixture for aarch64 and x86-64 Linux without running the result. The x86-64 guests that use threads or libc build against an x86-64 Linux sysroot: a directory holding `usr/x86_64-linux-gnu` and `usr/lib/gcc-cross/x86_64-linux-gnu` as Ubuntu's `g++-x86-64-linux-gnu` installs them, copied from such a machine.

```sh
git submodule update --init --recursive 3rdparty/zydis 3rdparty/softfloat
cd ps5lift
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR=$(brew --prefix llvm)/lib/cmake/llvm -DPS5LIFT_X86_64_SYSROOT=<sysroot>
ninja -C build
ctest --test-dir build --output-on-failure
```

`tools/ps5lift.py runtime` builds the runtime and [Berkeley SoftFloat](https://github.com/ucb-bar/berkeley-softfloat-3) (a submodule, for x87) into a shared library per target, and the program's `main` into an archive; the tests build and cache them in `build/runtime-<arch>`.

## Lifter

The runtime is a shared library; the program's `main`, from `libps5lift_main.a`, hands it the lifted program. The lifter, `build/lifter/ps5lift`, takes these options:

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

## Benchmark

`tools/ps5lift.py bench` times five memory-heavy kernels (streaming, pointer chasing, struct copies, a hash table and heap sort) built natively, natively without vectorization, and lifted with each memory model, and checks that every build computes the same results:

```sh
python3 tools/ps5lift.py bench --ps5lift build/lifter/ps5lift
```

`tso/relaxed` is the cost of keeping x86 ordering, `tso/native` the cost of lifting against the same code compiled natively without vectorization. On an x86-64 host the ordering is free and the numbers show the lifting overhead only; the Spark shows both. `--scale` changes the working set (16, the default, uses about 128 MiB).
