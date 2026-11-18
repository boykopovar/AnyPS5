# libSceAgcDriver

## add Functional

- [Missing texture channels](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp) (libSceAgcDriver) - a T# destination select that names a channel its format lacks reads channel (index mod channel count) for the one-channel 8, 16 and 32-bit formats, and the two-channel 8_8, 16_16 and 32_32 formats sampled through a view of their own layout, instead of the 0 and 1 a Vulkan view supplies. Not measured on a PS5. On an RX 6650 XT (gfx1032, `tools/hw-oracle`), `image_sample_lz` with identity selects and the result registers preset to other values writes every requested component: 8_UNORM, 8_UINT, 16_UNORM, 16_FLOAT and 32_FLOAT return (x, x, x, x), 8_8_UNORM, 16_16_UNORM and 16_16_FLOAT return (x, y, x, y), and 11_11_10_FLOAT returns (x, y, z, x). Converted formats (11_11_10_FLOAT and the others decoded in the shader from a 32-bit view), 5_6_5, the packed formats and block-compressed formats keep the existing fill.
