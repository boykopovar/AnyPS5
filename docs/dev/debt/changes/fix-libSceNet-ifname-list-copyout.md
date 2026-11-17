# libSceNet

## add Functional

- [sceNetGetIfnameNumList](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - returning NET_EFAULT for unwritable interfaceIds pointers via GuestCopyOut_nid_no_patch with caller errno preserved is a host-safety fallback; console behavior on unmapped memory is unmeasured.
