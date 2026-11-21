# shader

## add Functional

- [Ordered pixel interlock on RADV GFX11](../../../core/shader/recompiler/SpirvBackend/src/SpirvModuleEmitter.cpp) (shader recompiler) - the emitted `PixelInterlockOrderedEXT` module drops one of 64 overlapping interlock writes ~90% of runs on NAVI31 (gfx1100) under RADV (Mesa 26.2.6, [#18770](https://github.com/boykopovar/AnyPS5/issues/18770)); a minimal standalone interlock shader passes there, and so do RDNA2 + RADV, lavapipe, GFX12 + RADV (gfx1201, 30/30 runs) and RDNA3.5 + AMD proprietary, pointing at RADV's GFX11 POPS path, not the GPU-agnostic emission. `APS5_DUMP_SPIRV` dumps the exact module for a standalone harness. A quad-masked ordered pixel shader also kills or demotes inside the interlock region at entry, which the exit paths avoid by emitting `OpEndInvocationInterlockEXT` first; no test combines a quad pixel mask with an ordered pixel shader.
