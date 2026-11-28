# libSceAgcDriver

## change [PA_CL_CLIP_CNTL]

- [PA_CL_CLIP_CNTL](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver) - `CLIP_DISABLE` (bit 16) and `DX_LINEAR_ATTR_CLIP_ENA` (bit 24) are accepted without changing pipeline state: Vulkan clips primitives against viewport guard bands and clips `NoPerspective` outputs linearly; what clearing bit 24 or setting bit 16 changes on the console beyond standard clipping is undocumented. `UCP_CULL_ONLY_ENA` (bit 17) is accepted without changing pipeline state when `UCP_ENA_0..5` (bits 0..5, still rejected) and `PA_CL_VS_OUT_CNTL` `CLIP_DIST_ENA_0..7` (bits 0..7) are clear; whether bit 17 also affects shader clip distances on the console is unverified, so draws with any `CLIP_DIST_ENA` bit set still throw.
