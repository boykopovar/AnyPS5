# libSceAmpr

## add Functional

- [sceAmprAprCommandBufferConstructor](../../../core/libs/prx/libSceAmpr/Export.cpp) (libSceAmpr) - returning SCE_KERNEL_ERROR_EFAULT for unwritable buffer pointers via GuestCopyOut_nid_no_patch with caller errno preserved is a host-safety fallback; console behavior on unmapped memory is unmeasured.
