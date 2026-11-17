# shader

## add Functional

- [PS4 (GCN) geometry shaders](../../../core/shader/recompiler/Optimization/src/GeometryLowering.cpp) (shader recompiler, PS4) - not measured on a console. These throw: a copy VS export that is neither a GSVS ring dword nor a constant, copy VS control flow or EXEC writes, copy VS GSVS reads not spaced `VGT_GS_MAX_VERT_OUT` times 64 bytes apart, ESGS reads of a non-constant input vertex or of more than one dword, ESGS stores at a per-lane address, LDS in the GS, emits to streams 1-3, and GSVS stores at a constant address past `VGT_GS_VERT_ITEMSIZE` times `VGT_GS_MAX_VERT_OUT` dwords. A GSVS store at a computed address past that goes to a spare dword no output reads; on the console it lands in a neighbouring primitive's item. A `v_cmpx` in the copy VS, which writes EXEC implicitly, is not detected.

# libSceAgcDriver

## add Functional

- [PS4 (GCN) ES-GS-VS routing](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver, PS4) - not measured on a console. `VGT_SHADER_STAGES_EN` 0xb0 is drawn when `VGT_GS_MODE` is `GS_SCENARIO_G` off chip. On-chip GS (`VGT_GS_MODE.ONCHIP`), ES pass-through, the compute modes, `ELEMENT_INFO_EN`, `SUPPRESS_CUTS`, stream-out, vertex streams 1-3, point output (`VGT_GS_OUT_PRIM_TYPE` POINTLIST), adjacency, rectangle, quad and patch input primitives, LS-HS-ES-GS routing and viewport depth remapping through a geometry shader throw. The copy VS is read once per draw decode; a title that rewrites it in place at the same address keeps the old layout until the draw registers change.
