# libSceOpusCeltEnc

## add Functional

- [sceOpusCeltMultistreamEncCreate, sceOpusCeltMultistreamEncCreateEx, and sceOpusCeltMultistreamEncCtl](../../../core/libs/prx/libSceOpusCeltEnc/Export.cpp) - Implemented via `libopus` multistream encoder matching firmware 13.60 672-byte (`0x2A0`) state layout and verified `4000..4047` / `-1` CTL requests; forwarded CTL setters and `OPUS_RESET_STATE` check the `libopus` result before updating guest state and throw `std::runtime_error` on `libopus` rejection (such as force-channels 2 on uncoupled streams) because console behavior for `libopus`-rejected values is unmeasured; multistream encode and destroy exports are not yet implemented.
