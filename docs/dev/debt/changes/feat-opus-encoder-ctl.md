# libSceOpusCeltEnc

## change [sceOpusCeltEncCtl, sceOpusCeltMultistreamEncCreate, sceOpusCeltMultistreamEncCtl and sceOpusCeltMultistreamEncCreateEx]

- [sceOpusCeltMultistreamEncCreate, sceOpusCeltMultistreamEncCtl and sceOpusCeltMultistreamEncCreateEx](../../../core/libs/prx/libSceOpusCeltEnc/Export.cpp) answer only the checks firmware 13.42 makes before it uses the state, read from disassembly, not measured on a console, and throw through `NotImplemented_nid_no_patch` past them: `MultistreamEncCtl` dispatches requests 4000 to 4047 and -1 through a table that was not read, and the create calls need a multistream encoder, which this implementation does not create. The declarations carry only the arguments those checks read, and their names are mine.

## add Unknown function info

- [sceOpusCeltEncCtl](../../../core/libs/prx/libSceOpusCeltEnc/Export.cpp) (libSceOpusCeltEnc) - which requests the firmware accepts past its 13.42 checks, their error codes, and whether it rewrites the bitrate words at state +0x1C and +0xC0 are unknown. On a PS5 (firmware 11.60, payload process; a title process may differ) a zero-filled 512 byte state got `0x813F0016` for -1, 4000 to 4003, 4028 and 4031 and a null state `0x813F0001`, no write (prototype assumed, n=1), the state checked first as here. `OPUS_RESET_STATE` and the encoder SET requests of the pinned [libopus `opus_defines.h`](https://github.com/xiph/opus/blob/22244de5a79bd1d6d623c32e72bf1954b56235be/include/opus_defines.h) go to `opus_encoder_ctl` on the state's encoder. Every GET throws: libopus answers defaults; the console's answer is unknown. `OPUS_SET_APPLICATION` (2051 is created as libopus's 2053), the unassigned and decoder-only numbers, -1, -2 and values libopus refuses throw too.

# libSceOpusSilkEnc

## remove [sceOpusSilkEncCtl and sceOpusSilkEncCancel]

## add Functional

- [sceOpusSilkEncCancel](../../../core/libs/prx/libSceOpusSilkEnc/Export.cpp) (libSceOpusSilkEnc) - forwards to `sceAjmiBatchCancel` with the 32-bit word at state +0xF0, as firmware 13.42 disassembly indicates. A null state returns `0x81770001` before reading the state; inaccessible state throws preserving `errno`. A zero or uninitialized AJM context returns `SCE_AJM_ERROR_INVALID_CONTEXT`. On a PS5 (firmware 11.60, payload process) a zero-filled 512 byte state returned `0x80930001` with errno 49. Batch cancellation against a live AJM context remains unmeasured on console.

## add Unknown function info

- [sceOpusSilkEncCtl](../../../core/libs/prx/libSceOpusSilkEnc/Export.cpp) (libSceOpusSilkEnc) - which requests the firmware accepts past its 13.42 checks, their error codes, and whether it rewrites the bitrate words at state +0x1C and +0xDC are unknown. On a PS5 (firmware 11.60, payload process; a title process may differ) a zero-filled 512 byte state got `0x81770015` for -1, 4000 to 4003, 4028 and 4031 and a null state `0x81770001`, no write (prototype assumed, n=1), the state checked first as here. `OPUS_RESET_STATE` and the encoder SET requests up to 4031 of the pinned [libopus `opus_defines.h`](https://github.com/xiph/opus/blob/22244de5a79bd1d6d623c32e72bf1954b56235be/include/opus_defines.h) go to `opus_encoder_ctl` on the state's encoder. Every GET throws: libopus answers defaults; the console's answer is unknown. `OPUS_SET_APPLICATION` (2048 is created as libopus's 2052), the unassigned numbers, -2, -1, 11002, 11003 and values libopus refuses throw too.
