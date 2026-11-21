# libSceAgc

## add Functional

- [sceAgcCreateShader](../../../core/libs/prx/libSceAgc/Shader/src/CreateShader.cpp) (libSceAgc) - dst output pointer is validated upfront with GuestWritable_nid_no_patch and populated through GuestCopyOut_nid_no_patch with caller errno preserved; throwing runtime_error on unwritable pointer without mutating shader header or registering with driver is host safety fallback with console behavior unmeasured.
