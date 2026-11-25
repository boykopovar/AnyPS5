# libSceAgcDriver

## add Functional

- [Opaque black borders under a T# DST_SEL that moves alpha](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver) - RDNA2 (gfx1036 with the [hardware oracle](../HW_ORACLE.md)) applies the T# DST_SEL to the border color like a texel: an 8_8_8_8_UNORM or 8_8_8_8_UINT texture with DST_SEL W,Z,Y,X returns (1, 0, 0, 0) for an opaque black border (`BORDER_COLOR_TYPE` 1, `CLAMP_BORDER`, BC_SWIZZLE 0). AnyPS5 maps the DST_SEL to the image view's component swizzle and does not enable VK_EXT_border_color_swizzle, so Vulkan leaves this border undefined. AMD's Windows driver swizzles it like RDNA2; NVIDIA's (RTX 5080, 610.74), whose `borderColorSwizzleFromImage` is false, returns (0, 0, 0, 1). Transparent black and opaque white borders, and DST_SELs that keep alpha in place, match on all three.
