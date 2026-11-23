# libSceGLSlimVSH

## change [Pixel upload/download]

- [Pixel upload/download](../../../core/libs/prx/libSceGLSlimVSH/src/Images.cpp) (`Upload`, used by `glTexImage2D` and `glTexSubImage2D`, and `glReadPixels`) copy the guest `pixels` buffer through host buffers. An upload reads the rows with one `GuestCopyIn_nid_no_patch` covering exactly the bytes the rows occupy (no padding after the last row). `glReadPixels` writes the clipped rows with one `GuestCopyOut_nid_no_patch` when they are contiguous and one per row otherwise, so bytes outside the clipped region and the row padding stay untouched. An unreadable or unwritable buffer sets `GL_INVALID_VALUE` (0x0501). A failed upload leaves the texture image unchanged, and a failed per-row read leaves the earlier rows written. The 0x0501 code is a host-safety fallback; what the console does for an unmapped pixel buffer is unmeasured.
