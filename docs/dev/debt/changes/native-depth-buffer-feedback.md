# libSceAgcDriver

## add Functional

- [Writable buffer/native depth feedback](../../../core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp) is not implemented. Declared writable buffer ranges overlapping a native multisample depth attachment's depth, stencil or HTILE, or a statically described sampled native depth plane/HTILE in the same work, throw before pipeline creation or recording. Sampled overlap is checked before guest publication can retire its native identity. Separate supported D32 publication and buffer accesses remain available. Dynamically addressed BDA stores are outside these declared-range checks; no general feedback barrier or concurrent alias implementation is provided.
