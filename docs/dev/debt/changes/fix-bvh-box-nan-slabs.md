# shader

## add Functional

- [image_bvh_intersect_ray boxes](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) (`EmitImageBvhIntersectRay`) follow RDNA2 (gfx1036, AMD driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) for NaN slab times: a plane whose time is NaN, from a ray parallel to an axis whose origin lies on that plane or from a NaN bound, does not limit the box; a box with no non-NaN entry or exit time misses; and a NaN in the ray's origin or inverse direction misses every child. On 1,536 random float32 and float16 box queries (sorted and unsorted, box grow 0 to 255, axis-parallel rays from whole-number origins) every child pointer matched the hardware. The PS5's gfx1013 is not measured.
