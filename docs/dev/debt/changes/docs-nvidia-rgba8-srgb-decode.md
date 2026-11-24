# libSceAgcDriver

## add Functional

- [8_8_8_8_SRGB textures](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp) (libSceAgcDriver) are sampled as R8G8B8A8_SRGB with the device's own decode. RDNA2 (gfx1036 with the [hardware oracle](../HW_ORACLE.md)) returns the sRGB EOTF at bf16 precision, and AMD's Windows driver matches it in all 256 codes. NVIDIA's Windows driver (RTX 5080, 610.74) returns one bf16 step more in the red, green and blue channels for 29 of the 256 codes (1-12, 15, 19, 20, 32, 33, 65, 66, 68, 70, 97, 184, 186 and 249-253; code 1 reads 0x39a00000 instead of 0x399f0000), the same codes in which its R8_SRGB decode differs, measured with point `image_sample_lz`. Alpha matches. The bf16 shader decode of `APS5_SRGB_SHADER_DECODE=1` covers only 8_SRGB and 8_8_SRGB.
