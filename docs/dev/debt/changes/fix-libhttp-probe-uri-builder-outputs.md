# libSceHttp

## add Unknown function info

- [URI builder outputs](../../../core/libs/prx/libSceHttp/src/Uri.cpp) (libSceHttp) - `sceHttpUriBuild`, `sceHttpUriEscape` and `sceHttpUriMerge` write the required size and the output buffer through `GuestCopyOut_nid_no_patch` in the order they wrote them directly; an unwritable output returns `0x804311FE` with `errno` preserved, following `sceHttpsGetSslError` in the same library. The merge's non-opaque form keeps the old `strncpy` zero padding of the relative URI, including the forced terminator at the last byte. Console behavior on unmapped memory is unmeasured.
