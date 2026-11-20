# libSceAgcDriver

## add Functional

- [DST_SEL of a channel the format lacks](../../../core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp) (libSceAgcDriver) reads 0 for Y or Z and 1 for W, the host view's default. RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) reads channel i mod n of an n-channel format, in `image_load` and point `image_sample_lz`: X,Y,Z,W gives x,x,x,x for 8_UNORM, 16_UNORM, 16_FLOAT, 32_UINT, 32_FLOAT and 8_SRGB, x,y,x,y for 8_8, 16_16, 32_32, 8_8_SRGB and 4_4, and x,y,z,x for 11_11_10_FLOAT, 10_11_11_FLOAT and 9_9_9_5_FLOAT, and W,Z,Y,X on 16_UNORM gives x,x,x,x where AnyPS5 gives 1,0,0,x on an NVIDIA GeForce RTX 5080 (610.74) and on gfx1036 through Vulkan. Four-channel formats, 5_6_5 and selectors 0 and 1 match. Block-compressed formats are not measured, and neither is the PS5's gfx1013.
