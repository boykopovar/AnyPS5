# shader

## add Functional

- [Table S#s under `image_sample_a`](../../../core/shader/recompiler/Optimization/src/ResourceTracker.cpp) plan their table column from the S# read with every value ORed into dword 3 that sets only reserved bits 12-29 (`samplerDword3ReservedMask`) removed, as a direct S# of the same `_a` read already drops them, so the table entries hold the S# words in memory without those bits. What the console's `_a` read does with bits 12-29 of dword 3 is not measured on gfx1013 or RDNA2; the plain `image_sample` forms still refuse such a dword. PPSA38500 has 336 pixel shaders that read their S# from a material table inside a waterfall loop and OR a value derived from user data into those bits.
