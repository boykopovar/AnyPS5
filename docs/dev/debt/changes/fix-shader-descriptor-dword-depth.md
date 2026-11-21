# shader

## add Functional

- [Descriptor web dwords](../../../core/shader/recompiler/Optimization/src/ResourceTracker.cpp) are rebuilt from user data, the SRT and uniform operations up to 16 operations deep; a longer chain still folds onto the first descriptor and faults if a lane selects it. The budget is separate from the 8-level phi search that finds the Phis in them, because a dword assembled by an inlined helper is longer than the search. PPSA07642 (The Last of Us Part I) registers ten pixel shaders whose S# dword 1 is a `v_readfirstlane` of a clamp over an SRT dword, one ordered range select longer than the `convertF32ToI32Saturated` shape, and four of them reach the rebuild.
