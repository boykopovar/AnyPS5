# Project technical debt

Entries about one library or component are in [debt](debt/), one file per library or component, under the same sections as this page. Entries that belong to none stay here. An entry may end with `Closes when:` and the measurement, title or test that would close it, such as `Closes when: measured on a PS5`, so the entries a contributor can re-check are found by searching for it.

### Build

- Building on Windows requires a specific version of mingw - MinGW-w64 GCC 15.2.0 (`winlibs-gcc15`, `x86_64-ucrt-posix-seh`)
- Even compiled prx libraries on Windows require nearby (static linking of these dependencies causes conflicts):
  - libgcc_s_seh-1.dll
  - libstdc++-6.dll
  - libwinpthread-1.dll

### Silent stubs

Throughout the project, every function at every stage either **does exactly what it's supposed to or throws an exception**. Everywhere... except:

### Unknown function info

- AudioIn and NpSessionSignaling exports added without an implementation have assumed signatures
- `sceNetCtlGetIfStat` signature and 56-byte layout come from PPSA09030's call site and SharpProspero's `SceNetCtlIfStat`; the device kind is always 0, the byte counters are the host's totals over its up non-loopback hardware interfaces (not "since cleared"), and the not-connected result is assumed to match `sceNetCtlGetState`
- Font, Http2, Net, Ssl, SystemService and libkernel exports imported by PPSA12544 and added without an implementation have unknown signatures
- The Share, NpTrophy2, NpEntitlementAccess and WebBrowserDialog exports imported by PPSA12544's Unity plugins and added without an implementation have unknown signatures
- Acm, GameLiveStreaming, Net, NpEntitlementAccess, VoiceQoS, WebBrowserDialog and libkernel exports imported by PPSA14632 and added without an implementation are declared without parameters; of the VoiceQoS ones only those still throwing remain so

### Functional

- Controller haptic PCM that no controller sound card plays (a DualSense over Bluetooth, or any other controller) drives motor rumble from its per-channel RMS (large/small motor, scale 2x, below 0.01 ignored, 80 ms hold); the mapping approximates waveform energy and is not measured against console hardware
- [Cancellation coverage](ARCHITECTURE.md#thread-cancellation) checks deferred request handling and recovery, not full asynchronous support or console ABI compatibility. Cancellation during file/socket I/O, sleep interruption, cancellation versus exit/detach, condition signal conservation under cancellation, and safe rejection of non-null invalid or reclaimed thread handles remain unverified. No console title or homebrew establishes the cleanup NIDs or buffer layout. Accepted asynchronous mode currently has no diagnostic for its deferred behavior.
- `--to-intel` guest module trampolines are covered only by a synthetic relinker test; no game title has been verified with them on Linux or Windows.

- [Quick syntax checks](../../tools/check_compile.py) reject unexpanded `@response` arguments rather than parse compiler-specific quoting and risk retaining output flags. Expand response files in `compile_commands.json` before checking; the Ninja fallback expands them with `ninja -t compdb -x`.
- [Quick syntax checks](../../tools/check_compile.py) do not model every compiler option's operand count. A preprocessing mode following an unrecognized option without a joined value is rejected rather than risk removing that option's operand. Compiler wrappers with their own option grammar are not covered. Forwarded frontend actions (`-Xclang`, `-Xpreprocessor` and their joined forms) are rejected. Comma-forwarded options (`-Wp,`) allow only recognized macro/include operands; response-file tokens are rejected even as operands, and other options are rejected because unmodeled actions or output operands could bypass syntax analysis or write outputs. See the [Clang option grammar](https://clang.llvm.org/docs/ClangCommandLineReference.html).
