# libScePad

## add Unknown function info

- [scePadBondUpdateSendData and scePadControllerUpdateSendData](../../../core/libs/prx/libScePad/Export.cpp) (libScePad) - copy only the leading 64-bit size field through `GuestCopyIn_nid_no_patch` before validating it. Returning `PAD_ERROR_INVALID_ARG` with caller `errno` preserved for unreadable input is a host-safety fallback; console behavior on unmapped guest memory is unmeasured. The driver update operations remain unimplemented.
