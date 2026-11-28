# shader

## add Functional

- [image_bvh_intersect_ray triangles](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) (`EmitImageBvhIntersectRay`) follow RDNA2 (gfx1036, AMD driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)): node types 0 to 3 are the triangles (v0, v1, v2), (v1, v3, v2), (v2, v3, v4) and (v2, v4, v0); the triangle-id return gives the node's id plus its type; a barycentric-return miss keeps the denominator and the barycentric numerators and sets t to an infinity with the denominator's sign. Hits are Möller-Trumbore in f32, whose last bits of t, denominator and barycentrics can differ from the hardware's (the denominator matched the hardware bit for bit on 1 of 32 random rays); a zero or NaN denominator and the PS5's gfx1013 are not measured, and box nodes are not compared here.
