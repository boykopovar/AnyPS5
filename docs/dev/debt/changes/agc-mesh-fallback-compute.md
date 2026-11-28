# libSceAgcDriver

## change [Barycentrics without mesh shaders]

- [Barycentrics without mesh shaders](../../../core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/Draw.cpp) (AGC driver) - a pixel shader that reads barycentrics on the mesh path is emulated only on a device with `VK_EXT_mesh_shader`; a device with neither it nor `VK_KHR_fragment_shader_barycentric` (reported on Linux RADV with a GCN 5 GPU) throws that the device has neither the barycentric extension nor mesh shaders, because a geometry stage cannot follow a mesh stage and no other route is implemented. The compute fallback also throws for barycentric fragment inputs until the tessellation barycentrics path lands. `VK_AMD_shader_explicit_vertex_parameter`, which a GCN 5 device on RADV exposes, is an unexplored option.

## add Functional

- [Mesh compute readback cache](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp) (libSceAgcDriver) - `MeshComputeStages` keeps built readback vertex shaders in a static map keyed by mesh variant id with no trim or eviction. Fine for a fallback path with few distinct mesh programs; a title cycling many mesh variants would grow it without bound. Follow-up: bound it (LRU or cap with `Require`) and add a metric.

# shader

## add Functional

- [Mesh-path points run as compute](../../../core/shader/recompiler/Recompiler.cpp) (shader recompiler) - a mesh-path draw with point output (`outputPrimitiveSize` 1) that runs through the compute fallback (a device without `VK_EXT_mesh_shader`) throws: the vertex record carries no point size and the readback draws triangle or line lists only. The native mesh stage draws such points natively.
