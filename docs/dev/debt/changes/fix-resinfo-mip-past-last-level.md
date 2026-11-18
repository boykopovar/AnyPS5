# shader

## add Functional

- [`image_get_resinfo` at a mip past the T#'s last level](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) returns 0 for width, height and depth or layers and the level count in the fourth component, as measured with the [hardware oracle](../HW_ORACLE.md) on RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010) for 1D, 2D, 3D, 2D-array and 1D-array T#s with 1 to 7 levels, base levels 0 to 2 and mips up to 15 and 0xffffffff; the PS5's gfx1013 is not measured. The query reads level 0 and selects zeros, because Vulkan leaves `OpImageQuerySizeLod` undefined past the view's levels (AMD's driver returned the halved size of the last level there).
