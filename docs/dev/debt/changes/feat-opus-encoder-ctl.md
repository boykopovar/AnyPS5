# libSceOpusCeltEnc

## change [sceOpusCeltEncCtl, sceOpusCeltMultistreamEncCreate, sceOpusCeltMultistreamEncCtl and sceOpusCeltMultistreamEncCreateEx]

- [sceOpusCeltMultistreamEncCreate, sceOpusCeltMultistreamEncCtl and sceOpusCeltMultistreamEncCreateEx](../../../core/libs/prx/libSceOpusCeltEnc/Export.cpp) answer only the checks firmware 13.42 makes before it uses the state, read from disassembly, not measured on a console, and throw through `NotImplemented_nid_no_patch` past them: `MultistreamEncCtl` dispatches requests 4000 to 4047 and -1 through a table that was not read, and the create calls need a multistream encoder, which this implementation does not create. The declarations carry only the arguments those checks read, and their names are mine.

## add Unknown function info

- [sceOpusCeltEncCtl](../../../core/libs/prx/libSceOpusCeltEnc/Export.cpp) (libSceOpusCeltEnc) - the firmware's dispatch table past its 13.42 checks was not read, so which requests it accepts, their error codes, and whether it also rewrites the bitrate words at state +0x1C and +0xC0 are unknown. `OPUS_RESET_STATE` and the encoder SET requests of the pinned [libopus `opus_defines.h`](https://github.com/xiph/opus/blob/22244de5a79bd1d6d623c32e72bf1954b56235be/include/opus_defines.h) go to `opus_encoder_ctl` on the state's encoder. Every GET throws: libopus answers its own defaults, such as a bitrate of 120000 at 48 kHz stereo where Create writes the measured 99000, and what the console answers is unknown. `OPUS_SET_APPLICATION` (2051 is created as libopus's 2053), the unassigned and decoder-only numbers, -1, -2 and values libopus refuses throw too.

# libSceOpusSilkEnc

## change [sceOpusSilkEncCtl and sceOpusSilkEncCancel]

- [sceOpusSilkEncCancel](../../../core/libs/prx/libSceOpusSilkEnc/Export.cpp) answers only the null-state check firmware 13.42 makes, read from disassembly, not measured on a console, and throws through `NotImplemented_nid_no_patch` past it: it makes a call with the 32-bit word at state +0xF0 as its first argument.

## add Unknown function info

- [sceOpusSilkEncCtl](../../../core/libs/prx/libSceOpusSilkEnc/Export.cpp) (libSceOpusSilkEnc) - the firmware's dispatch code past its 13.42 checks was not read, so which requests it accepts, their error codes, and whether it also rewrites the bitrate words at state +0x1C and +0xDC are unknown. `OPUS_RESET_STATE` and the encoder SET requests up to 4031 of the pinned [libopus `opus_defines.h`](https://github.com/xiph/opus/blob/22244de5a79bd1d6d623c32e72bf1954b56235be/include/opus_defines.h) go to `opus_encoder_ctl` on the state's encoder. Every GET throws: libopus answers its own defaults, such as a bitrate of 120000 at 48 kHz stereo where Create writes the measured 99000, and what the console answers is unknown. `OPUS_SET_APPLICATION` (2048 is created as libopus's 2052), the unassigned numbers, -2, -1, 11002, 11003 and values libopus refuses throw too.
