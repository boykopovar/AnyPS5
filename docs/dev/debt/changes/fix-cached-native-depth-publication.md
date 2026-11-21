# libSceAgcDriver

## change [Shader buffer access to native multisample targets]

- [Shader buffer access to native multisample targets](../../../core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp) - Shader buffer descriptors retain the existing native color/depth guest-access refusal during preparation, completion and binding, including cached bindings and moved draw ranges. The check covers logical descriptor ranges, not larger import windows; dynamically addressed BDA heaps and vertex/index input snapshots remain outside this check. Native color buffer publication and console equivalence remain unimplemented or unmeasured; supported D32/D32S8 publication is described above.
