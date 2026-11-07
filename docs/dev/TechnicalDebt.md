# Project technical debt

Entries about one library or component are in [debt](debt/), one file per library or component, under the same sections as this page. Entries that belong to none stay here. An entry may end with `Closes when:` and the measurement, title or test that would close it, such as `Closes when: measured on a PS5`, so the entries a contributor can re-check are found by searching for it. It may also end with `Labels:` and labels that replace what [`tools/debt_report.py`](../../tools/debt_report.py) infers for that family: `console`, `firmware`, `gpu`, `title` or `code` for what would close it, `no-test` or `tested`, and `windows`, `macos` or `linux`.

### Build

- The full Windows build requires x86-64 MinGW-w64 GCC with POSIX threads and DWARF or SEH exception support for System V ABI functions. The build checks these capabilities; see the [Windows build requirements](BUILD.md#windows-setup-and-troubleshooting) for the validated WinLibs and MSYS2 toolchains.
- Even compiled prx libraries on Windows require nearby (static linking of these dependencies causes conflicts):
  - libgcc_s_seh-1.dll
  - libstdc++-6.dll
  - libwinpthread-1.dll

### Silent stubs

Throughout the project, every function at every stage either **does exactly what it's supposed to or throws an exception**. The exceptions are listed under **Silent stubs** in each library's file in [debt](debt/).

### Unknown function info

- AudioIn and NpSessionSignaling exports added without an implementation have assumed signatures
- Font, Http2, Net, Ssl, SystemService and libkernel exports imported by PPSA12544 and added without an implementation have unknown signatures
- The Share, NpTrophy2, NpEntitlementAccess and WebBrowserDialog exports imported by PPSA12544's Unity plugins and added without an implementation have unknown signatures
- Acm, GameLiveStreaming, Net, NpEntitlementAccess, VoiceQoS, WebBrowserDialog and libkernel exports imported by PPSA14632 and added without an implementation are declared without parameters; of the VoiceQoS ones only those still throwing remain so

### Functional

- Controller haptic PCM that no controller sound card plays (a DualSense over Bluetooth, or any other controller) drives motor rumble from its per-channel RMS (large/small motor, scale 2x, below 0.01 ignored, 80 ms hold); the mapping approximates waveform energy and is not measured against console hardware
- [Cancellation coverage](ARCHITECTURE.md#thread-cancellation) checks deferred request handling and recovery, not full asynchronous support or console ABI compatibility. Cancellation during file/socket I/O, sleep interruption, cancellation versus exit/detach, condition signal conservation under cancellation, and safe rejection of non-null invalid or reclaimed thread handles remain unverified. No console title or homebrew establishes the cleanup NIDs or buffer layout. Accepted asynchronous mode currently has no diagnostic for its deferred behavior.
- `--to-intel` guest module trampolines are covered only by a synthetic relinker test; no game title has been verified with them on Linux or Windows.
- [Quick syntax checks](../../tools/check_compile.py) do not support Windows-style response-file quoting. GNU response files reject empty options, byte order marks, quoted CRLF, Windows text-mode EOF markers and escapes whose meanings differ between GCC and Clang, including backslashes in single quotes, most backslashes in double quotes and escaped line endings. Supply expanded compile arguments for those forms; the Ninja fallback uses `ninja -t compdb -x`.
- [Quick syntax checks](../../tools/check_compile.py) do not model every compiler option's operand count. A preprocessing mode following an unrecognized option without a joined value is rejected rather than risk removing that option's operand. Compiler wrappers with their own option grammar are not covered, except a leading `cmake -E env`, whose options other than `NAME=VALUE`, `--unset=`, `--modify` and `--` are rejected. Forwarded frontend actions (`-Xclang`, `-Xpreprocessor` and their joined forms) are rejected. Comma-forwarded options (`-Wp,`) allow only recognized macro/include operands; response-file tokens are rejected even as operands, and other options are rejected because unmodeled actions or output operands could bypass syntax analysis or write outputs. See the [Clang option grammar](https://clang.llvm.org/docs/ClangCommandLineReference.html).
- [Quick syntax checks](../../tools/check_compile.py) reject `-save-temps=` and `--save-temps=` values other than `cwd` and `obj` instead of passing them to the compiler: an unknown value fails on GCC, while newer Clang builds take any value as an output prefix and write preprocessed files even under `-fsyntax-only` (measured on Apple clang 21), so a passed-through mode either fails the check on one compiler or silently writes outputs on the other.

### ps5lift

#### Bugs

Each of these gives a wrong result, corrupts state or reports a misleading error instead of stopping cleanly, and most have no fixture that would catch them.

- Unnormals, pseudo-NaNs and pseudo-infinities are computed as SoftFloat normalises them, instead of raising IE and returning the indefinite; `fxam` already classifies them as unsupported.
- [`PS5LIFT_MXCSR_MASK`](../../ps5lift/runtime/include/ps5lift/State.h) is 0xffff where Zen 2's MXCSR mask is 0x2ffff: the PS5's `cpuid` reports misaligned SSE, which allows MXCSR bit 17, and `ldmxcsr` and `fxrstor` with bit 17 set stop the program instead of taking the value.

#### Gaps

- [The runtime](../../ps5lift/runtime/src/Runtime.cpp) maps the whole image read-write (through AnyPS5's guest arena when its libc is linked in), and runs modules' initializers but not the executable's nor any finalizer; the relinker's entry stub is the only entry contract tested.
- [`cpuid` and `xgetbv`](../../ps5lift/runtime/src/Cpuid.cpp) answer as an AMD Zen 2: with the signature of AMD's 4700S desktop kit, which is built on the PS5's chip (family 17h, model 47h, stepping 0), and with the user-mode extensions LLVM enables for `znver2` plus the optional ones AnyPS5's host check lists for the console. None of it is measured on a console. The highest leaves reported are 0xd and 0x80000001, so code reads the cache, topology and brand-string leaves as absent; leaves 5 and 6 and `xgetbv` of a register other than XCR0 stop the program.
- Lifted calls nest on the host stack, which the runtime sizes at 512 MiB for every guest thread, those libkernel starts included; a native thread that calls guest code has the host stack its creator gave it.
- `longjmp` unwinds lifted frames ([Jump.cpp](../../ps5lift/runtime/src/Jump.cpp)) but not native frames between them, so a `longjmp` out of a guest callback that native code called stops the program. Its target frame is the innermost one with a `setjmp` call whose entry stack pointer is at or above the saved one, which assumes the frame did not move its stack pointer above that value after `setjmp`.
- Native calls through a guest function pointer read from memory pass only the arguments and results the host keeps in registers, cost a signal each, and the x86-64 host's trampoline has not run. [Callbacks](../../ps5lift/runtime/include/ps5lift/Bridge.hpp) take and return scalars only, and each signature has 256 thunks; the 257th distinct guest address for one signature stops the program. The guest state a foreign native thread gets on its first callback lives until that thread exits.
- An SSE exception that MXCSR unmasks raises no SIGFPE, where the console raises one: [`ps5lift_apply_mxcsr`](../../ps5lift/runtime/src/Runtime.cpp) gives the host FPU MXCSR's rounding mode, DAZ and FTZ but never its exception masks, for lifted code's `ldmxcsr` and a lifted libc's `feenableexcept` alike.
- A binary128 `long double` from host code reaches the guest through `GuestCpuStoreExtended` and the bridges' `ReturnExtended` rounded once more, to the x87 format's 64-bit significand in the control word's rounding mode ([X87.cpp](../../ps5lift/runtime/src/X87.cpp), `ps5lift_extended_from_host`), so a result that host math already rounded to binary128 is rounded twice, which can be off by one in the last place, and the precision control is ignored: a guest that sets it to 24 or 53 bits still gets 64.
- [GuestCpu](../../ps5lift/runtime/src/GuestCpu.cpp), the guest FPU state that a lifted AnyPS5 libc reads: x87 is only its control and status words, so `fegetenv` reports every x87 register empty; `fwait` with an unmasked pending x87 exception ends the program where the console raises SIGFPE; the MXCSR support mask is reported as `0xffff`, not measured on a PS5. Flags that lifted arithmetic raises reach `fetestexcept` through the host FPU, but LLVM may reorder or fold floating-point operations around a call because it assumes the default environment.
- [x87](../../ps5lift/runtime/src/X87.cpp): the transcendental instructions (`fsin`, `fcos`, `fsincos`, `fptan`, `fpatan`, `f2xm1`, `fyl2x`, `fyl2xp1`), `fxtract`, `fbld`, `fbstp`, `fsave`, `frstor`, `fnop` and `ffreep` are reported as unsupported. `fprem` always finishes the reduction in one step, so C2 is never set, and the C bits of an `fprem` with a NaN result and the rest of the x87 behaviour were measured on an Intel Xeon, not on Zen 2. An unmasked x87 exception ends the program at the next `fwait`, not at the next x87 instruction, and `fnstenv`, `fxsave` and `fxsave64` record zero instruction and operand pointers and last opcode. `fxsave` writes the first 416 bytes of its area and leaves the reserved rest alone; the `fx-state` fixture matched an Intel Pentium Silver (Goldmont Plus) for the words, MXCSR and its mask, the loaded registers and an XMM register.
