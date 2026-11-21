# libSceHidControl

## add Unknown function info

- [libSceHidControlUnknown_00qes5NswlQ guest buffers](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - the result is cleared and later set with `StoreGuest`, and the 16 request bytes are copied in with `GuestCopyIn_nid_no_patch`, with `errno` preserved; returning `ErrorInvalidArgument` (the null-pointer error) for an unwritable result or unreadable data is a host-safety fallback, and whether the console faults on unmapped memory is unmeasured.
