# shader

## add Functional

- [An image_load of a T# the same shader stores to](../../../core/shader/recompiler/Optimization/src/ResourceTracker.cpp) is a separate sampled resource: `ImageKey` keys images by the opcode's class, so `ImageRead` (sampled) and `ImageWrite` (storage) of one source get two bindings, and the read goes through a view of the cached storage image or a snapshot that Vulkan does not keep coherent with the dispatch's own storage writes. A load after a store of the same texel can return the texel from before the dispatch. RDNA2 (gfx1036) hardware returns the stored value, with and without `glc`. Through the hardware oracle and `agc_oracle_runner`, with the image pre-filled with 0xAB, AnyPS5 returned 0xAB bytes for every format measured on an NVIDIA GeForce RTX 5080 (driver 610.74), and on gfx1036 (AMD Windows driver 32.0.21045.16010) for the one-channel formats and some two-channel ones; four-channel formats read back there. The PS5's gfx1013 is not measured.
