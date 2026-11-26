# libSceAgcDriver

## change [Unnormalized S# coordinates]

- [Unnormalized S# coordinates](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver) - `FORCE_UNNORMALIZED` uses a Vulkan `unnormalizedCoordinates` sampler, so the S# LOD range, LOD bias and mip filter are not applied, which is exact only on a one-level view. An anisotropic magnification filter on a plain 1D or 2D image (explicit or implicit LOD, constant offsets) instead [divides the folded coordinates by the base-level size](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) and samples a normalized anisotropic sampler at LOD 0 with edge or border clamping. On RDNA2 (gfx1032, not the PS5's gfx1013) the [hardware oracle](../HW_ORACLE.md) matches this exactly for power-of-two widths and within about 0.01 otherwise. Other ratios, 2D footprints, gradients and gathers are not measured and still refuse.
