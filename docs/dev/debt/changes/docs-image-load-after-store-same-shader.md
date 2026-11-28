# shader

## add Functional

- [An image_load of a T# the same shader stores to](../../../core/shader/recompiler/Optimization/src/ResourceTracker.cpp) returns the texel from before the dispatch when the [read through the storage binding](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) is off: different descriptors, registration modules, multisampled, cube, mipmapped, sRGB-decoded, depth and emulated-compare reads, image tables, a swizzle that is not identity, the 16-bit packed formats, an atomic storage image or one merged with a render target, and formats without `VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT`. `ImageKey` gives `ImageRead` and `ImageWrite` of one source separate bindings, and Vulkan does not keep the sampled one coherent with the dispatch's stores. RDNA2 (gfx1036) returns the stored value, with and without `glc`. The redirected read is measured on an NVIDIA GeForce RTX 2060 only; the PS5's gfx1013 is not measured.
