# libSceOpusCeltEnc

## add Functional

- [sceOpusCeltMultistreamEncCreate, sceOpusCeltMultistreamEncCreateEx, and sceOpusCeltMultistreamEncCtl](../../../core/libs/prx/libSceOpusCeltEnc/Export.cpp) - Implemented via `libopus` multistream encoder matching firmware 13.60 672-byte (`0x2A0`) state layout and verified `4000..4047` / `-1` CTL requests; multistream encode and destroy exports are not yet implemented.
