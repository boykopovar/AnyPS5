# libScePad

## add Unknown function info

- [scePadGetDeviceId and scePadControllerUpdateIsUpdateNeeded2](../../../core/libs/prx/libScePad/Export.cpp) (libScePad) - each writes its documented default result to the guest with `GuestCopyOut_nid_no_patch`, preserving `errno` and returning the existing `PAD_ERROR_INVALID_ARG` on an unwritable non-null pointer instead of crashing the host. The write precedes the invalid-handle or null-version check, preserving the existing output and error precedence for valid buffers. The error for an unmapped non-null guest pointer is a host-safety fallback; retail console behavior has not been measured.
