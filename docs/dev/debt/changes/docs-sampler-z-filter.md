# libSceAgcDriver

## add Functional

- [S# Z_FILTER](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver, word 2 bits 24-25) is not decoded: a Vulkan sampler filters every axis of a 3D texture with its min and mag filters, so the third axis always follows the S#'s XY filter. On RDNA2 (gfx1036 with the [hardware oracle](../HW_ORACLE.md), 4x4x4 32_FLOAT, `image_sample_lz` with DIM 3D) Z_FILTER 1 samples the nearest slice and 2 blends the two nearest slices whatever the XY filter is, and 0 and 3 follow the XY filter. A bilinear S# with Z_FILTER 1 therefore blends slices on both AnyPS5 hosts where RDNA2 does not (126.25 instead of 101.25 a quarter slice past slice 1), and a point S# with Z_FILTER 2 reads one slice where RDNA2 blends (101 instead of 126). The other six combinations match on an RTX 5080 (610.74) and on gfx1036 itself.
