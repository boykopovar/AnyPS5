# Stage 3: compiler-generated program compatibility

## Scope and limitations

This is an experimental **x86-64 instruction interpreter**, not a PS5 emulator.
The first Stage 3 baseline uses twelve C functions, each accepting a single
unsigned 32-bit SysV AMD64 argument in EDI and returning EAX. The runner
prepends `mov edi, imm32` and executes the compiler's real function bytes.
No guest OS, loader, external symbols, guest heap, or general ABI is provided.

## Reproducible generation

On Linux x86-64 CI, `clang-17` compiles
`tests/stage3_fixture.c` with `-O1 -fno-stack-protector -fno-pic
-fno-asynchronous-unwind-tables -fno-unwind-tables -fno-builtin
-fcf-protection=none -falign-functions=1 -falign-loops=1`.
The generator uses `readelf -r` to **reject relocations**, `nm -S` to
locate exact function sizes and `objcopy --only-section=.text -O binary`
to extract machine code. The generated header is built in the CMake build
directory; checked-in byte arrays are not treated as compiler output.

The native reference is separately compiled from the **same C source**.
Only Linux x86-64 runs the ELF extractor; Windows/macOS retain the existing
portable C++ tests, and iOS ARM64 remains compile-only.

## Published baseline corpus

| Function | Inputs | Cases |
| --- | --- | ---: |
| triangular sum | 0–128 | 129 |
| bitmix | 0–511 | 512 |
| popcount | 0–255 | 256 |
| factorial | 0–12 | 13 |
| Fibonacci | 0–32 | 33 |
| Euclidean GCD | 0–255 | 256 |
| decimal digit sum | 0–255 | 256 |
| 32-bit bit reversal | 0–255 | 256 |
| array sum | lengths 0–128 | 129 |
| insertion sort | lengths 0–128 | 129 |
| recursive factorial (indirect callback) | 0–12 | 13 |
| cross-function callback to compiled bitmix | 0–64 | 65 |
| **Total** | | **2,047** |

The CTest executable prints per-function pass counts and a total
`STAGE3 COMPATIBILITY passed/2047` line. A mismatch or execution exception
causes CI failure; failures are not counted as passes. The denominator
represents **only this deliberately limited corpus**, not compatibility
with arbitrary x86-64 software or PS5 games.

## Known gaps and next work

- Recursive calls through a guest callback trampoline are covered; general
  multi-function linking, external relocations and ABI stack arguments still
  need Phase 4 loader/ABI work.
- Pointer and array inputs are now covered by a bounded 4096-address guest-memory fixture; broader memory/ABI coverage remains incomplete.
- Insertion sorting is covered for lengths 0–128 with deterministic mixed-value arrays and adjacent memory canaries; larger and randomized arrays remain untested.
- The generator requires Linux ELF and Clang 17; other compilers are not
  silently treated as equivalent.
- A green CTest confirms only the exact generated corpus and toolchain.

## Expansion checkpoint

The eleven-function corpus exercises compiler-emitted integer division in
addition to arithmetic, branching, shifts and multiplication. The interpreter
now includes unsigned F7 /6 DIV (32/64-bit), with dedicated quotient/remainder
regression tests. Treat the expanded compatibility percentage as **pending**
until its own complete CI run succeeds.

## Array expansion

The runner now sets guest RDI to address 4096, ESI to array length,
initializes guest words with `writeMemory32`, executes compiler-extracted
array sum/insertion sort, and compares both EAX and every resulting array
word with a native reference. The 66 new cases are included in the
1,777-case denominator. This expanded total is pending a green CI run.

## Recursive call expansion

The compiled recursive factorial accepts EDI=n and RSI=callback. The native
reference supplies a native recursive callback; the guest runner supplies a
bounded-code trampoline that resets RSI to its own guest address and jumps to
the same compiler-extracted function. This exercises indirect CALL, nested
stack frames and RET without pretending that a general ELF loader exists.
The 13 added cases raise the published corpus to 1,790. CI validation of
this expansion is required before claiming all 1,790 pass.

## Multi-function and memory-boundary expansion

The new callback fixture calls a separately compiled bitmix function twice,
using a guest code offset in RSI and indirect CALL/RET. The 65 input cases
check the result against native two-function execution. Array sum and insertion
sort now cover 129 lengths each, with guard words immediately before and after
the guest array; this adds 192 cases. The expanded denominator is 2,047
(1,790 + 192 + 65), pending final CI validation.
