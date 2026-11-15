# libScePad

## add Unknown function info

- [scePadGetActiveTime](../../../core/libs/prx/libScePad/Export.cpp) (libScePad) - clears the 64-bit active time output via GuestCopyOut_nid_no_patch with caller errno preserved before checking the handle; returning PAD_ERROR_INVALID_ARG (0x80920001) for an unwritable output pointer is a host-safety fallback with console behavior unmeasured.
