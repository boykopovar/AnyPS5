# libSceSystemLogger2 technical debt

### Console service

- [libSceSystemLogger2 source](../../../core/libs/prx/libSceSystemLogger2/Export.cpp) (libSceSystemLogger2) - read from the firmware 13.40 module (SHA-256 `934AE1CDDE8BC66200EF2891923C53684B70D5BEB600744774A73A190C347485`), not run against the console logger. The logger daemon and its monitor singleton are absent, so `sceSystemLogger2IsAvailableMonitor` and `sceSystemLogger2GetEventStatus` return `0x818C000E` after the native argument checks; that code is the not-available result of `IsAvailableMonitor` and an inference for `GetEventStatus`. `sceSystemLogger2Initialize` only sets the initialized flag: the native `0x818C0003` allocation failure and its subsystem probe are not reproduced. `sceSystemLogger2GetTimestamp` always reports `networkClock` with the wall clock in microseconds since 0001-01-01; the `secureClock` and `NONE` fallbacks (taken when the network clock is unset) are unreachable here. `sceSystemLogger2TerminateWithResourceRelease` clears the flag; `sceSystemLogger2Terminate` is `xor %eax,%eax; ret` in 13.40 and keeps it.
- [Event submission](../../../core/libs/prx/libSceSystemLogger2/Export.cpp) (libSceSystemLogger2) - the five `sceSystemLogger2Set*EventAsync` entry points validate the event record as 13.40 does and then return `0x818C000E`; nothing is delivered, queued or written to the console's IPMI client. The checks are the string at `+0x00` (and `+0xD0`, `+0x168` or `+0x4D0`) shorter than `0x81` bytes, the clock source at `+0x90` equal to `networkClock`, `secureClock` or `NONE`, the entry count at `+0xB8` at most 4, entry names at `+0xD0` with stride `0x28` at most `0x23` bytes, entry languages at `+0xD8` at most 2 bytes, the detail string at `+0xCC` at most `0x400` bytes, and the RNPS token at `+0x158` non-null. Rejected records return `0x818C0002`, calls before `Initialize` return `0x818C0004`; `SetEventAsyncInternal` zeroes its status word once the record is accepted. Whether a title needs delivery, and what the daemon returns when the console queue is full, remain unmeasured.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceSystemLogger2/Export.cpp) - 3 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `PcjxaaFQATU` |
| `Unknown` | `UI7zoVuRjdU` |
| `Unknown` | `eI4wpp+8Bco` |
