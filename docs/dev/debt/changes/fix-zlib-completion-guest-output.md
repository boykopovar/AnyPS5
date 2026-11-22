# libSceZlib

## change [libSceZlib]

- [libSceZlib](../../../core/libs/prx/libSceZlib/Export.cpp) is unverified on PS5 for several outstanding requests in submission order, repeated `sceZlibGetResult`, `0x81120002` for an unknown ID, `0x81120027` on timeout and results dropped by `sceZlibFinalize`: they follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/0fe263a4760dfbfa973366890061749b4af0de97/src/core/libraries/zlib/zlib.cpp) (PS4), and the microsecond timeout follows SharpProspero. `0x8112000B` (not ready) is never returned: requests finish at submission. A wait interrupted by `sceZlibFinalize` and non-null work memory throw. Native validation confirms the 64-bit initialization length, 2 KiB output granularity up to 64 KiB, null-argument errors and initialization precedence. An unwritable `sceZlibGetResult` output returns `0x81120016`. Driver scheduling, cancellation and scratch memory are unverified; malformed streams map to `0x811200FF`, exhausted output to `0x8112001C`.

## add Functional

- [`sceZlibWaitForDone`](../../../core/libs/prx/libSceZlib/Export.cpp) returns `0x81120016` for an unreadable timeout or unwritable completion ID as a host-safety fallback. Console behavior is unmeasured. The timeout is read before waiting, and output memory is checked only when a completion is ready. The full output range is checked before copying the ID. A concurrent mapping change between the check and the copy may still cause a partial write. A failed copy leaves the completion queued and wakes another waiter.
