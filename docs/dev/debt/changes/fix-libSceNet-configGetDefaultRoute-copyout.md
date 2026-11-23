# libSceNet

## add Unknown function info

- [sceNetConfigGetDefaultRoute](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - copies out default route gateway address via GuestCopyOut_nid_no_patch with caller errno preserved; returning fail(NET_EFAULT) for an unmapped or unwritable pointer is a host-safety fallback with console behavior unmeasured.
