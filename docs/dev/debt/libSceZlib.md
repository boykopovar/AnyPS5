# libSceZlib technical debt

### Functional

- [libSceZlib](../../../core/libs/prx/libSceZlib/Export.cpp) is unverified on PS5 for several outstanding requests in submission order, repeated `sceZlibGetResult`, `0x81120002` for an unknown ID, `0x81120027` on timeout and results dropped by `sceZlibFinalize`: they follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/0fe263a4760dfbfa973366890061749b4af0de97/src/core/libraries/zlib/zlib.cpp) (PS4), and the microsecond timeout follows SharpProspero. `0x8112000B` (not ready) is never returned: requests finish at submission. A wait interrupted by `sceZlibFinalize`, non-null work memory and an unaligned destination throw. Native validation confirms the 64-bit initialization length, 2 KiB output granularity up to 64 KiB, null-argument errors and initialization precedence. Driver scheduling, cancellation and scratch memory are unverified; malformed streams map to `0x811200FF`, exhausted output to `0x8112001C`.
