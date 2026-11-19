# libSceAgcDriver

## add Functional

- [Mip levels past the full chain](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureTiling.cpp): sampled textures and block/depth copies expose only the physical root chain; filtered samples/gathers beyond it read its last level. Unsupported shader accesses retain the refusals in [shader](shader.md); console LOD selection remains unmeasured. Storage textures back each extra 1x1x1 level separately, preserving layers and guest offsets through uploads, storage/color views and writeback. Ownership, pending writes and accounting cover the whole surface. Whole-surface fill/copy optimizations retain their extra-level refusal and use guest-memory operations; tracking remains per layer. Uniform DCC uploads/clears reach all backed levels. Guest tiling and DCC console equivalence remain unverified.

# shader

## add Functional

- [T#s whose levels reach past the full mip chain](../../../core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp) have unrepresented texels; extra-level mip loads and unrepresented-base texel/LOD queries throw. Sampling/gathers still clamp to represented data, with console behavior unverified. Safe physical query LODs retain the decoded guest count: in-count extra levels report 1x1 spatial extents; LODs at or past count zero all extent/layer components and retain count. Pure table size queries are supported. Raw LAST_LEVEL still clamps to MAX_MIP; the reported LAST9/MAX0 count discrepancy is unchanged. Query results agree with [desktop gfx1031 HIP evidence](https://github.com/boykopovar/AnyPS5/pull/6280#issuecomment-6254884569), not PS5 measurements or sampling/gather coverage.
