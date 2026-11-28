# libSceAgcDriver

## add Functional

- [Depth bias constant across unit formats](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver) - `PA_SU_POLY_OFFSET_DB_FMT_CNTL` units that differ from the target's (`0x1e9`, float units with 23 bits, on D16; `0xf0`, 16-bit units, on D32F) convert the front and back constants by a fixed 1/256 or 256. For float units the Vulkan unit is 2^(e-23) for the primitive's largest-depth exponent e, so the exact factor is 2^(e-7): the fixed factor is exact only for a largest depth in [0.5, 1), and each halving below 0.5 doubles the difference. `0xf0` on D32F can be exact with `VK_EXT_depth_bias_control`'s force-unorm representation, which is not used; `0x1e9` on D16 has no exact Vulkan representation without a D32F proxy. Slope is unaffected. The Witcher 3 (PPSA03977) uses `0x1e9` over D16; the approximation lifts the abort, and console equivalence is unmeasured.
