# shader

## add Functional

- [2D bias, offset and mip-load accesses through a 1D T#](../../../core/shader/recompiler/Optimization/src/ResourceTracker.cpp) (shader recompiler) - DIM 2D `image_sample_b`, `image_sample_lz_o`, `image_sample_l_o`, `image_sample_b_o` and `image_load_mip` through a 1D T# take the 1D mode of the other 2D accesses through a 1D T#; through a 2D-array view of a 1D T# they still throw. Measured on RDNA2 (gfx1036, [hardware oracle](../HW_ORACLE.md)), not the PS5's gfx1013, on a 64-texel, five-level 32_FLOAT 1D T#: each equals the DIM 1D form on 32 of 32 rows whatever the second coordinate, the y offset or y (even -1) hold, with clamp and wrap addressing. The translated offset and mip-load forms match the hardware on every row on NVIDIA and AMD. The bias forms equal the translated DIM 1D ones; in compute shaders both differ from the hardware, as the implicit LOD is taken as 0.
