# libSceSystemLogger2Game

## change [libSceSystemLogger2Game additional exports]

- [libSceSystemLogger2Game additional exports](../../../core/libs/prx/libSceSystemLogger2Game/Export.cpp) - these name/NID pairs are present in the 13.60 export inventory. `sceSystemLogger2GameGetTimestamp` takes a `SceSystemLogger2Timestamp*` and writes the same 0x30-byte secure-clock timestamp as `libSceSystemLogger2`; the local backend does not model this module's IPMI client initialization. `sceSystemLogger2GameSetEvent` still has an unverified signature and behavior, remains an import-resolution placeholder and throws through `NotImplemented_nid_no_patch`. The timestamp pointer and output layout were read from firmware 13.42 disassembly, not tested on a console
