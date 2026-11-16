# libSceHttp

## add Unknown function info

- [sceHttpUriSweepPath guest buffers](../../../core/libs/prx/libSceHttp/src/Uri.cpp) (libSceHttp) - the source and the bytes of `dst` the sweep can touch are copied in, swept on the host and copied back with GuestCopyIn_nid_no_patch / GuestCopyOut_nid_no_patch, so bytes the sweep does not write come back unchanged; returning `ERROR_INVALID_VALUE` (its null-pointer error) for an unreadable source or an unwritable `dst` is a host-safety fallback, and console behaviour on unmapped memory is unmeasured.
