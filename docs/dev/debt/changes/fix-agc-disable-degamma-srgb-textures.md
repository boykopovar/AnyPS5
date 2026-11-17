# libSceAgcDriver

## change [DISABLE_DEGAMMA]

- [DISABLE_DEGAMMA](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver) - S# word 0 bit 31 (`SKIP_DEGAMMA` on RDNA 2, Mesa `src/amd/registers/gfx10-rsrc.json` field `SKIP_DEGAMMA` in `SQ_IMG_SAMP_WORD0`, `ac_descriptors.c`) disables sRGB-to-linear conversion when sampling `_SRGB` formats. Not measured on a PS5. On an RX 6650 XT (gfx1032, `tools/hw-oracle`) `image_sample` through 8_SRGB, 8_8_SRGB and 8_8_8_8_SRGB with the bit returns exactly what 8_UNORM, 8_8_UNORM and 8_8_8_8_UNORM return, and the bit changes nothing on those UNORM formats. A direct, sampled-only image of those three sRGB formats whose every paired sampler sets the bit is therefore [materialized as UNORM](../../../core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp). Mixed sampler states, the BC sRGB formats and table images still throw (`RequireDisablesDegammaOn`).
