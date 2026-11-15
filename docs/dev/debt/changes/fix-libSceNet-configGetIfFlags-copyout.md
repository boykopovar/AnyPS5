# libSceNet

## add Unknown function info

- [sceNetConfigGetIfFlags](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - copies out interface flags via GuestCopyOut_nid_no_patch with caller errno preserved; returning NET_CONFIG_ERROR_NULL_FLAGS for an unmapped or unwritable pointer is a host-safety fallback with console behavior unmeasured.
