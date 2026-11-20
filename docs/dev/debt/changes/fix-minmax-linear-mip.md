# libSceAgcDriver

## change [Min/max S# reduction]

- [Min/max S# reduction](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver) - `FILTER_MODE` 1 and 2 use Vulkan's min and max reduction (the same `SQ_IMG_FILTER_MODE` values RADV programs). Measured on RDNA2 (gfx1035), not on the console: only texels of non-zero weight are reduced, and the weight is taken after rounding to the 8-bit subtexel grid (a sample within 1/512 texel of a centre reduces that texel alone); a driver with other `subTexelPrecisionBits` reduces a different set there. With a linear mip filter, gfx1036 ([hardware oracle](../HW_ORACLE.md)) and AMD's Windows driver reduce both levels' texels together, a level counting when its mip weight rounded to 1/256 is non-zero; a host whose mip weights differ (see `Mip selection and blending on NVIDIA`) drops other levels. Gathers ignore the mode.
