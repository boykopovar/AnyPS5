# shader

## add Functional

- [PS4 (GCN) geometry shaders](../../../core/shader/recompiler/Optimization/src/GeometryLowering.cpp) (shader recompiler, PS4) - not measured on a console. The ES is drawn as a Vulkan vertex shader and the GS as a Vulkan geometry shader; ESGS ring stores and reads become the record between them, and GSVS ring stores go to a per-invocation array that each emit copies out through the copy VS's exports. A copy VS export that is neither a GSVS ring dword nor a constant, an ESGS read whose vertex is not a constant input vertex or that loads more than one dword, an ESGS store at a per-lane address, LDS in the GS, and emits to vertex streams 1-3 throw. A GSVS store beyond the ring item is clamped to its last dword; on the console it would land in a neighbouring primitive's item.

# libSceAgcDriver

## add Functional

- [PS4 (GCN) ES-GS-VS routing](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver, PS4) - not measured on a console. `VGT_SHADER_STAGES_EN` 0xb0 is drawn when `VGT_GS_MODE` is `GS_SCENARIO_G` off chip. On-chip GS (`VGT_GS_MODE.ONCHIP`), ES pass-through, the compute modes, `ELEMENT_INFO_EN`, `SUPPRESS_CUTS`, stream-out, vertex streams 1-3, point output (`VGT_GS_OUT_PRIM_TYPE` POINTLIST), adjacency, rectangle, quad and patch input primitives, LS-HS-ES-GS routing and viewport depth remapping through a geometry shader throw. The copy VS is read once per draw decode; a title that rewrites it in place at the same address keeps the old layout until the draw registers change.
