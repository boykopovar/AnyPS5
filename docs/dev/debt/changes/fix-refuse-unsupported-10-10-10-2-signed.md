# libSceAgcDriver

## add Functional

- [10_10_10_2 SNORM and SINT textures](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp) (libSceAgcDriver, guest formats 51 and 55) sample through `A2B10G10R10_SNORM_PACK32` and `A2B10G10R10_SINT_PACK32`, optional Vulkan formats, and throw `unsupported guest texture format` on a device that cannot sample them; there is no emulated path. Neither an NVIDIA GeForce RTX 5080 (driver 610.74) nor gfx1036 (AMD Windows driver 32.0.21045.16010) reports any feature for them. On gfx1036 hardware `image_load` decodes X, Y and Z as signed 10-bit fields (SNORM c / 511 with -512 giving -1.0; SINT sign-extended) and W as a signed 2-bit field (SNORM 1 is 1.0, -2 and -1 are -1.0; SINT -2 and -1). The PS5's gfx1013 is not measured.
