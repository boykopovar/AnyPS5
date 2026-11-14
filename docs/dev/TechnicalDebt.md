# Project technical debt

Entries about one library or component are in [debt](debt/), one file per library or component, under the same sections as this page. Entries that belong to none stay here. An entry may end with `Closes when:` and the measurement, title or test that would close it, such as `Closes when: measured on a PS5`, so the entries a contributor can re-check are found by searching for it. It may also end with `Labels:` and labels that replace what [`tools/debt_report.py`](../../tools/debt_report.py) infers for that family: `console`, `firmware`, `gpu`, `title` or `code` for what would close it, `no-test` or `tested`, and `windows`, `macos` or `linux`.

### Build

- The full Windows build requires x86-64 MinGW-w64 GCC with POSIX threads and DWARF or SEH exception support for System V ABI functions. The build checks these capabilities; see the [Windows build requirements](BUILD.md#windows-setup-and-troubleshooting) for the validated WinLibs and MSYS2 toolchains.
- Even compiled prx libraries on Windows require nearby (static linking of these dependencies causes conflicts):
  - libgcc_s_seh-1.dll
  - libstdc++-6.dll
  - libwinpthread-1.dll
- With a `libstdc++-6.dll` from another toolchain earlier on `PATH` (such as Git's `mingw64\bin`), a program that finds the three DLLs above only through `PATH` loads that copy and fails with "Entry Point Not Found". With CMake 3.28 or later, ctest puts the build compiler's directory first on `PATH` for the tests this project registers; with older CMake, and for programs run outside ctest, this is not handled.

### Silent stubs

Throughout the project, every function at every stage either **does exactly what it's supposed to or throws an exception**. The exceptions are listed under **Silent stubs** in each library's file in [debt](debt/).

### Unknown function info

- AudioIn and NpSessionSignaling exports added without an implementation have assumed signatures
- Font, Http2, Net, Ssl, SystemService and libkernel exports imported by PPSA12544 and added without an implementation have unknown signatures
- The Share, NpTrophy2, NpEntitlementAccess and WebBrowserDialog exports imported by PPSA12544's Unity plugins and added without an implementation have unknown signatures
- Acm, GameLiveStreaming, Net, NpEntitlementAccess, VoiceQoS, WebBrowserDialog and libkernel exports imported by PPSA14632 and added without an implementation are declared without parameters; of the VoiceQoS ones only those still throwing remain so

### Functional

- `title_run.py --compare` compares no frame percentiles: neither `aps5-summary.jsonl` nor `anyps5-report.json` holds them, so only the average FPS of each is compared. Closes when: a run report or trace summary field carries frame percentiles and `compare_runs` pairs it.
- Controller haptic PCM that no controller sound card plays (a DualSense over Bluetooth, or any other controller) drives motor rumble from its per-channel RMS (large/small motor, scale 2x, below 0.01 ignored, 80 ms hold); the mapping approximates waveform energy and is not measured against console hardware
- [Cancellation coverage](ARCHITECTURE.md#thread-cancellation) checks deferred request handling and recovery, not full asynchronous support or console ABI compatibility. Cancellation during file/socket I/O, sleep interruption, cancellation versus exit/detach, condition signal conservation under cancellation, and safe rejection of non-null invalid or reclaimed thread handles remain unverified. No console title or homebrew establishes the cleanup NIDs or buffer layout. Accepted asynchronous mode currently has no diagnostic for its deferred behavior.
- `--to-intel` guest module trampolines are covered only by a synthetic relinker test; no game title has been verified with them on Linux or Windows.
- [Quick syntax checks](../../tools/check_compile.py) do not support Windows-style response-file quoting. GNU response files reject empty options, byte order marks, quoted CRLF, Windows text-mode EOF markers and escapes whose meanings differ between GCC and Clang, including backslashes in single quotes, most backslashes in double quotes and escaped line endings. Supply expanded compile arguments for those forms; the Ninja fallback uses `ninja -t compdb -x`.
- [Quick syntax checks](../../tools/check_compile.py) do not model every compiler option's operand count. A preprocessing mode following an unrecognized option without a joined value is rejected rather than risk removing that option's operand. Compiler wrappers with their own option grammar are not covered, except a leading `cmake -E env`, whose options other than `NAME=VALUE`, `--unset=`, `--modify` and `--` are rejected. Forwarded frontend actions (`-Xclang`, `-Xpreprocessor` and their joined forms) are rejected. Comma-forwarded options (`-Wp,`) allow only recognized macro/include operands; response-file tokens are rejected even as operands, and other options are rejected because unmodeled actions or output operands could bypass syntax analysis or write outputs. See the [Clang option grammar](https://clang.llvm.org/docs/ClangCommandLineReference.html).
- [Quick syntax checks](../../tools/check_compile.py) reject `-save-temps=` and `--save-temps=` values other than `cwd` and `obj` instead of passing them to the compiler: an unknown value fails on GCC, while newer Clang builds take any value as an output prefix and write preprocessed files even under `-fsyntax-only` (measured on Apple clang 21), so a passed-through mode either fails the check on one compiler or silently writes outputs on the other.

- Focus pause for rumble, not verified on hardware: real controllers (DualSense, Xbox, generic) and real focus changes on Windows, macOS and Linux have not been tested. The console-side behaviour is not measured; the pause is a host-side comfort setting and is not a console behaviour.
