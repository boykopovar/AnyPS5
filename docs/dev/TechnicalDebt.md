# Project technical debt

Entries about one library or component are in [debt](debt/), one file per library or component, under the same sections as this page. Entries that belong to none stay here.

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
- Acm, GameLiveStreaming, Net, NpEntitlementAccess, VoiceQoS, WebBrowserDialog and libkernel exports imported by PPSA14632 and added without an implementation are declared without parameters

### Functional

- Controller haptic PCM drives Xbox-style motor rumble from its per-channel RMS (large/small motor, scale 2x, below 0.01 ignored, 80 ms hold); the mapping approximates waveform energy and is not measured against console hardware
- [Cancellation coverage](ARCHITECTURE.md#thread-cancellation) checks deferred request handling and recovery, not full asynchronous support or console ABI compatibility. Cancellation during file/socket I/O, sleep interruption, cancellation versus exit/detach, condition signal conservation under cancellation, and safe rejection of non-null invalid or reclaimed thread handles remain unverified. No console title or homebrew establishes the cleanup NIDs or buffer layout. Accepted asynchronous mode currently has no diagnostic for its deferred behavior.
- `--to-intel` guest module trampolines are covered only by a synthetic relinker test; no game title has been verified with them on Linux or Windows.
- Guest `TZ` changes reach libc `tzset`, `localtime`, `localtime_s`, `ctime`, `mktime` and `strftime` by rewriting the host process `TZ`, so they also shift every other host consumer of `TZ` (for example libSceRtc local time) once a guest changes it. The zone switch and the conversion run under one lock, but host code outside libc does not take it. The zone grammar is the host C runtime's, not FreeBSD's: Windows accepts only the `std offset [dst]` form and treats an empty guest `TZ` as `UTC0`, a `:Europe/Berlin` style name is read as a zone abbreviation there and yields a wrong zone name (for example `rop`), and zone file names are honoured on Linux only. A guest `TZ` that the guest environment accepts but the host rejects (on Windows, longer than the host environment limit) makes the time functions throw `std::runtime_error`. `tm_zone` of a `tm` computed before a `TZ` change can point at storage that now holds the new zone name, so its `%Z` output is stale or changed.
