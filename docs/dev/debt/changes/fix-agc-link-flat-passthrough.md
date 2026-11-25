# libSceAgc

## add Functional

- [sceAgcLinkShaders](../../../core/libs/prx/libSceAgc/Shader/src/LinkShaders.cpp) (libSceAgc) - maps a flat pixel input as pass-through (`FLAT_SHADE` and `OFFSET` bit 5), as the console's `sceAgcLinkShaders` and `sceAgcCreateInterpolantMapping` did on firmware 13.00 (a flat input semantic linked to hardware slot 0 gave 0x420, to slot 2 gave 0x422). Flat 16-bit inputs and flat inputs without a matching vertex output were not measured. The console writes the 32 interpolant registers first and `VGT_SHADER_STAGES_EN` and `VGT_GS_OUT_PRIM_TYPE` after them; this implementation writes those two first, which matters only to a caller that reads the array by position.
