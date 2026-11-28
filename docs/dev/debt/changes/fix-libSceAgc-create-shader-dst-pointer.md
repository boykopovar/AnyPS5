# libSceAgc

## add Functional

- [sceAgcCreateShader](../../../core/libs/prx/libSceAgc/Shader/src/CreateShader.cpp) (libSceAgc) - dst output pointer is validated upfront under `GuestArena::HostWrite` with `GuestWritable_nid_no_patch` and populated via `GuestCopyOut_nid_no_patch` before `AgcDriverRegisterShader_nid_postfix` with caller `errno` preserved; throwing `runtime_error` on unwritable pointer without mutating the shader header or registering the shader is a host-safety fallback with console behavior unmeasured. Closes when: measured on hardware. Labels: console, tested.
