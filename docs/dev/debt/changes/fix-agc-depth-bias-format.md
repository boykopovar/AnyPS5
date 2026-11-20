# libSceAgcDriver

## add Functional

- [Depth bias constant across unit formats](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver) - two `PA_SU_POLY_OFFSET_DB_FMT_CNTL` cases are approximate: `0x1e9` (float units, 23 bits) on D16 is scaled by 1/256, and `0xf0` (16-bit units) on D32F by 256 on a device whose `VK_EXT_depth_bias_control` has neither the force-unorm representation with `depthBiasExact` nor the float one; with either, `0xf0` on D32F is exact. The Vulkan float unit is 2^(e-23) for the primitive's largest-depth exponent e, so a fixed factor is exact only for a largest depth in [0.5, 1), and each halving below 0.5 doubles the difference. `0x1e9` on D16 has no exact Vulkan representation without a D32F proxy. Slope is unaffected. The Witcher 3 (PPSA03977) uses `0x1e9` over D16; the approximation lifts the abort, and console equivalence is unmeasured.
