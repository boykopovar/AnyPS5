# libSceAgc

## add Functional

- [sceAgcLinkShaders](../../../core/libs/prx/libSceAgc/Shader/src/LinkShaders.cpp) (libSceAgc) - reading the context and primitive outputs and the vertex and pixel shader headers through `GuestCopyIn_nid_no_patch`, and writing the outputs through `GuestCopyOut_nid_no_patch` (restoring the context registers if the primitive registers cannot be written), returning `GRAPHICS5_ERROR_INVALID_SHADER_PROGRAM`, its result for a null pointer, with `errno` preserved on an inaccessible pointer, is a host-safety fallback: the console's result for such a pointer is not measured. The special-register and semantic tables the headers point to are still read directly.
