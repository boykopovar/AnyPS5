# libSceOpusCeltEnc

## add Functional

- [sceOpusCeltMultistreamEncCreate, sceOpusCeltMultistreamEncCreateEx, and sceOpusCeltMultistreamEncCtl](../../../core/libs/prx/libSceOpusCeltEnc/Export.cpp) - Implemented via `libopus` multistream encoder matching firmware 13.60 672-byte (`0x2A0`) state layout and verified `4000..4047` / `-1` CTL requests; forwarded CTL setters check `libopus` before updating guest state and return the request's defined error code (e.g. `0x813F000C` for force-channels 2 on uncoupled streams) while preserving guest state and the `+0x9C` dirty flag on `libopus` rejection (console behavior for `libopus`-rejected values is unmeasured); `OPUS_RESET_STATE` checks `libopus` before resetting state; multistream encode and destroy exports are not yet implemented.
