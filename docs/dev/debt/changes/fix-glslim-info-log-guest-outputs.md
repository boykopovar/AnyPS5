# libSceGLSlimVSH

## add Functional

- [glGetShaderInfoLog and glGetProgramInfoLog](../../../core/libs/prx/libSceGLSlimVSH/src/Objects.cpp) (libSceGLSlimVSH) write the empty information log and its optional length through `GuestCopyOut_nid_no_patch` instead of dereferencing the guest pointers directly. An unwritable log or length sets `GL_INVALID_VALUE` (0x0501), preserves `errno`, and stops before later outputs are written; when the log write succeeds and the length write fails, the terminator remains written. A zero buffer size does not access the log pointer. These error results are host-safety fallbacks; console behavior for unmapped output pointers is unmeasured.
