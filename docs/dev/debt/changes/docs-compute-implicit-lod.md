# shader

## add Functional

- [Implicit-LOD samples outside pixel shaders](../../../core/shader/recompiler/IntermediateRepresentation/include/IntermediateRepresentation/IrMetadata/Resources.hpp) (`image_sample` without a derivative, LOD or LZ form) are taken at LOD 0. In a compute shader RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) derives their LOD from each quad of four lanes, lane 1 as the x and lane 2 as the y neighbour, as `image_get_lod` does: a quad whose x neighbour is 3 texels away reads level 2 with a point mip filter and blends levels 1 and 2 at 0.586 with a linear one, where AnyPS5 reads level 0 on an NVIDIA GeForce RTX 5080 (610.74) and on gfx1036 through Vulkan. Other non-pixel stages and the PS5's gfx1013 are not measured.
