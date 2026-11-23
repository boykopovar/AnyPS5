# libScePad

## add Unknown function info

- [scePadSetTriggerEffect](../../../core/libs/prx/libScePad/Export.cpp) (libScePad) - copies in trigger effect parameters via GuestCopyIn_nid_no_patch with caller errno preserved; returning PAD_ERROR_INVALID_ARG for an unmapped or unreadable pointer is a host-safety fallback with console behavior unmeasured.
