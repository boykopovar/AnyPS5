# libSceGLSlimVSH

## change [eglChooseConfig, eglCreateContext and eglCreatePbufferSurface]

- [eglChooseConfig, eglCreateContext and eglCreatePbufferSurface](../../../core/libs/prx/libSceGLSlimVSH/src/Egl.cpp) read the guest `attributes` list through `GuestCopyIn_nid_no_patch`, in copies of up to 64 words that never cross a 4 KiB boundary, and still read a value only after its key is accepted. A list that starts on or runs into unmapped memory returns `EGL_BAD_PARAMETER` (0x300c) as a host-safety fallback; what the console returns for an unmapped or unterminated attribute list is unmeasured.
