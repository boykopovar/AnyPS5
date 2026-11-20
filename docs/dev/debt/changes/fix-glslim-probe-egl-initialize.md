# libSceGLSlimVSH

## add Functional

- [eglInitialize](../../../core/libs/prx/libSceGLSlimVSH/src/Egl.cpp) writes the `major` and `minor` version outputs through `GuestCopyOut_nid_no_patch` and returns `EGL_FALSE` with `EGL_BAD_PARAMETER` (0x300c) when a non-null output is unwritable, leaving the display uninitialized if it was not already. Null outputs are still skipped. The 0x300c code follows the other EGL exports in this file; what the console returns for an unmapped version output is unmeasured.
