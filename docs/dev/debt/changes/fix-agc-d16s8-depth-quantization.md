# libSceAgcDriver

## change [16-bit depth with stencil]

- [16-bit depth with stencil](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver) is stored as `D24_UNORM_S8_UINT`, or `D32_SFLOAT_S8_UINT`, on a device without `D16_UNORM_S8_UINT` as an attachment. Clears, depth bounds and fragment depth writes round to the nearest 16-bit unorm step; a fragment shader with early fragment tests that writes memory throws when depth testing or writing is enabled. The constant depth bias is scaled by 256 for D24, and a non-zero constant on the D32 float fallback throws. Not measured on console hardware.
