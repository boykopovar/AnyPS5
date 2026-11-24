# shader

## add Functional

- [image_get_lod on NVIDIA](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) returns the host's `OpImageQueryLod`. RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) and AMD's Windows driver return log2 of the footprint with its fraction rounded to 1/256, so a 1.125-texel step gives 44/256 and a 3-texel step 1.5859 in both components with a linear mip filter. An NVIDIA GeForce RTX 5080 (610.74, default driver settings) returns the whole part plus min(1, 1.75 × that fraction), truncated to 1/256, in both the clamped and the unclamped component: 77/256 for the 1.125-texel step and 2.0 for the 3-texel step, the curve it also blends mip levels with. Whole-number LODs match. The PS5's gfx1013 is not measured.
