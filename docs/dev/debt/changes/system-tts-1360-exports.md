# libSceSystemTts

## change aIz2UUh6oi0
- [SystemTts export `aIz2UUh6oi0`](../../../core/libs/prx/libSceSystemTts/Export.cpp) validates `outStatus` (`0x819A1001` on null/fault), zeroes it, opens `"SceSystemTtsStatus"` via `sceKernelOpenEventFlag`, waits 100 ms with `WAITMODE_OR`, and writes `1` if any of bits 0..3 (`0xf`) is set, matching 13.60 disassembly; the Arbitrator status publisher remains unimplemented. Labels: firmware, tested.

## change sceSystemTtsConvertText, sceSystemTtsPauseResume, sceSystemTtsRepeat, sceSystemTtsWait
- [sceSystemTtsConvertText, sceSystemTtsPauseResume, sceSystemTtsRepeat, sceSystemTtsWait](../../../core/libs/prx/libSceSystemTts/Export.cpp) follow 13.60 disassembly: `sceSystemTtsWait` returns `0x819A1008` when `speechId` is absent, `sceSystemTtsConvertText` validates output pointers and `SpeakParam` (`0x819A1001`) before throwing, and `sceSystemTtsPauseResume`/`sceSystemTtsRepeat` throw prior to Arbitrator IPMI IPC. Labels: firmware, tested.

## change sceSystemTtsCancel, sceSystemTtsIsAccessibilityAvailable, sceSystemTtsRegisterCallback, sceSystemTtsSpeakRequest
- [sceSystemTtsCancel, sceSystemTtsIsAccessibilityAvailable, sceSystemTtsRegisterCallback, sceSystemTtsSpeakRequest and sceSystemTtsUnregisterCallback](../../../core/libs/prx/libSceSystemTts/Export.cpp) follow 13.60 disassembly for callback map management (`0x819A1001` on null callback, non-overwriting insert, `0` on unregister), active item lookup (`0x819A1008` on `Cancel`), and `SpeakParam`/language validation (`0x819A1001`) prior to unimplemented IPC. Labels: firmware, tested.
