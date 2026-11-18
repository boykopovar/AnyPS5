# libSceGLSlimVSH

## change [Generate_nid_no_patch]

- [Generate_nid_no_patch](../../../core/libs/prx/libSceGLSlimVSH/src/State.hpp) (`glGenBuffers`, `glGenTextures`, `glGenFramebuffers`, `glGenRenderbuffers`) builds the generated names in a host vector and copies them to the guest `names`/`textures` array with one `GuestCopyOut_nid_no_patch` before it commits them. An unwritable array sets `GL_OUT_OF_MEMORY` (0x0505) and reserves no names. That code is a host-safety fallback; what the console does for an unmapped output array is unmeasured.
