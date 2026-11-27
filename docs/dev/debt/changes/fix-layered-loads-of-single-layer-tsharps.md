# shader

## add Functional

- [Layered accesses through a 1D or 2D T#](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) (shader recompiler) - `image_load`, `image_load_mip`, `image_store` and image atomics with DIM 1D_ARRAY, 2D_ARRAY or CUBE through a 1D or 2D T# access the texel only when the layer or face coordinate is 0: other layers load zero, drop the store and leave an atomic's texel unchanged, returning 0, as DIM 3D accesses through such a T# do for z. Measured on RDNA2 (gfx1036, [hardware oracle](../HW_ORACLE.md)), not the PS5's gfx1013, on 64x64 2D and 64-texel 1D 32_FLOAT T#s with layers 1, 2, 3, 5, 7 and 0xffffffff; with this rule the translation matches every `image_load` and `image_load_mip` row of the T# and DIM combinations it accepts (32 random rows each) on NVIDIA and AMD. Samples clamp the layer instead.
