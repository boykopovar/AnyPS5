# libSceVnaInternal

## change VnaInternal handle calls
- [VnaInternal handle calls](../../../core/libs/prx/libSceVnaInternal/Export.cpp) - 77 exports, `sceVnaTerminate` and the four uninitialized `sceVnaUtil*` status/language queries among them, return `0x816F8007` and nothing else. In firmware 13.42/13.60 the 73 `sceVna*` calls look up a handle while `sceVnaInitialize`/`sceVnaInitializeForParty` throw, and the four `sceVnaUtil*` calls check the util refcount while `sceVnaUtilInitialize` throws. Read from disassembly, not measured on a console.

## add Unknown function info
- [sceVnaGetErrorSeverity, sceVnaUtilUpdateVoiceInputStatus, sceVnaUtilRemoveVoiceInputStatus](../../../core/libs/prx/libSceVnaInternal/Export.cpp) - severity mapping (`critical`, `major`, `normal`, `info`, `minor`, empty) and 8-byte voice-input status map updates/removals follow `libSceVnaInternal.sprx` disassembly; `GuestCopy*` error handling (`0x816F8004`) is a host-safety fallback unmeasured on console.
