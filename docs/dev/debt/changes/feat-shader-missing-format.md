# shader

## add Functional

- [Signed packed images](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) (GFX10 codes 31, 35, 38, 42, 45 and 49) have no native AMD or PS5 measurement. Their codes follow AMD's RDNA 1 ISA §8.2.8, Table 47, and Mesa's [`gfx10-rsrc.json`](https://gitlab.freedesktop.org/mesa/mesa/-/blob/418c4cfa6708a0e0b1175e72fb8fd27d3ca1615a/src/amd/registers/gfx10-rsrc.json); the [host views](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp) use R32_UINT and the shader uses the existing typed-buffer signed conversion for loads and stores. Store saturation, SNORM rounding and NaN conversion, D16 SNORM loads through f32 then nearest-even f16, and partial-store zero fill are not measured on console hardware. Sampling, gathers, LOD queries, image tables, packed accesses and selecting the missing W of a three-component format throw. Render targets are not added.
