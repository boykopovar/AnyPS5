# shader

## add Functional

- [BVH descriptor type and size](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) (`EmitImageBvhIntersectRay`) - the translation ignores the T#'s type (word 3 bits 28-31) and returns four invalid dwords for a node past the size field. RDNA2 (gfx1036, AMD driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) answers every box and triangle query with four zero dwords when the type is 0 to 7, and tests the node for types 8 to 15 (RADV writes 8). Past the size, box nodes are invalid as in AnyPS5 (a 32-bit box needs its node index below the size, a 16-bit box and a triangle at most the size), but a triangle returns a miss: +inf, 1.0, triangle id 0xffffffff and 0, or +inf, 1.0, 0 and 0 with barycentrics returned. Measured with `image_bvh_intersect_ray` for sizes 0 to 0x300 and node indices 0 to 15. The PS5's gfx1013 is not measured.
