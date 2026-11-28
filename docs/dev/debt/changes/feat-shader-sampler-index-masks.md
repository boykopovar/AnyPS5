# shader

## change A shader plans at most 64 [samplers]

- A shader plans at most 128 [samplers](../../../core/shader/recompiler/Optimization/src/ResourceTracker.cpp) (`ShaderInfo::MaxSamplers`), at most 32 of them direct (each takes two elements of the 64-element sampler heap, which [`BindingAllocator`](../../../core/shader/recompiler/Optimization/src/BindingAllocator.cpp) holds them to), and at most 256 image/sampler pairs (`ShaderInfo::MaxSampledPairs`); past them it refuses with `sampler resource limit exceeded`, `shader sampler pairs exceed runtime heap capacity` or `sampled image/sampler pair limit exceeded`. Masks indexed by sampler are 128-bit (`SamplerMask`) and masks indexed by sampler heap element 64-bit; the per-sampler specialization constant ranges are 128 IDs wide. Ratchet & Clank: Rift Apart (PPSA01474) plans up to 63 samplers and 112 pairs in one shader, and Ghostrunner (PPSA03685) 65 in one compute shader (4 direct).
